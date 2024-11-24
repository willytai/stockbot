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

    void                                registerInvestment(const AutoInvestment& investment);

private:
    void                                stop();

    void                                save();
    void                                load();

private:
    std::vector<AutoInvestment>         m_investments;
    std::queue<AutoInvestment>          m_pendingRegistreation;

    std::shared_ptr<spdlog::logger>     m_logger;
};

} // namespace stockbot

#endif
