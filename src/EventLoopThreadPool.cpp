/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#include "EventLoopThreadPool.hpp"
#include "EventLoop.hpp"
#include "EventLoopThread.hpp"
#include <iostream>

#include <cassert>
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop) noexcept
        : m_base_loop(baseLoop), m_started(false), m_num_threads(std::thread::hardware_concurrency()), m_next(0){}

EventLoopThreadPool::~EventLoopThreadPool() = default;

void EventLoopThreadPool::start()
{
    std::cout << "Starting pool with " << m_num_threads << " threads" << "\n";
    assert(!m_started);
    m_base_loop->assertInLoopThread();

    m_started = true;

    for (uint32_t i = 0; i < m_num_threads; ++i)
    {
        auto *t = new EventLoopThread;
        m_threads.emplace_back(t);
        m_loops.push_back(t->startLoop());
    }
}

EventLoop *EventLoopThreadPool::get_next_loop()
{
    m_base_loop->assertInLoopThread();
    EventLoop *loop = m_base_loop;

    if (!m_loops.empty())
    {
        // round-robin
        loop = m_loops[m_next];
        m_next = (m_next + 1) % (m_loops.size());
    }
    return loop;
}