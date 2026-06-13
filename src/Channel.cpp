/*
* Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#include "Channel.hpp"
#include "EventLoop.hpp"
#include <iostream>

const uint32_t Channel::kReadEvent = EPOLLIN | EPOLLPRI | EPOLLRDHUP;
const uint32_t Channel::kWriteEvent = EPOLLOUT;
const uint32_t Channel::kNoneEvent = 0;

Channel::Channel(EventLoop *loop, const int fd_arg, const bool periodic_notification) :
    m_loop(loop), m_fd(fd_arg), m_with_pn(periodic_notification), m_mark(ChannelMark::NEW)
{
}

void Channel::update()
{
    m_loop->updateChannel(this); // just to reach event manager
}

void Channel::on_events(const int64_t receiveTime) const
{
    const uint32_t r_events = m_revents;
    if (r_events & (EPOLLRDHUP | EPOLLHUP)) {
        if (m_close_cb) m_close_cb();
        return;
    }

    if(r_events & EPOLLERR){
        std::cerr << "ERROR FROM events \n";
        if (m_error_cb) m_error_cb();
        return;
    }

    if(r_events & (EPOLLIN | EPOLLPRI))
    {
        if (m_read_cb) m_read_cb(receiveTime);
    }
    if(r_events & EPOLLOUT)
    {
        if (m_write_cb) m_write_cb();
    }
}

void Channel::on_periodic_notification(const int64_t now) const
{
    if (m_periodic_notification_cb)
    {
        m_periodic_notification_cb(now);
    }
}
