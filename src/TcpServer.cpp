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
#include <iostream>
#include <utility>
#include "TcpConnection.hpp"
#include "buffer/ProtoBuffer.h"

TcpServer::TcpServer(EventLoop *loop, const uint16_t listen_port, std::string name, int server_id,
                     int32_t snd_buff, int32_t rcv_buff, int32_t keep_alive, int32_t backlog, int32_t linger_enabled)
        : m_loop(loop),
          m_acceptor(new Acceptor(loop, listen_port, snd_buff, rcv_buff, keep_alive, backlog, linger_enabled != 0)),
          m_started(false), m_next_conn_id(1L), m_thread_pool(new EventLoopThreadPool(loop)), m_name(std::move(name)),
          m_server_id(server_id) {

    assert(loop);

    m_acceptor->set_new_conn_callback([this](int sock_fd, const std::string &ip, int16_t port, int family) {
        on_new_connection(sock_fd, ip, port, family);
    });
}

void TcpServer::set_pool_size(uint32_t numThreads) {
    assert(numThreads >= 0);
    m_thread_pool->set_pool_size(numThreads);
}

int TcpServer::listen_port() {
    return m_acceptor->listen_port();
}

uint32_t TcpServer::pool_size() const {
    return m_thread_pool->pool_size();
}

void TcpServer::start() {
    if (!m_started) {
        m_started = true;
        m_thread_pool->start();
    }

    if (!m_acceptor->listening()) {
        m_acceptor->listen();
        DEBUG_I("Server %s with id %d listening on port %d", m_name.c_str(), m_server_id, m_acceptor->listen_port());
    }
}

void TcpServer::on_new_connection(int sock_fd, const std::string &ip, int16_t port, int family) {
    DEBUG_D("New connection %s:%d sock_fd : %d family %d", ip.c_str(), port, sock_fd, family);

    // Lorsqu'une nouvelle connexion arrive, enregistrez d'abord un objet TcpConnection, puis gérez-le avec le event_loop_thread

    EventLoop *event_loop = m_thread_pool->get_next_loop();

    auto conn = std::make_shared<TcpConnection>(event_loop, sock_fd, ip, port, family, m_next_conn_id);
    m_connections[m_next_conn_id] = conn;
    m_next_conn_id++;

    conn->set_on_connection_state_change(m_connection_state_change_cb);
    conn->set_on_data_received(m_data_received_cb);
    conn->set_on_write_complete(m_write_complete_cb);
    conn->set_on_connection_closed([this](auto _arg) { remove_connection(_arg); });

    event_loop->run([conn] { conn->connection_established(); });
}

void TcpServer::remove_connection(std::shared_ptr<TcpConnection> const &conn) {
    m_loop->run([this, conn] { remove_connection_internal(conn); });
}

void TcpServer::remove_connection_internal(std::shared_ptr<TcpConnection> const &conn) {
    m_loop->assertInLoopThread();
    DEBUG_D("TcpServer::removeConnection [ %s ] - connection %ld [%s]", m_name.c_str(), conn->conn_id(),
            conn->ip_addr().c_str());

    // À ce stade, l'objet conn est détenu par lui-même et l'objet m_connections,
    // Le nombre de références tombe à 1 lorsque conn est supprimé de m_connections,
    // S'il n'est pas traité, il sera détruit après avoir quitté le champ d'application
    // Enfin utilisé std::bind pour prolonger la durée de vie de TcpConnection à connectDestroyed
    // lorsque l'appel se termine
    size_t n = m_connections.erase(conn->conn_id());
    assert(n == 1);
    (void) n;
    EventLoop *event_loop = conn->event_loop();
    event_loop->queue([conn] { conn->connection_destroyed(); });
}

int32_t TcpServer::server_id() const {
    return m_server_id;
}

std::shared_ptr<TcpConnection> TcpServer::conn(long id) {
    m_loop->assertInLoopThread();
    if (m_connections.find(id) != m_connections.end()) {
        return m_connections[id];
    }
    return nullptr;
}

TcpServer::~TcpServer() = default;