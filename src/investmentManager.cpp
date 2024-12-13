#include "investmentManager.h"
#include "buffer/equityDataBuffer.h"
#include "buffer/streamDataBuffer.h"
#include "app.h"
#include "utils/logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <shared_mutex>

#ifdef TARGET_LOGGER
#undef TARGET_LOGGER
#endif
#define TARGET_LOGGER m_logger

namespace stockbot {

static const std::filesystem::path DATA_DIR("./stockbot_data");
static const std::filesystem::path FILENAME("investment_manager.json");
static const std::filesystem::path CACHE_PATH = DATA_DIR / FILENAME;

InvestmentManager::InvestmentManager(std::shared_ptr<App> app, std::shared_ptr<spdlog::logger> logger)
    : m_app(app)
    , m_logger(logger)
{
    load();
    LOG_INFO("InvestmentManager initialized.");
}

InvestmentManager::~InvestmentManager()
{
    stop();
    save();
}

void InvestmentManager::addPendingInvestment(AutoInvestment&& investment)
{
    m_pendingRegistration.emplace(investment.id, investment);
}

void InvestmentManager::linkAndRegisterAutoInvestment(const std::string& investmentId, const std::vector<std::string>& accounts)
{
    if (m_pendingRegistration.contains(investmentId)) {
        auto& investment = m_pendingRegistration[investmentId];
        investment.accounts = accounts;
        m_registrationQueue.push(investment);
        // remove pending item
        m_pendingRegistration.erase(investmentId);

        LOG_INFO("Investment id {} linked and sent to registration queue.", investmentId);
    } else {
        LOG_ERROR("Investment id {} not found in pendding map.", investmentId);
    }
}

void InvestmentManager::enqueueStreamData(const std::string& data)
{
    m_streamDataQueue.push(data);
}

void InvestmentManager::run()
{
    m_streamDataBuffer = std::make_unique<StreamDataBuffer>();

    LOG_INFO("Stream data buffer initialized.");

    m_registrationWorker = std::thread(std::bind(&InvestmentManager::processRegistrations, this));

    LOG_INFO("Registration worker started.");

    // 2 workers for now
    m_streamDataWorkerPool.resize(2);
    for (auto& worker : m_streamDataWorkerPool) {
        worker = std::thread(std::bind(&InvestmentManager::processStreamData, this));
    }

    LOG_INFO("Stream data workers started.");
}

void InvestmentManager::stop()
{
    // stop workers
    LOG_INFO("Shutting down registration queue and stopping registration worker...");
    m_registrationQueue.shutdown();

    LOG_INFO("Shutting down stream data queue and stopping stream data workers...");
    m_streamDataQueue.shutdown();

    if (m_registrationWorker.joinable()) {
        m_registrationWorker.join();
    }

    for (auto& worker : m_streamDataWorkerPool) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    // release buffer
    m_streamDataBuffer.reset();
}

void InvestmentManager::save()
{
    // NOTE: safe to call even when workers are working

    // save everything in m_activeInvestments and m_registrationQueue
    if (!std::filesystem::exists(DATA_DIR)) {
        std::filesystem::create_directories(DATA_DIR);
    }
    // overwrite
    std::ofstream file(CACHE_PATH, std::ios_base::trunc);
    if (file.is_open()) {
        // collect all
        std::vector<AutoInvestment> collection;
        {
            // read lock
            std::shared_lock lock(m_mtInvestment);
            for (const auto& [_, investment] : m_activeInvestments) {
                collection.push_back(investment);
            }
        }

        // remaining ones in the queue
        std::queue<AutoInvestment> tmp;
        m_registrationQueue.takeSnapshot(tmp);
        while (!tmp.empty()) {
            collection.push_back(tmp.front());
            tmp.pop();
        }

        if (collection.empty()) {
            file << "[]";
        } else {
            file << json(collection).dump(4);
        }

        LOG_INFO("Investment data cached to {}.", CACHE_PATH.c_str());
    }
}

void InvestmentManager::load()
{
    // load into registration queue
    if (std::filesystem::exists(CACHE_PATH)) {
        std::ifstream file(CACHE_PATH);
        if (file.is_open()) {
            json data;
            file >> data;
            if (data.is_array()) {
                for (const auto& val : data) {
                    m_registrationQueue.push(
                        val.get<AutoInvestment>()
                    );
                }

                LOG_INFO("Investment data loaded from {}.", CACHE_PATH.c_str());
            }
        }
    }
}

void InvestmentManager::processRegistrations()
{
    AutoInvestment investment;
    while (m_registrationQueue.pop(investment)) {
        // add to active
        {
            // write lock
            std::unique_lock lock(m_mtInvestment);
            m_activeInvestments.emplace(investment.ticker, investment);
        }
        // simply subscribing the tickers with the streamer client
        m_app->subscribeTickersToStream({investment.ticker});

        LOG_DEBUG("{} registered.", investment.ticker);
    }
}

void InvestmentManager::processStreamData()
{
    std::string data;
    while (m_streamDataQueue.pop(data)) {
        try {
            json jsonData = json::parse(data);
            // LOG_INFO("\n{}", jsonData.dump(4));

            // only care about "data" field
            if (jsonData.contains("data")) {
                // "data" filed contains a vector of data grouped by the service type
                for (const json& serviceData : jsonData["data"]) {
                    // process contents of
                    // LEVELONE_EQUITIES service
                    // SUBS command
                    // for now, could expand in the future
                    if (serviceData["service"] == "LEVELONE_EQUITIES" &&
                        serviceData["command"] == "SUBS"
                    ) {
                        // "content" field contains a vector of data grouped by the "key"
                        // which is the ticker for level one equities
                        std::string ticker("");
                        std::unordered_map<schwabcpp::StreamerField::LevelOneEquity, double> fields;
                        for (const json& contentData : serviceData["content"]) {
                            // iterate
                            for (auto it = contentData.begin(); it != contentData.end(); ++it) {
                                const auto& key = it.key();
                                const auto& val = it.value();
                                if (key == "delayed" ||
                                    key == "assetMainType" ||
                                    key == "assetSubType" ||
                                    key == "cusip"
                                ) {
                                    // the very first response will contain these in addition to the subscribed fields
                                    // ignoring them for now
                                    continue;
                                } else if (key == "key") {
                                    // the ticker
                                    ticker = val;
                                } else {
                                    // subscribed fields
                                    fields.emplace(schwabcpp::StreamerField::toLevelOneEquityField(key), val.get<double>());
                                }
                            }
                            // add the data into stream buffer
                            // create and register the task
                            std::weak_ptr<EquityDataBuffer> equityBufferRef = m_streamDataBuffer->addLevelOneEquityData(ticker, fields);
                            createAndRegisterTask(ticker, equityBufferRef);
                        }
                    } else {
                        LOG_WARN(
                            "Unsupported service type: {} (command: {})",
                            serviceData["service"].get<std::string>(),
                            serviceData["command"].get<std::string>()
                        );
                    }
                }
            } else {
                LOG_DEBUG("Ignoring empty data...");
            }
        } catch (const json::exception& e) {
            LOG_ERROR("Unable to process stream data: {}.", e.what());
        } catch (...) {
            LOG_ERROR("Unknown error ocurred when processing stream data.");
        }
    }
}

void InvestmentManager::createAndRegisterTask(const std::string& ticker, std::weak_ptr<EquityDataBuffer> equityBufferRef)
{
    std::unique_lock lock(m_mtTaskRecord);
    if (!m_taskRecord.contains(ticker)) {
        m_taskRecord.insert(ticker);
        lock.unlock();

        auto task = [this, ticker, equityBufferRef] {

            // temporarily obtain ownership of the buffer
            if (std::shared_ptr<EquityDataBuffer> buffer = equityBufferRef.lock()) {

                // placeholders for the interested buffer data
                double lastPrice;
                double lod;
                double hod;
                double netPercentChange;

                // thread safe access
                {
                    auto lock = buffer->lockForAccess();

                    lastPrice = buffer->getLastPrice();
                    lod = buffer->getLOD();
                    hod = buffer->getHOD();
                    netPercentChange = buffer->getNetPercentChange();
                }

                // TODO: analyze and signal
                LOG_INFO("{}: last price {:.2f}, lod {:.2f}, hod {:.2f}, net change {:.2f}%", ticker, lastPrice, lod, hod, netPercentChange);
            }

            // remove from record
            {
                std::lock_guard lock(m_mtTaskRecord);
                m_taskRecord.erase(ticker);
            }
        };

        m_app->registerTask(task);
    } else {
        LOG_DEBUG("Task already exists for {}", ticker);
    }
}

}
