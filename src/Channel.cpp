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

Channel::Channel(EventLoop *loop, int fd_arg, bool pn) : m_loop(loop), m_fd(fd_arg), m_with_pn(pn), m_mark(ChannelMark::NEW)
{
}

void Channel::update()
{
    m_loop->updateChannel(this); // just to reach event manager
}

void Channel::on_events(int64_t receiveTime)
{
    uint32_t revents = m_revents;
    if((revents & EPOLLHUP) && !(revents & EPOLLIN)){
        if (m_close_cb) {
            m_close_cb();
        }
    }

    if(revents & (EPOLLERR)){
        if (m_error_cb) {
            std::cerr << "ERROR FROM events \n";
            m_error_cb();
        }
    }

    if(revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)){
        if (m_read_cb) {
            m_read_cb(receiveTime);
        }
    }

    if(revents & EPOLLOUT){
        if (m_write_cb) {
            m_write_cb();
        }
    }
}

void Channel::on_periodic_notification(int64_t now)
{
    if (m_periodic_notification_cb)
    {
        m_periodic_notification_cb(now);
    }
}