//
// Created by Steve Tchatchouang
//

#if !defined(EVENT_LOOP_THREAD)
#define EVENT_LOOP_THREAD

#include <mutex>
#include <thread>
#include <condition_variable>

#include "fastlog/not_copyable.hpp"

class EventLoop;

class EventLoopThread : notcopyable
{
public:
    EventLoopThread();
    ~EventLoopThread();
    EventLoop *startLoop();

private:
    void threadFunc();

    EventLoop *m_loop;
    bool m_exiting;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};

#endif // EVENT_LOOP_THREAD