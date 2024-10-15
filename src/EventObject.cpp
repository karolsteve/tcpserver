//
// Created by Steve Tchatchouang on 04/10/2022.
//

#include "EventObject.h"
#include "Timer.h"

EventObject::EventObject(void *object) : m_event_object(object)
{
}

void EventObject::on_event()
{
    auto *timer = (Timer *)m_event_object;
    timer->on_event();
}

int64_t EventObject::time() const
{
    return m_tme;
}

void EventObject::time(int64_t time)
{
    m_tme = time;
}