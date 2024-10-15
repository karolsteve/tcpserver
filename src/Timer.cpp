//
// Created by Steve Tchatchouang on 04/10/2022.
//

#include "Timer.h"

#include <utility>
#include "EventObject.h"
#include "fastlog/FastLog.h"
#include "EventLoop.hpp"

Timer::Timer(std::function<void()> function, EventLoop *event_loop) : m_event_loop(event_loop), m_callback(std::move(function))
{
    m_event_object = new EventObject(this);
}

Timer::~Timer()
{
    stop();
    if (m_event_object != nullptr)
    {
        delete m_event_object;
        m_event_object = nullptr;
    }
}

void Timer::start()
{
    if (m_started || m_timeout == 0)
    {
        return;
    }
    m_started = true;
    m_event_loop->schedule_event(m_event_object, m_timeout);
}

void Timer::stop()
{
    if (!m_started)
    {
        return;
    }
    m_started = false;
    m_event_loop->remove_event(m_event_object);
}

void Timer::set_timeout(uint32_t ms, bool repeat)
{
    if (ms == m_timeout)
    {
        return;
    }
    m_repeatable = repeat;
    m_timeout = ms;
    if (m_started)
    {
        m_event_loop->remove_event(m_event_object);
        m_event_loop->schedule_event(m_event_object, m_timeout);
    }
}

void Timer::on_event()
{
    m_callback();
    DEBUG_D("m_timer(%p) call", this);
    if (m_started && m_repeatable && m_timeout != 0)
    {
        m_event_loop->schedule_event(m_event_object, m_timeout);
    }
}
