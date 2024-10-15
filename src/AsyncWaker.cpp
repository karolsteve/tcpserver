/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#include "AsyncWaker.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "fastlog/FastLog.h"

#include <unistd.h>

AsyncWaker::AsyncWaker(EventLoop *loop)
        : m_waker_fd(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)), m_loop(loop),
          m_waker_channel(new Channel(loop, m_waker_fd)) {
    if (m_waker_fd < 0) {
        perror("Async wake Failed in event_fd");
        abort();
    } else {
        DEBUG_D("AsyncWake::instance created:: fd is %d", m_waker_fd);
    }

    m_waker_channel->set_read_cb([this](int64_t time) { handleRead(time); });
    m_waker_channel->enable_reading();
}

AsyncWaker::~AsyncWaker() {
    ::close(m_waker_fd);
    if(m_waker_channel != nullptr){
        delete m_waker_channel;
        m_waker_channel = nullptr;
    }

}

void AsyncWaker::handleRead(int64_t) {
    m_loop->assertInLoopThread();
    uint64_t one = 1;
    ssize_t n = ::read(m_waker_fd, &one, sizeof one);
    if (n != sizeof one) {
        DEBUG_F("AsyncWake::handleRead() reads %lu bytes instead of 8", n);
    }
}

void AsyncWaker::wakeup() const {
    DEBUG_D("WakeUp from async wake");
    uint64_t one = 1;
    ssize_t n = ::write(m_waker_fd, &one, sizeof one);
    if (n != sizeof one) {
        DEBUG_F("AsyncWaker::wakeup() writes %ld bytes instead of 8", n);
    }
}