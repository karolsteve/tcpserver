/*
 * Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#include "TcpConnection.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"

#include <cassert>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <cstring>
#include <utility>
#include "buffer/ByteStream.h"
#include "buffer/ProtoBuffer.h"
#include "timeutils/TimeUtils.hpp"

TcpConnection::TcpConnection(EventLoop *loop, int sock_fd, std::string ip, const uint16_t port, const int family, const long conn_id)
        : m_loop(loop), m_fd(sock_fd), m_ip(std::move(ip)), m_port(port), m_family(family), m_conn_id(conn_id),
          m_channel(std::make_unique<Channel>(loop, sock_fd, true)), m_outgoing_byte_stream(std::make_unique<ByteStream>()) {
    assert(loop);

    m_last_event_time = TimeUtils::current_time_in_millis();

    m_channel->set_read_cb([this](int64_t received_time) { handle_read(received_time); });
    m_channel->set_write_cb([this] { handle_write(); });
    m_channel->set_close_cb([this] { handle_close(0); });
    m_channel->set_error_cb([this] { handle_error(0); });
    m_channel->set_periodic_notification_cb([this](auto now) { on_periodic_notification(now); });
}

TcpConnection::~TcpConnection() {
    ::close(m_fd);
    if (m_outgoing_byte_stream != nullptr) {
        m_outgoing_byte_stream->clean();
        m_outgoing_byte_stream = nullptr;
    }

    DEBUG_D("TcpConnection::dtor[%ld] fd is %d", m_conn_id, m_fd);
}

void TcpConnection::connection_established() {
    DEBUG_D("CONN ESTABLISHED");
    m_loop->assertInLoopThread();
    assert(m_state == kConnecting);
    m_state = kConnected;
    m_channel->enable_reading();
    m_connection_state_change_cb(shared_from_this());
    m_last_event_time = TimeUtils::current_time_in_millis();
    set_timeout(15);//just to detect and close useless conn
}

void TcpConnection::on_periodic_notification(const int64_t now) {
    DEBUG_D("Periodic event %ld fd is %d", m_conn_id, m_fd);

    check_timeout(now);
}

void TcpConnection::handle_read(const int64_t receiveTime) {
    m_loop->assertInLoopThread();

    ProtoBuffer *buffer = m_loop->network_buffer();
    while (true) {
        buffer->rewind();
        const ssize_t readCount = recv(m_channel->fd(), buffer->bytes(), READ_BUFFER_SIZE, MSG_DONTWAIT);
        const int local_errno = errno;
        int opt_val;
        if (socklen_t opt_len = sizeof opt_val; ::getsockopt(m_channel->fd(), SOL_SOCKET, SO_ERROR, &opt_val, &opt_len) == 0) {
            DEBUG_D("ROROR %d %d", opt_val, local_errno);
        }
        DEBUG_D("Handle read count %ld", readCount);
        if (readCount < 0) {
            if (local_errno == EAGAIN || local_errno == EWOULDBLOCK) {
                break;
            }

            DEBUG_F("connection recv failed. errno is %d. client %ld [%s]: %s", local_errno, m_conn_id, ip_addr().c_str(), strerror(local_errno));
            handle_error(local_errno);
            return;
        }

        if (readCount == 0) {
            DEBUG_W("Closing sock on read 0 %s %hd", ip_addr().c_str(), port());
            handle_close(0);
            return;
        }

        buffer->limit((uint32_t) readCount);
        m_last_event_time = TimeUtils::current_time_in_millis();
        m_data_received_cb(shared_from_this(), buffer, receiveTime);
        if (m_state != kConnected) return;
    }
}

void TcpConnection::handle_write() {
    m_loop->assertInLoopThread();

    DEBUG_D("Handle write. state is %s", state_str().c_str());

    if (!m_channel->has_write_op()) {
        DEBUG_W("HANDLE WRITE CALLED... but NOT WRITE OPS. %ld [%s] state is %s", conn_id(), ip_addr().c_str(),state_str().c_str());
        return;
    }

    ProtoBuffer *buffer = m_loop->network_buffer();
    buffer->clear();
    m_outgoing_byte_stream->get(buffer);
    buffer->flip();

    if (uint32_t remaining = buffer->remaining(); remaining != 0) {
        uint32_t total_sent = 0;
        while (true) {
            ssize_t sent_length = ::send(m_channel->fd(), buffer->bytes() + total_sent, remaining, MSG_NOSIGNAL | MSG_DONTWAIT);
            const int local_errno = errno;
            if (sent_length < 0) {
                if (local_errno == EWOULDBLOCK || local_errno == EAGAIN) {
                    DEBUG_W("Got would block on tks send");
                    break;
                }
                DEBUG_W("Error when writing on socket %d", local_errno);
                handle_error(local_errno);
                return;
            }

            m_outgoing_byte_stream->discard(sent_length);
            if (!m_outgoing_byte_stream->has_data()) {
                m_channel->disable_write();
                auto self = shared_from_this();
                m_loop->queue([self] { self->m_write_completed_cb(self->shared_from_this()); });
                if (m_state == kDisconnecting) {
                    graceful_shutdown_internal();
                }
            }
            total_sent += sent_length;
            remaining -= sent_length;
            if (remaining == 0) {
                break;
            }
        }
    }
}

void TcpConnection::handle_error(int local_errno) {
    if (local_errno == 0) {
        int opt_val;
        socklen_t opt_len = sizeof opt_val;
        if (::getsockopt(m_channel->fd(), SOL_SOCKET, SO_ERROR, &opt_val, &opt_len) == 0) {
            local_errno = opt_val;
        }
    }

    // Si l'erreur est "saine", on ne fait rien et on continue
    if (local_errno == 0 || local_errno == EAGAIN || local_errno == EWOULDBLOCK || local_errno == EINTR) {
        DEBUG_W("[EventLoop][%s] Socket transient warning fd=%d: err=%d desc=%s. Ignored.",ip_addr().c_str(), m_channel->fd(), local_errno, std::strerror(local_errno));
        return;
    }

    DEBUG_E("[EventLoop][%s] CLOSING SOCKET fd=%d: err=%d desc=%s", ip_addr().c_str(), m_channel->fd(), local_errno, std::strerror(local_errno));
    handle_close(1);
}

void TcpConnection::handle_close(const int reason) {
    m_loop->assertInLoopThread();
    DEBUG_W("Close called with reason %d. state is %s", reason, state_str().c_str());

    if (m_state == kDisconnected) return;
    assert(m_state == kConnected || m_state == kDisconnecting);

    m_state = kDisconnected;

    m_last_event_time = TimeUtils::current_time_in_millis();

    m_channel->disable_all();
    m_connection_close_cb(shared_from_this());
}

void TcpConnection::connection_destroyed()
{
    m_loop->assertInLoopThread();
    assert(m_state == kDisconnected);
    m_loop->remove_channel(m_channel.get());
    m_connection_state_change_cb(shared_from_this());
}

void TcpConnection::graceful_shutdown() {
    auto self = shared_from_this();
    m_loop->run([self]
    {
        DEBUG_D("Graceful Shutdown called on conn %ld. state is %s", self->conn_id(), self->state_str().c_str());
        if (self->m_state != kConnected && self->m_state != kConnecting)
        {
            return;
        }
        self->m_state = kDisconnecting;
        self->m_shutdown_started = true;
        self->m_shutdown_time = TimeUtils::current_time_in_millis();
        self->graceful_shutdown_internal();
    });
}

//why close is not called directly https://stackoverflow.com/a/23483487/2413201
void TcpConnection::graceful_shutdown_internal() const {
    m_loop->assertInLoopThread();
    if (!m_channel->has_write_op()) {
        //fermer le socket avec élégance
        ::shutdown(m_channel->fd(), SHUT_WR);
    }
}

void TcpConnection::write_buffer(ProtoBuffer *buffer) {
    auto self = shared_from_this();
    m_loop->run([self, buffer]
    {
        if (self->is_connected()) {
            self->write_buffer_internal(buffer);
        } else {
            DEBUG_E("WRITE BUFF CALLED WHEN not connected. state is %s", self->state_str().c_str());
        }
    });

}

void TcpConnection::write_buffer_internal(ProtoBuffer *buffer) const
{
    m_loop->assertInLoopThread();
    m_outgoing_byte_stream->append(buffer);
    if (!m_channel->has_write_op() && m_outgoing_byte_stream->has_data()) {
        m_channel->enable_writing();
    }
}

void TcpConnection::set_timeout(time_t timeout) {
    m_timeout = timeout;
    m_last_event_time = TimeUtils::current_time_in_millis();
}

bool TcpConnection::is_connected() const {
    return m_state == kConnected;
}

void TcpConnection::check_timeout(const uint64_t now) {
    m_loop->assertInLoopThread();
    if (m_shutdown_started) {
        if (now - m_shutdown_time > 5'000)
        {
            DEBUG_E("HAMMER");
            handle_close(-1);
        }
    } else {
        if (now - m_last_event_time > m_timeout * 1000L)
            graceful_shutdown();
    }
}
