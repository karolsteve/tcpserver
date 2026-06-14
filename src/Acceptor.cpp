/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */
#include "Acceptor.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"
#include "TcpConnection.hpp"
#include <fastlog/FastLog.h>

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <cassert>

static std::atomic_long next_conn_id;

Acceptor::Acceptor(EventLoop* loop, int listen_port, int32_t snd_buff, int32_t rcv_buff) : m_loop(loop), m_listening_port(listen_port)
{
    // Create an AF_INET stream socket to receive incoming connections on
    int m_server_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (m_server_fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "socket()");
    }

    int on{1};
    // Allow socket descriptor to be reusable
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
    {
        ::close(m_server_fd);
        throw std::system_error(errno, std::generic_category(), "setsockopt SO_REUSEADDR");
    }

    if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)))
    {
        ::close(m_server_fd);
        throw std::system_error(errno, std::generic_category(), "setsockopt SO_REUSEPORT");
    }

    // Bind the socket
    sockaddr_in addr{};
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);

    if (bind(m_server_fd, (sockaddr*)&addr, sizeof(addr)) != 0)
    {
        ::close(m_server_fd);
        throw std::system_error(errno, std::generic_category(), "bind()");
    }

    //no delay
    if (setsockopt(m_server_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(int)))
    {
        ::close(m_server_fd);
        throw std::system_error(errno, std::generic_category(), "no delay");
    }

    //snd_buff
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_SNDBUF, &snd_buff, sizeof(int)))
    {
        ::close(m_server_fd);
        throw std::system_error(errno, std::generic_category(), "snd_buff");
    }
    //rcv_buff
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_RCVBUF, &rcv_buff, sizeof(int)))
    {
        ::close(m_server_fd);
        throw std::system_error(errno, std::generic_category(), "rcv_buff()");
    }

    m_channel = std::make_unique<Channel>(loop, m_server_fd);
    m_channel->set_read_cb([this](int64_t time) { handleRead(time); });
}

void Acceptor::listen()
{
    m_loop->assertInLoopThread();
    m_channel->enable_reading();
    m_listening = true;
    // Set the listen backlog
    if (::listen(m_channel->fd(), 65535) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "listen()");
    }
}

void Acceptor::handleRead(int64_t)
{
    m_loop->assertInLoopThread();

    // server socket; call accept as many times as we can
    for (;;)
    {
        sockaddr_in in_addr{};
        socklen_t in_addr_len = sizeof(in_addr);
        int new_client_fd = ::accept4(m_channel->fd(), (sockaddr*)&in_addr, &in_addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (new_client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)//if we processed all the connections
            {
                break;
            }
            std::fprintf(stderr, "[EventLoop] accept4: %s\n", std::strerror(errno));
            continue;
        }

        if (int yes = 1; setsockopt(new_client_fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) != 0)
        {
            perror("Fail to set tcp no delay on client");
        }

        char c_addr[INET6_ADDRSTRLEN];
        inet_ntop(in_addr.sin_family, (void*)&(in_addr.sin_addr), c_addr, INET6_ADDRSTRLEN);

        on_new_connection(new_client_fd, c_addr, ntohs(in_addr.sin_port), in_addr.sin_family);
    }
}

void Acceptor::on_new_connection(int sock_fd, const std::string& ip, uint16_t port, int family)
{
    m_loop->assertInLoopThread();

    auto conn = std::make_shared<TcpConnection>(m_loop, sock_fd, ip, port, family, ++next_conn_id);
    m_connections[conn->conn_id()] = conn;
    DEBUG_D("New connection %s:%d sock_fd : %d family %d id %ld", ip.c_str(), port, sock_fd, family, conn->conn_id());

    conn->set_on_connection_state_change(m_connection_state_change_cb);
    conn->set_on_data_received(m_data_received_cb);
    conn->set_on_write_complete(m_write_complete_cb);
    conn->set_on_connection_closed([this](const auto& _arg) { remove_connection_internal(_arg); });
    m_loop -> queue([conn] {conn->connection_established();});
}

void Acceptor::remove_connection_internal(std::shared_ptr<TcpConnection> const &conn) {
    m_loop->assertInLoopThread();
    DEBUG_D("TcpServer::removeConnection - connection %ld [%s]", conn->conn_id(), conn->ip_addr().c_str());
    const size_t n = m_connections.erase(conn->conn_id());
    assert(n == 1);
    (void) n;
    m_loop->queue([conn] { conn->connection_destroyed(); });
}

Acceptor::~Acceptor()
{
    if (m_channel != nullptr)
    {
        ::close(m_channel->fd());
        m_channel = nullptr;
    }
}
