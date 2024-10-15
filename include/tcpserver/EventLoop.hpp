//
// Created by Steve Tchatchouang
//

#if !defined(EVENT_LOOP)
#define EVENT_LOOP

#include <functional>
#include <thread>
#include <mutex>
#include <list>

#include "fastlog/not_copyable.hpp"
#include "TimerQueue.h"

#define READ_BUFFER_SIZE (2 * 1024 * 1024)

class EventManager;
class Channel;
class AsyncWaker;
class ProtoBuffer;
class EventObject;


class EventLoop : notcopyable
{
private:
    bool m_looping;
    const std::thread::id m_thread_id;
    std::mutex m_mutex;

    // Gère automatiquement les pointeurs vers les EventManagers avec unique_ptr
    // Vous n'avez pas à vous soucier de la destruction de EventManager
    std::unique_ptr<EventManager> m_event_manager;
    bool m_quit;
    std::vector<std::function<void()>> m_run_queue;

    void do_pending_queue();
    bool m_calling_pending_queue;
    std::unique_ptr<AsyncWaker> m_async_waker;
    std::unique_ptr<TimerQueue> m_timerqueue;

    ProtoBuffer *m_network_buffer{nullptr};
    std::list<EventObject *> m_events;
    int call_events(int64_t now);
    void abortNotInLoopThread();
public:
    EventLoop();

    ~EventLoop();

    void loop();

    void quit();

    void wakeup();

    // Si l'utilisateur appelle cette fonction sur le thread IO courant, le callback sera exécuté de manière synchrone ;
    //  Si l'utilisateur appelle runInLoop() sur un autre thread,
    //  to_run sera ajouté à la file d'attente, et le thread IO sera réveillé pour appeler ce Functor
    void run(std::function<void()> const &to_run);

    // mettre le to_run dans la file d'attente et réveiller le thread IO si nécessaire
    void queue(std::function<void()> const &to_run);

    void assertInLoopThread()
    {
        if (!isInLoopThread())
        {
            abortNotInLoopThread();
        }
    }

    ProtoBuffer *network_buffer();

    // Déterminez s'il se trouve dans le fil de la boucle
    [[nodiscard]] bool isInLoopThread() const
    {
        return m_thread_id == std::this_thread::get_id();
    }

    void updateChannel(Channel *channel);

    void remove_channel(Channel *channel);

    void schedule_event(EventObject *, uint32_t timeout);

    void remove_event(EventObject *);

    void runAt(const int64_t &time, std::function<void()> const &cb);

    void runAfter(int64_t delay, std::function<void()> const &cb);

    void runEvery(int64_t interval, std::function<void()> const &cb);
};

#endif // EVENT_LOOP
