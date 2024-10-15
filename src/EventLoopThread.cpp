/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#include "EventLoop.hpp"
#include "EventLoopThread.hpp"

EventLoopThread::EventLoopThread() : m_loop(nullptr), m_exiting(false) {}

EventLoopThread::~EventLoopThread()
{
    m_exiting = true;
    m_loop->quit();
    m_thread.join();
}

EventLoop *EventLoopThread::startLoop()
{
    m_thread = std::thread([this] { threadFunc(); });

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this] { return this->m_loop != nullptr; });
    }

    return m_loop;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop{};

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_loop = &loop;
        m_cond.notify_one();
    }

    loop.loop();
}