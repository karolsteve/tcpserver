/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#include "EventLoop.hpp"
#include "EventManager.hpp"
#include "Channel.hpp"
#include "AsyncWaker.hpp"
#include "buffer/ProtoBuffer.h"
#include "EventObject.h"
#include "timeutils/TimeUtils.hpp"

#include <iostream>
#include <cassert>
#include <csignal>

class IgnoreSigpipe
{
public:
    IgnoreSigpipe(){
        ::signal(SIGPIPE, SIG_IGN);
    }

};

//Utilisez __thread pour vous assurer qu'un thread ne peut créer qu'une seule EventLoop
// La variable __thread est une entité distincte pour chaque thread
// similaire à statique, uniquement initialisé au moment de la compilation
__thread EventLoop *t_loopInThisThread = nullptr;

EventLoop::EventLoop() : m_looping(false), m_thread_id(std::this_thread::get_id()),
                         m_event_manager(new EventManager(this)), m_quit(false),
                         m_calling_pending_queue(false), m_async_waker(new AsyncWaker(this)),
                         m_timerqueue(new TimerQueue(this)){

    DEBUG_D("EventLoop created");

    if (t_loopInThisThread) {
        DEBUG_F("Another EventLoop exists in this thread");
        abortNotInLoopThread();
    } else {
        t_loopInThisThread = this;
    }
}

void EventLoop::loop() {
    assert(!m_looping);
    assertInLoopThread();

    m_looping = true;
    m_quit = false;

    std::vector<Channel *> channels{};

    while (!m_quit) {
        try {
            channels.clear();

            // Ici epoll est en polling, il est bloqué, si vous souhaitez rappeler des événements actifs, vous devez trouver un moyen de le réveiller
            // on utilise ici une méthode très astucieuse, en particulier en utilisant un descripteur de fichier pour se réveiller

            int64_t time = m_event_manager->epoll(call_events(TimeUtils::current_time_in_millis()), &channels);

            call_events(time);

            for (auto const &it: channels) {
                it->on_events(time);
            }

            do_pending_queue();
            m_event_manager->check_periodic_observers();
        }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
        }
    }
    m_looping = false;
}

//L'événement est enregistré dans le canal et doit être mis à jour, mais
// Pour assurer la sécurité des threads, le canal lui-même ne sera pas mis à jour,
// Transférer le travail de mise à jour dans la boucle,
// puis synchroniser le canal dans le sondage dans le thread de la boucle
void EventLoop::updateChannel(Channel *channel) {
    assertInLoopThread();
    m_event_manager->updateChannel(channel);
}

void EventLoop::remove_channel(Channel *channel) {
    assert(channel->owner_loop() == this);
    assertInLoopThread();
    m_event_manager->remove_channel(channel);
}

void EventLoop::quit() {
    m_quit = true;
    if (!isInLoopThread()) {
        m_async_waker->wakeup();
    }
}

void EventLoop::abortNotInLoopThread() {
    std::cerr << "LOG_FATAL:   "
              << "EventLoop::abortNotInLoopThread - EventLoop " << this
              << " was created in threadId_ = " << m_thread_id
              << ", current thread id = " << std::this_thread::get_id()
              << "\n";
    std::abort();
}

void EventLoop::run(std::function<void()> const &to_run) {
    if (isInLoopThread()) {
        to_run();
    } else {
        queue(to_run);
    }
}

//queue est public, il ne doit pas seulement être appelé par run,
// il peut aussi être appelé directement
void EventLoop::queue(std::function<void()> const &to_run) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_run_queue.push_back(to_run);
    }

    if (!isInLoopThread() || m_calling_pending_queue) {
        m_async_waker->wakeup();
    }
}

void EventLoop::do_pending_queue() {
    std::vector<std::function<void()>> functors;
    m_calling_pending_queue = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        functors.swap(m_run_queue);
    }

    for (auto &functor: functors) {
        functor();
    }

    m_calling_pending_queue = false;
}

ProtoBuffer *EventLoop::network_buffer() {
    if (m_network_buffer == nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_network_buffer == nullptr) {
            m_network_buffer = new ProtoBuffer((uint32_t) READ_BUFFER_SIZE);
        }
    }
    return m_network_buffer;
}

void EventLoop::schedule_event(EventObject *eventObject, uint32_t time) {
    eventObject->time(TimeUtils::current_time_in_millis() + time);
    std::list<EventObject *>::iterator iter;
    for (iter = m_events.begin(); iter != m_events.end(); iter++) {
        if ((*iter)->time() > eventObject->time()) {
            break;
        }
    }
    m_events.insert(iter, eventObject);
}

void EventLoop::remove_event(EventObject *eventObject) {
    for (auto iter = m_events.begin(); iter != m_events.end(); iter++) {
        if (*iter == eventObject) {
            m_events.erase(iter);
            break;
        }
    }
}

int EventLoop::call_events(int64_t now) {
    if (!m_events.empty()) {
        for (auto iter = m_events.begin(); iter != m_events.end();) {
            EventObject *eventObject = (*iter);
            if (eventObject->time() <= now) {
                iter = m_events.erase(iter);
                eventObject->on_event();
            } else {
                int diff = (int) (eventObject->time() - now);
                return diff > 1000 ? 1000 : diff;
            }
        }
    }
    return 1000;
}

//new
void EventLoop::runAt(const int64_t &time, std::function<void()> const &cb)
{
    m_timerqueue->addTimer(cb, time, 0);
}

void EventLoop::runAfter(int64_t delay, const std::function<void()> &cb)
{
    int64_t time(TimeUtils::current_time_in_millis() + delay);
    runAt(time, cb);
}

void EventLoop::runEvery(int64_t interval, std::function<void()> const &cb)
{
    int64_t time(TimeUtils::current_time_in_millis() + interval);
    m_timerqueue->addTimer(cb, time, interval);
}


EventLoop::~EventLoop() {
    assert(!m_looping);
    t_loopInThisThread = nullptr;
    if (m_network_buffer != nullptr) {
        delete m_network_buffer;
        m_network_buffer = nullptr;
    }
}

void EventLoop::wakeup() {
    m_async_waker->wakeup();
}
