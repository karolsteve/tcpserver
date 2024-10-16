//
// Created by Steve Tchatchouang on 18/05/23.
//

#include <sys/timerfd.h>
#include "TimerQueue.h"
#include "EventLoop.hpp"
#include <unistd.h>
#include "timeutils/TimeUtils.hpp"
#include <cstring>

void TimerNode::restart(int64_t now) {
    if (repeat_) {
        expiration_ = TimeUtils::current_time_in_millis() + interval_; //Timestamp::addTime(now, interval_);
    } else {
        expiration_ = -1;
    }
}

TimerQueue::TimerQueue(EventLoop *loop)
        : loop_(loop), timerfd_(createTimerfd()), timerfdChannel_(loop_, timerfd_) {
    timerfdChannel_.set_read_cb(std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_.enable_reading();
}

TimerQueue::~TimerQueue() {
    close(timerfd_);
}

int TimerQueue::createTimerfd() {
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {

    }
    return timerfd;
}

void TimerQueue::addTimer(const TimerCallback &cb, int64_t when, int64_t interval) {
    TimerNode timernode(cb, when, interval);
    loop_->run(std::bind(&TimerQueue::addTimerInLoop, this, timernode));
}

void TimerQueue::addTimerInLoop(TimerNode timernode) {
    loop_->assertInLoopThread();
    bool earliest = insert(timernode);
    if (earliest) {
        startTimer(timernode);
    }
}

bool TimerQueue::insert(TimerNode &timernode) {
    bool earliest = false;
    int64_t when = timernode.expiration();
    auto it = timers_.begin();
    if (it == timers_.end() || when < it->expiration()) {
        earliest = true;
    }
    timers_.insert(timernode);
    return earliest;
}

void TimerQueue::startTimer(TimerNode &timernode) const {
    struct itimerspec newValue{};
    bzero(&newValue, sizeof(newValue));
    int64_t millis = timernode.expiration() - TimeUtils::current_time_in_millis();
    if (millis < 1000) {
        millis = 1000;
    }
    newValue.it_value.tv_sec = millis / 1000;
    newValue.it_value.tv_nsec = (millis % 1000) * 1000 * 1000;
    int ret = timerfd_settime(timerfd_, 0, &newValue, nullptr);
    if (ret == -1) {

    }
}

void TimerQueue::handleRead() {
    loop_->assertInLoopThread();
    int64_t count;
    size_t n = read(timerfd_, &count, sizeof(count));
    if (n != sizeof(count)) {

    }

    TimerNode sentry(TimeUtils::current_time_in_millis());
    std::vector<TimerNode> expired = getExpired(sentry);
    for (auto &mem: expired) {
        mem.run();
    }
    handleExpiredTimer(expired);
}

std::vector<TimerNode> TimerQueue::getExpired(TimerNode &sentry) {
    std::vector<TimerNode> expired;
    auto it = timers_.lower_bound(sentry);
    std::copy(timers_.begin(), it, back_inserter(expired));
    timers_.erase(timers_.begin(), it);
    return expired;
}

void TimerQueue::handleExpiredTimer(std::vector<TimerNode> &expired) {
    int64_t now(TimeUtils::current_time_in_millis());
    for (auto &mem: expired) {
        if (mem.repeat()) {
            mem.restart(now);
            insert(mem);
        }
    }
    if (!timers_.empty()) {
        TimerNode nextExpire = *timers_.begin();
        if (nextExpire.expiration() != -1) {
            startTimer(nextExpire);
        }
    }
}