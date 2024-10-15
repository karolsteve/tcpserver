//
// Created by Steve Tchatchouang on 04/10/2022.
//

#ifndef TKS_TIMER_H
#define TKS_TIMER_H

#include <cstdint>
#include <functional>

class EventObject;
class EventLoop;

class Timer {
public:
    Timer(std::function<void()> function, EventLoop* event_loop);
    ~Timer();

    void start();
    void stop();
    void set_timeout(uint32_t ms, bool repeat);

private:
    void on_event();
    EventLoop *m_event_loop;
    std::function<void()> m_callback;
    bool m_started{false};
    bool m_repeatable{false};
    uint32_t m_timeout{0};
    EventObject *m_event_object;

    friend class EventObject;
};
#endif //TKS_TIMER_H
