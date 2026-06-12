/*
* Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#if !defined(TKS_ACCEPTOR)
#define TKS_ACCEPTOR
#include <string>
#include <functional>

#include "EventLoop.hpp"
#include "fastlog/not_copyable.hpp"

class EventLoop;
class Channel;

class Acceptor : notcopyable
{
private:
    EventLoop *m_loop;
    int m_listening_port;
    std::unique_ptr<Channel> m_channel;
    bool m_listening{false};
    std::function<void(int sockfd, const std::string &, int16_t port, int family, EventLoop* evt_loop)> m_new_connection_callback;

    void handleRead(int64_t) const;

public:
    Acceptor(EventLoop *loop, int listen_port, int32_t snd_buff, int32_t rcv_buff);

    ~Acceptor();

    void set_new_conn_callback(std::function<void(int sockfd, const std::string &, int16_t port, int family, EventLoop* evt_loop)> const &cb) { m_new_connection_callback = cb; }

    void listen();
    [[nodiscard]] bool listening() const { return m_listening; }

    [[nodiscard]] int listen_port() const { return m_listening_port; }
};

#endif // TKS_ACCEPTOR
