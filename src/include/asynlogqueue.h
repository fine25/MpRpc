#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

// 异步日志队列
template <typename T>
class AsynLogQueue
{
public:
    void Push(const T &data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(data);
        m_condvariable.notify_one();
    }

    T Pop()
    {
        std::unique_lock < std::mutex>lock(m_mutex);
        // 防止虚假唤醒
        while (m_queue.empty())
        {
            // 日志队列为空，进入wait状态
            m_condvariable.wait(lock);
        }
        T data = m_queue.front();
        m_queue.pop();
        return data;
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condvariable;
};