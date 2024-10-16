//
// Created by Steve Tchatchouang on 18/05/23.
//

#ifndef TKS_TIMERQUEUE_H
#define TKS_TIMERQUEUE_H

#include <memory>
#include <set>
#include <utility>
#include <vector>
#include <functional>
#include <cstdint>
#include "fastlog/not_copyable.hpp"
#include "Channel.hpp"

class TimerNode {
public:
    typedef std::function<void()> TimerCallback;

    explicit TimerNode(int64_t expiration)
            : expiration_(expiration), interval_(0), repeat_(false) {}

    TimerNode(TimerCallback cb, int64_t expiration, int64_t interval)
            : cb_(std::move(cb)), expiration_(expiration), interval_(interval), repeat_(interval > 0) {}

    void run() const { cb_(); }

    [[nodiscard]] int64_t expiration() const { return expiration_; }

    [[nodiscard]] bool repeat() const { return repeat_; }

    void restart(int64_t now);

    bool operator<(const TimerNode &rhs) const {
        //Const doit être ajouté ici, car les objets constants ne peuvent appeler que des fonctions membres const
        return expiration_ < rhs.expiration_;
    }

    bool operator==(const TimerNode &rhs) const {
        return expiration_ == rhs.expiration_;
    }

private:
    const TimerCallback cb_;
    int64_t expiration_;
    const int64_t interval_;
    const bool repeat_;
};

class TimerQueue : notcopyable {
public:
    typedef std::function<void()> TimerCallback;
    typedef std::multiset<TimerNode> TimerList;

    explicit TimerQueue(EventLoop *loop);

    ~TimerQueue();

    void handleRead();

    void addTimer(const TimerCallback &cb, int64_t when, int64_t interval);

private:
    void addTimerInLoop(TimerNode timernode);

    static int createTimerfd();

    bool insert(TimerNode &timernode);

    void startTimer(TimerNode &timernode) const;

    std::vector<TimerNode> getExpired(TimerNode &sentry);

    void handleExpiredTimer(std::vector<TimerNode> &expired);

private:
    EventLoop *loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    TimerList timers_; //Seules les opérations d'écriture seront effectuées dans le thread loop_, ne verrouillez pas
};


#endif //TKS_TIMERQUEUE_H
