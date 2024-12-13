#include "taskManager.h"
#include "utils/logger.h"

#ifdef TARGET_LOGGER
#undef TARGET_LOGGER
#endif
#define TARGET_LOGGER m_logger

namespace stockbot {

TaskManager::TaskManager(int poolSize,
                         std::shared_ptr<spdlog::logger> logger)
    : m_logger(logger)
{
    m_threadPool.resize(poolSize);
}

TaskManager::~TaskManager()
{
    m_taskQueue.shutdown();

    for (auto& worker : m_threadPool) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void TaskManager::run()
{
    for (auto& worker : m_threadPool) {
        LOG_DEBUG("Launching worker...");
        worker = std::thread(
            [this]{
                Task task;
                while (m_taskQueue.pop(task)) {
                    task();
                }

                LOG_DEBUG("Worker terminated.");
            }
        );
    }
}

void TaskManager::addTask(Task task)
{
    m_taskQueue.push(task);
}

}
