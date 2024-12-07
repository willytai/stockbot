#include "investmentManager.h"
#include "utils/logger.h"
#include <filesystem>
#include <fstream>

#ifdef TARGET_LOGGER
#undef TARGET_LOGGER
#endif
#define TARGET_LOGGER m_logger

namespace stockbot {

static const std::filesystem::path DATA_DIR("./stockbot_data");
static const std::filesystem::path FILENAME("investment_manager.json");
static const std::filesystem::path CACHE_PATH = DATA_DIR / FILENAME;

InvestmentManager::InvestmentManager(std::shared_ptr<spdlog::logger> logger)
    : m_logger(logger)
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
        {
            // critical section
            std::lock_guard lock(m_mutexQ);
            m_registrationQueue.push(investment);
        }
        // remove pending item
        m_pendingRegistration.erase(investmentId);

        LOG_INFO("Investment id {} linked and sent to registration queue.", investmentId);
    } else {
        LOG_ERROR("Investment id {} not found in pendding map.", investmentId);
    }
}

void InvestmentManager::run()
{
    NOTIMPLEMENTEDERROR;
}

void InvestmentManager::stop()
{
    // NOTE: stop all workers
    NOTIMPLEMENTEDERROR;
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
            std::lock_guard lock(m_mutexV);
            collection = m_activeInvestments;
        }
        std::queue<AutoInvestment> tmp;
        {
            std::lock_guard lock(m_mutexQ);
            tmp = m_registrationQueue;
        }
        while (!tmp.empty()) {
            collection.push_back(tmp.front());
            tmp.pop();
        }

        file << json(collection).dump(4);

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

}
