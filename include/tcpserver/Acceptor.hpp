/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#if !defined(TKS_ACCEPTOR)
#define TKS_ACCEPTOR
#include <string>
#include <functional>
#include "fastlog/not_copyable.hpp"

class EventLoop;
class Channel;

class Acceptor : notcopyable
{
private:
    EventLoop *m_loop;
    bool m_listening;
    int m_listening_port;
    int32_t m_keep_alive;
    int32_t m_backlog;
    bool m_with_linger;

    Channel *m_channel;
    std::function<void(int sockfd, const std::string &, int16_t port, int family)> m_new_connection_callback;

    void handleRead(int64_t);

public:
    Acceptor(EventLoop *loop, int listen_port, int32_t snd_buff, int32_t rcv_buff, int32_t keep_alive, int32_t backlog,
             bool with_linger);
    ~Acceptor();

    void set_new_conn_callback(std::function<void(int sockfd, const std::string &, int16_t port, int family)> const &cb) { m_new_connection_callback = cb; }

    void listen();
    [[nodiscard]] bool listening() const { return m_listening; }

    [[nodiscard]] int listen_port() const { return m_listening_port; }
};

#endif // TKS_ACCEPTOR
