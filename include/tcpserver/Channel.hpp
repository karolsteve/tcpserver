/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#if !defined(TKS_CHANNEL)
#define TKS_CHANNEL

#include <cstdint>
#include <functional>
#include <sys/epoll.h>

#include "fastlog/not_copyable.hpp"

class EventLoop;

enum class ChannelMark {
    NEW,
    ADDED,
    DELETED
};

// Un canal d'E/S sélectionnable.
// Chaque objet Channel appartient à un seul thread IO, chaque canal
// L'objet est uniquement responsable de la distribution de l'événement IO d'un descripteur de fichier fd du début à la fin,
// ???mais il ne possédera pas le fd, ni ne fermera le fd sur le destructeur???
class Channel : notcopyable {
public:

    Channel(EventLoop *loop, int fd_arg, bool periodic_notification = false);

    [[nodiscard]] int fd() const { return m_fd; }

    [[nodiscard]] uint32_t events() const { return m_events_flag; }

    //Enregistrez-vous pour les événements . Notez la fonction update()
    void enable_reading(){
        m_events_flag |= kReadEvent;
        update();
    }

    void enable_writing(){
        m_events_flag |= kWriteEvent;
        update();
    }

    void disable_write(){
        m_events_flag &= ~kWriteEvent;
        update();
    }

    void disable_all(){
        m_events_flag = kNoneEvent;
        update();
    }

    [[nodiscard]] bool is_none_events() const{
        return m_events_flag == kNoneEvent;
    }


    [[nodiscard]] bool has_write_op() const {
        return m_events_flag & kWriteEvent;
    }

    [[nodiscard]] bool supports_pn() const { return m_with_pn; }

    // for poller
    void mark(ChannelMark mark) { m_mark = mark; }

    ChannelMark mark() { return m_mark; }

    // on_events est le noyau de Channel, il est appelé par EventLoop::loop()
    // Sa fonction est d'appeler différents callbacks utilisateur selon la valeur de events
    void on_events(int64_t receiveTime);

    void on_periodic_notification(int64_t now);

    EventLoop *owner_loop() { return m_loop; }

    void set_read_cb(std::function<void(int64_t)> const &cb) { m_read_cb = cb; }

    void set_write_cb(std::function<void()> const &cb) { m_write_cb = cb; }

    void set_error_cb(std::function<void()> const &cb) { m_error_cb = cb; }

    void set_close_cb(std::function<void()> const &cb) { m_close_cb = cb; }

    void set_periodic_notification_cb(std::function<void(uint64_t)> const &cb) { m_periodic_notification_cb = cb; }

    void set_revents(uint32_t revents) {
        m_revents = revents;
    }

private:
    void update();

    static const uint32_t kReadEvent;
    static const uint32_t kWriteEvent;
    static const uint32_t kNoneEvent;

private:
    EventLoop *m_loop;
    const int m_fd;
    bool m_with_pn;

    uint32_t m_events_flag{0};
    ChannelMark m_mark; // used by Poller

    std::function<void(int64_t)> m_read_cb;
    std::function<void()> m_write_cb;
    std::function<void()> m_error_cb;
    std::function<void()> m_close_cb;
    std::function<void(uint64_t)> m_periodic_notification_cb;
    uint32_t m_revents{0};
};

#endif // TKS_CHANNEL
