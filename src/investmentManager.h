#ifndef __INVESTMENT_MANAGER_H__
#define __INVESTMENT_MANAGER_H__

#include "autoInvestment.h"
#include "spdlog/logger.h"
#include "utils/concurrentQueue.h"
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace stockbot {

class App;
class StreamDataBuffer;
class EquityDataBuffer;

class InvestmentManager
{
public:
                                        InvestmentManager(
                                            std::shared_ptr<App> app,
                                            std::shared_ptr<spdlog::logger> logger
                                        );
                                        ~InvestmentManager();

    void                                run();

    void                                addPendingInvestment(AutoInvestment&& investment);
    void                                linkAndRegisterAutoInvestment(const std::string& investmentId, const std::vector<std::string>& accounts);

    void                                enqueueStreamData(const std::string& data);

private:
    void                                stop();

    void                                save();
    void                                load();

    void                                processRegistrations();
    void                                processStreamData();

    void                                createAndRegisterTask(const std::string& ticker, std::weak_ptr<EquityDataBuffer> equityBufferRef);

private:
    // -- active investment container
    std::unordered_multimap<std::string, AutoInvestment>
                                        m_activeInvestments;    // thread safe
    std::shared_mutex                   m_mtInvestment;

    // -- task registration management
    std::unordered_set<std::string>     m_taskRecord;
    std::mutex                          m_mtTaskRecord;

    // -- registration pipeline
    ConcurrentQueue<AutoInvestment>     m_registrationQueue;
    std::thread                         m_registrationWorker;
    std::unordered_map<std::string, AutoInvestment>
                                        m_pendingRegistration;  // not thread safe, should be accessed from only one thread

    // -- stream data processing pipeline
    ConcurrentQueue<std::string>        m_streamDataQueue;
    std::vector<std::thread>            m_streamDataWorkerPool;

    // -- buffer that holds the processed stream data
    std::unique_ptr<StreamDataBuffer>   m_streamDataBuffer;

    std::shared_ptr<App>                m_app;

    std::shared_ptr<spdlog::logger>     m_logger;
};

} // namespace stockbot

#endif
