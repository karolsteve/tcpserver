/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */
#include "Acceptor.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"
#include <fastlog/FastLog.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

// TCP_FASTOPEN is defined in linux 3.7. We define this here so older kernels can compile.
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN 23
#endif

static bool set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl()");
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl()");
        return false;
    }
    return true;
}

Acceptor::Acceptor(EventLoop *loop, int listen_port, int32_t snd_buff, int32_t rcv_buff, int32_t keep_alive,
                   int32_t backlog, bool with_linger) : m_loop(loop), m_listening(false), m_listening_port(listen_port)
                   , m_keep_alive(keep_alive), m_backlog(backlog), m_with_linger(with_linger)
{
    // Create an AF_INET stream socket to receive incoming connections on
    int m_server_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_server_fd < 0)
    {
        perror("Failed to launch server");
        throw std::runtime_error("Can not initialize socket. check console");
    }

    int on{1};
    // Allow socket descriptor to be reuseable
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        perror("Fail to set reuse addr in server sock");
        close(m_server_fd);
        throw std::runtime_error("Fail to set reuse opt on socket");
    }

    // Bind the socket
    sockaddr_in addr{};
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);

    if (bind(m_server_fd, (sockaddr *)&addr, sizeof(addr)) != 0)
    {
        perror("Fail to bind socket to port");
        close(m_server_fd);
        throw std::runtime_error("Fail to bind server to addr");
    }

    // non blocking
    set_nonblocking(m_server_fd);
    //no delay
    if (setsockopt(m_server_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(int)))
    {
        perror("Fail to set no delay");
        close(m_server_fd);
        throw std::runtime_error("Fail to set no delay");
    }

    //snd_buff
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_SNDBUF, &snd_buff, sizeof(int)))
    {
        perror("Fail to set snd buff");
        close(m_server_fd);
        throw std::runtime_error("Fail to set snd buff");
    }
    //rcv_buff
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_RCVBUF, &rcv_buff, sizeof(int)))
    {
        perror("Fail to set rcv buff");
        close(m_server_fd);
        throw std::runtime_error("Fail to set rcv buff");
    }

    m_channel = new Channel(loop, m_server_fd);
    m_channel->set_read_cb([this](int64_t time) { handleRead(time); });
}

void Acceptor::listen()
{
    m_loop->assertInLoopThread();
    m_channel->enable_reading();
    m_listening = true;
    // Set the listen backlog
    if (::listen(m_channel->fd(), m_backlog > SOMAXCONN ? SOMAXCONN : m_backlog) != 0)
    {
        perror("Failed to listen");
        close(m_channel->fd());
        throw std::runtime_error("Fail to listen");
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
        int new_client_fd = ::accept(m_channel->fd(), (sockaddr *)&in_addr, &in_addr_len);
        if (new_client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // we processed all the connections
                break;
            }
            else
            {
                perror("Error on accept");
                continue;
            }
        }
        else
        {
            set_nonblocking(new_client_fd);
            int yes = 1;
            if (setsockopt(new_client_fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)))
            {
                perror("Fail to set tcp no delay on client");
            }

            if (setsockopt(new_client_fd, SOL_SOCKET, SO_KEEPALIVE, &m_keep_alive, sizeof(int)))
            {
                perror("Fail to set tcp keep alive on client");
            }

            if(m_with_linger){
                linger opt{};
                opt.l_onoff = 1;
                opt.l_linger = 0;
                if (setsockopt(new_client_fd, SOL_SOCKET, SO_LINGER, &opt, sizeof(opt)))
                {
                    perror("Fail to set so_linger on client");
                }
            }

            char c_addr[INET6_ADDRSTRLEN];
            inet_ntop(in_addr.sin_family, (void *)&(in_addr.sin_addr), c_addr, INET6_ADDRSTRLEN);

            m_new_connection_callback(new_client_fd, c_addr, ntohs(in_addr.sin_port), in_addr.sin_family);
        }
    }
}

Acceptor::~Acceptor()
{
    if (m_channel != nullptr)
    {
        ::close(m_channel->fd());
        delete m_channel;
        m_channel = nullptr;
    }
}