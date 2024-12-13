#ifndef __TASK_MANAGER_H__
#define __TASK_MANAGER_H__

#include "spdlog/logger.h"
#include <utils/concurrentQueue.h>
#include <vector>
#include <thread>
#include <functional>

namespace stockbot {

class TaskManager
{
    using Task = std::function<void()>;
public:
                                TaskManager(
                                    int poolSize,
                                    std::shared_ptr<spdlog::logger> logger
                                );
                                ~TaskManager();

    void                        run();
    void                        addTask(Task task);

private:
    ConcurrentQueue<Task>       m_taskQueue;
    std::vector<std::thread>    m_threadPool;

    std::shared_ptr<spdlog::logger>     m_logger;
};

}

#endif
