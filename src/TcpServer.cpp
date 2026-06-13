/*
* Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#include "TcpServer.hpp"
#include "EventLoop.hpp"
#include "EventLoopThreadPool.hpp"
#include "Acceptor.hpp"
#include <cassert>
#include <utility>
#include "buffer/ProtoBuffer.h"

TcpServer::TcpServer(EventLoop *loop, const uint16_t listen_port, std::string name, int server_id, int32_t snd_buff, int32_t rcv_buff, uint32_t num_threads)
        : m_loop(loop), m_listen_port(listen_port), m_started(false),
            m_thread_pool(std::make_unique<EventLoopThreadPool>(loop)), m_name(std::move(name)),
            m_server_id(server_id), m_snd_buff(snd_buff), m_rcv_buff(rcv_buff) {

    m_thread_pool->set_pool_size(num_threads);
}

int TcpServer::listen_port() const
{
    return m_listen_port;
}

uint32_t TcpServer::pool_size() const {
    return m_thread_pool->pool_size();
}

void TcpServer::start() {
    if (!m_started) {
        m_started = true;
        m_thread_pool->start();
    }

    assert(pool_size() > 0);

    for (int i = 0; i < m_thread_pool->pool_size(); ++i)
    {
        EventLoop *event_loop = m_thread_pool->get_next_loop();
        auto acceptor = std::make_unique<Acceptor>(event_loop, m_listen_port, m_snd_buff, m_rcv_buff);
        acceptor->set_on_connection_state_change(m_connection_state_change_cb);
        acceptor->set_on_data_received(m_data_received_cb);
        acceptor->set_on_write_complete(m_write_complete_cb);

        auto * a = acceptor.get();
        m_acceptors.push_back(std::move(acceptor));
        event_loop->run([a]{a->listen();});
        DEBUG_I("Server %s with id %d listening on port %d", m_name.c_str(), m_server_id, m_listen_port);
    }
}

int32_t TcpServer::server_id() const {
    return m_server_id;
}


TcpServer::~TcpServer() = default;