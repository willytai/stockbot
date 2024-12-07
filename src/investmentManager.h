#ifndef __INVESTMENT_MANAGER_H__
#define __INVESTMENT_MANAGER_H__

#include "autoInvestment.h"
#include "spdlog/logger.h"
#include <queue>

namespace stockbot {

class InvestmentManager
{
public:
                                        InvestmentManager(
                                            std::shared_ptr<spdlog::logger> logger
                                        );
                                        ~InvestmentManager();

    void                                run();

    void                                addPendingInvestment(AutoInvestment&& investment);
    void                                linkAndRegisterAutoInvestment(const std::string& investmentId, const std::vector<std::string>& accounts);

private:
    void                                stop();

    void                                save();
    void                                load();

private:
    std::vector<AutoInvestment>         m_activeInvestments;    // thread safe
    std::queue<AutoInvestment>          m_registrationQueue;    // thread safe
    std::mutex                          m_mutexV;
    std::mutex                          m_mutexQ;

    std::unordered_map<std::string, AutoInvestment>
                                        m_pendingRegistration;  // not thread safe, should be access from only one thread

    std::shared_ptr<spdlog::logger>     m_logger;
};

} // namespace stockbot

#endif
