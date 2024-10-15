/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#include "EventManager.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "timeutils/TimeUtils.hpp"
#include "fastlog/FastLog.h"

#include <cassert>
#include <sys/epoll.h>
#include <unistd.h>
#include <memory.h>

#define INIT_EVENTS_SIZE 16
#define MAX_EVENTS_SIZE 4096

EventManager::EventManager(EventLoop *owner) : m_owner_loop(owner), m_epoll_fd(epoll_create1(EPOLL_CLOEXEC)),
                                               m_event_list(INIT_EVENTS_SIZE) {
    if (m_epoll_fd < 0) {
        perror("Fail to launch epoll");
        exit(-1);
    }
}

EventManager::~EventManager() { ::close(m_epoll_fd); }

int64_t EventManager::epoll(int timeout_ms, std::vector<Channel *> *channels) {
    auto max_events = (int32_t) m_event_list.size();
    int32_t num_events = ::epoll_wait(m_epoll_fd, m_event_list.data(), max_events, timeout_ms);
    int64_t now = TimeUtils::current_time_in_millis();
    if (num_events > 0) {
        DEBUG_D("%d events happened", num_events);

        for (int i = 0; i < num_events; ++i) {
            Channel *channel = m_channels.find(m_event_list[i].data.fd)->second;
            channel->set_revents(m_event_list[i].events);
            channels->push_back(channel);
        }

        if (num_events == max_events) {
            if (max_events < MAX_EVENTS_SIZE) {
                DEBUG_W("RESIZING m_EVENT LIST from %d to %d", max_events, max_events * 2);
                m_event_list.resize(max_events * 2);
            } else {
                DEBUG_F("SKIP RESIZE. CURRENT SIZE %d is gte than %d", max_events, MAX_EVENTS_SIZE);
            }
        }
    } else if(num_events == -1){
        DEBUG_F("Epoll wait FAILED %s", strerror(errno));
    }

    return now;
}

void EventManager::check_periodic_observers() {
    if (m_periodic_notification_observers.empty()) {
        return;
    }

    auto now = TimeUtils::current_time_in_millis();
    for (auto const &item: m_periodic_notification_observers) {
        item.second->on_periodic_notification(now);
    }
}

void EventManager::updateChannel(Channel *channel) {
    m_owner_loop->assertInLoopThread();

    const ChannelMark mark = channel->mark();

    if (mark == ChannelMark::NEW || mark == ChannelMark::DELETED) {
        int fd = channel->fd();

        //add new fd with EPOLL_CTL_ADD
        if (mark == ChannelMark::NEW) {
            assert(m_channels.find(fd) == m_channels.end());
            m_channels[fd] = channel;
            // pn
            if (channel->supports_pn()) {
                assert(m_periodic_notification_observers.find(fd) == m_periodic_notification_observers.end());
                m_periodic_notification_observers[fd] = channel;
            }
        } else // index == kDeleted
        {
            assert(m_channels.find(fd) != m_channels.end());
            assert(m_channels[fd] == channel);

            // pn
            if (channel->supports_pn()) {
                assert(m_periodic_notification_observers.find(fd) != m_periodic_notification_observers.end());
                assert(m_periodic_notification_observers[fd] == channel);
            }
        }

        channel->mark(ChannelMark::ADDED);
        apply_ops(EPOLL_CTL_ADD, channel);
    } else {
        //EPOLL_CTL_MOD/DEL update current fd
        int fd = channel->fd();
        (void) fd;
        assert(m_channels.find(fd) != m_channels.end());
        assert(m_channels[fd] == channel);
        assert(mark == ChannelMark::ADDED);
        if (channel->is_none_events()) {
            apply_ops(EPOLL_CTL_DEL, channel);
            channel->mark(ChannelMark::DELETED);
        } else {
            apply_ops(EPOLL_CTL_MOD, channel);
        }
    }
}

void EventManager::remove_channel(Channel *channel) {
    m_owner_loop->assertInLoopThread();
    int fd = channel->fd();

    assert(m_channels.find(fd) != m_channels.end());
    assert(m_channels[fd] == channel);
    assert(channel->is_none_events());

    ChannelMark mark = channel->mark();
    assert(mark == ChannelMark::ADDED || mark == ChannelMark::DELETED);
    size_t n = m_channels.erase(fd);
    (void) n;
    assert(n == 1);

    if (channel->supports_pn()) {
        n = m_periodic_notification_observers.erase(fd);
        (void) n;
        assert(n == 1);
    }

    if (mark == ChannelMark::ADDED) {
        apply_ops(EPOLL_CTL_DEL, channel);
    }
    channel->mark(ChannelMark::DELETED);
}

void EventManager::apply_ops(int operation, Channel *channel) const {
    assert(operation == EPOLL_CTL_ADD || operation == EPOLL_CTL_MOD || operation == EPOLL_CTL_DEL);
    struct epoll_event ev{};
    ::memset(&ev, 0, sizeof(ev));
    ev.events = channel->events() | EPOLLET;
    ev.data.fd = channel->fd();
    int fd = channel->fd();

    DEBUG_D("Epoll ctl op %d, fd %d. events %d", operation, fd, channel->events());

    if (::epoll_ctl(m_epoll_fd, operation, fd, &ev) == 0) {
        return;
    }

    switch (operation) {
        case EPOLL_CTL_MOD:
            if (errno == ENOENT)//fd closed and reopened
            {
                if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                    DEBUG_F("EPOLL ADD AGAIN FAILED");
                    return;
                }
                return;
            }
            break;
        case EPOLL_CTL_ADD:
            if (errno == EEXIST)//already exist
            {
                if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
                    DEBUG_F("EPOLL MOD AGAIN FAILED");
                    return;
                }
                return;
            }
            break;
        case EPOLL_CTL_DEL:
            if (errno == ENOENT || errno == EBADF || errno == EPERM) {
                DEBUG_F("EPOLL DEL FAILED");
                return;
            }
            break;
        default:
            break;
    }
    perror("Epoll ops failed");
    DEBUG_F("Epoll ops failed  %s", strerror(errno));
}