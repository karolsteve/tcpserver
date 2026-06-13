//
// Created by Steve Tchatchouang
//

#if !defined(EVENT_LOOP)
#define EVENT_LOOP

#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <list>

#include "fastlog/not_copyable.hpp"

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

    std::unique_ptr<EventManager> m_event_manager;
    std::atomic<bool> m_quit{false};
    std::vector<std::function<void()>> m_run_queue;

    void do_pending_queue();
    std::atomic<bool> m_calling_pending_queue{false};
    std::unique_ptr<AsyncWaker> m_async_waker;

    ProtoBuffer *m_network_buffer{nullptr};
    std::list<EventObject *> m_events;
    int call_events(int64_t now);
    void abortNotInLoopThread() const;
public:
    EventLoop();

    ~EventLoop();

    void loop();

    void quit();

    void wakeup() const;

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

    void remove_event(const EventObject *);
};

#endif // EVENT_LOOP
