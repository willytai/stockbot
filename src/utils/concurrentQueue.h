#ifndef __CONCURRENT_QUEUE_H__
#define __CONCURRENT_QUEUE_H__

#include <condition_variable>
#include <mutex>
#include <queue>

namespace stockbot {

template <typename T>
class ConcurrentQueue
{
public:
    void                    push(const T& data)
                            {
                                {
                                    std::lock_guard lock(m_mutex);
                                    m_queue.push(data);
                                }
                                m_cv.notify_one();
                            }

    bool                    pop(T& data)
                            {
                                std::unique_lock lock(m_mutex);
                                m_cv.wait(lock, [this] { return !m_shouldRun || !m_queue.empty(); });

                                // discarding all objects when signaled to exit
                                bool result = false;
                                if (m_shouldRun && !m_queue.empty()) {
                                    data = m_queue.front();
                                    m_queue.pop();
                                    result = true;
                                }

                                return result;
                            }

    void                    shutdown()
                            {
                                {
                                    std::lock_guard lock(m_mutex);
                                    m_shouldRun = false;
                                }
                                m_cv.notify_all();
                            }

    void                    takeSnapshot(std::queue<T>& data)
                            {
                                std::lock_guard lock(m_mutex);
                                data = m_queue;
                            }

private:
    std::queue<T>           m_queue;
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    bool                    m_shouldRun = true;

};

} // namespace stockbot

#endif
