/*
* Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#if !defined(TKS_ACCEPTOR)
#define TKS_ACCEPTOR
#include <string>
#include <memory>
#include <functional>

#include "EventLoop.hpp"
#include "fastlog/not_copyable.hpp"

class EventLoop;
class Channel;
class TcpConnection;

class Acceptor : notcopyable
{
private:
    EventLoop *m_loop;
    int m_listening_port;
    std::unique_ptr<Channel> m_channel;
    bool m_listening{false};
    std::unordered_map<long, std::shared_ptr<TcpConnection>> m_connections;
    std::function<void(const std::shared_ptr<TcpConnection> &, ProtoBuffer *buf, int64_t time)> m_data_received_cb;
    std::function<void(std::shared_ptr<TcpConnection> const &)> m_write_complete_cb;
    std::function<void(const std::shared_ptr<TcpConnection> &)> m_connection_state_change_cb;

    void handleRead(int64_t);

    void on_new_connection(int sock_fd, const std::string &ip, uint16_t port, int family);

    void remove_connection_internal(std::shared_ptr<TcpConnection> const &conn);

public:
    Acceptor(EventLoop *loop, int listen_port, int32_t snd_buff, int32_t rcv_buff);

    ~Acceptor();

    void set_on_data_received(std::function<void(const std::shared_ptr<TcpConnection> &, ProtoBuffer *buf, int64_t time)> const &cb) { m_data_received_cb = cb; }

    void set_on_write_complete(std::function<void(std::shared_ptr<TcpConnection> const &)> const &cb) { m_write_complete_cb = cb; }

    void set_on_connection_state_change(std::function<void(std::shared_ptr<TcpConnection> const &)> const &cb) { m_connection_state_change_cb = cb; }

    void listen();

    [[nodiscard]] bool listening() const { return m_listening; }

    [[nodiscard]] int listen_port() const { return m_listening_port; }
};

#endif // TKS_ACCEPTOR
