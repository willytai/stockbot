#include "investmentManager.h"
#include "utils/logger.h"

#ifdef TARGET_LOGGER
#undef TARGET_LOGGER
#endif
#define TARGET_LOGGER m_logger

namespace stockbot {

InvestmentManager::InvestmentManager(std::shared_ptr<spdlog::logger> logger)
    : m_logger(logger)
{
    LOG_INFO("InvestmentManager initialized.");
}

InvestmentManager::~InvestmentManager()
{
    stop();
}

void InvestmentManager::stop()
{
    NOTIMPLEMENTEDERROR;
}

}
