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

static bool check_socket_error(int fd) {
    if (fd < 0) {
        return true;
    }
    int ret;
    int code;
    socklen_t len = sizeof(int);
    ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &code, &len);
    if (ret != 0 || code != 0) {
        DEBUG_W("Sock error %d ret is %d code is %d", fd, ret, code);
    }
    return (ret || code) != 0;
}

TcpConnection::TcpConnection(EventLoop *loop, int sock_fd, std::string ip, int16_t port, int family, long conn_id)
        : m_loop(loop), m_fd(sock_fd), m_ip(std::move(ip)), m_port(port), m_family(family), m_conn_id(conn_id),
          m_state(kConnecting),
          m_channel(new Channel(loop, sock_fd, true)), m_outgoing_byte_stream(new ByteStream()) {
    assert(loop);

    m_last_event_time = TimeUtils::current_time_in_millis();

    m_channel->set_read_cb([this](int64_t received_time) { handle_read(received_time); });
    m_channel->set_write_cb([this] { handle_write(); });
    m_channel->set_close_cb([this] { handle_close(0); });
    m_channel->set_error_cb([this] { handle_error(true); });
    m_channel->set_periodic_notification_cb([this](auto now) { on_periodic_notification(now); });
}

TcpConnection::~TcpConnection() {
    ::close(m_fd);
    if (m_outgoing_byte_stream != nullptr) {
        m_outgoing_byte_stream->clean();
        delete m_outgoing_byte_stream;
        m_outgoing_byte_stream = nullptr;
    }

    DEBUG_D("TcpConnection::dtor[%ld] fd is %d", m_conn_id, m_fd);
}

void TcpConnection::on_periodic_notification(int64_t now) {
    DEBUG_D("Periodic event %ld fd is %d", m_conn_id, m_fd);

    check_timeout(now);
}

void TcpConnection::handle_read(int64_t receiveTime) {
    m_loop->assertInLoopThread();

    if (check_socket_error(m_channel->fd())) {
        DEBUG_F("Handle read error");
        handle_error();
        return;
    } else {
        ssize_t readCount;
        ProtoBuffer *buffer = m_loop->network_buffer();
        while (true) {
            buffer->rewind();
            readCount = recv(m_channel->fd(), buffer->bytes(), READ_BUFFER_SIZE, 0);
            DEBUG_D("Handle read count %ld", readCount);
            if (readCount < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }

                DEBUG_F("connection recv failed. errno is %d. client %ld [%s]: %s", errno, m_conn_id, ip_addr().c_str(),
                        strerror(errno));
                handle_error(false);//new
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

//            if (readCount != READ_BUFFER_SIZE) {
//                break;
//            }
        }
    }
}

void TcpConnection::handle_write() {
    m_loop->assertInLoopThread();

    DEBUG_D("Handle write");

    if (!m_channel->has_write_op()) {
        DEBUG_F("HANDLE WRITE CALLED... but NOT WRITE OPS. %ld [%s] state is %s", conn_id(), ip_addr().c_str(),
                state_str().c_str());
        return;
    }

    if (check_socket_error(m_channel->fd()) != 0) {
        DEBUG_F("OOOOHHH SOCK ERR");
        handle_error();
        return;
    }

    ProtoBuffer *buffer = m_loop->network_buffer();
    buffer->clear();
    m_outgoing_byte_stream->get(buffer);
    buffer->flip();

    uint32_t remaining = buffer->remaining();

    if (remaining) {
        uint32_t total_sent = 0;
        while (true) {
            ssize_t sent_length = ::send(m_channel->fd(), buffer->bytes() + total_sent, remaining, 0);
            if (sent_length < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    DEBUG_W("Got would block on tks send");
                    break;
                }
                DEBUG_W("Error when writing on socket %d", errno);
                handle_error(false);
                perror("Error when writing");
                return;
            } else if (sent_length == 0) {
                DEBUG_F("Got 0 on send. HOW IS IT POSSIBLE ??? CHECK IT");
            } else {
                m_outgoing_byte_stream->discard(sent_length);
                if (!m_outgoing_byte_stream->has_data()) {
                    m_channel->disable_write();
                    if (m_write_completed_cb) {
                        m_loop->queue([this] { m_write_completed_cb(this->shared_from_this()); });
                    }
                    if (m_state == kDisconnecting) {
                        shutdown_internal();
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
}

void TcpConnection::handle_close(int reason) {
    m_loop->assertInLoopThread();
    DEBUG_W("Close called with reason %d. state is %s", reason, state_str().c_str());

    m_last_event_time = TimeUtils::current_time_in_millis();

    assert(m_state == kConnected || m_state == kDisconnecting);

    m_channel->disable_all();
    m_connection_close_cb(shared_from_this());
}

void TcpConnection::handle_error(bool check_sock_err) {
    int local_errno = errno;
    if (check_sock_err || local_errno == 0) {
        int opt_val;
        socklen_t opt_len = sizeof opt_val;
        if (::getsockopt(m_channel->fd(), SOL_SOCKET, SO_ERROR, &opt_val, &opt_len) == 0) {
            local_errno = opt_val;
        }
    }

    if (local_errno == ECONNRESET || local_errno == ECONNABORTED) {
        DEBUG_W("Closing because got conn error %d", local_errno);
        handle_close(1);
    } else if (local_errno == ENETUNREACH || local_errno == EHOSTUNREACH) {
        DEBUG_W("Closing because got un_reach error %d", local_errno);
        handle_close(1);
    } else if (local_errno == EPROTO || local_errno == ENOTCONN || local_errno == ESHUTDOWN ||
               local_errno == ENETDOWN) {
        DEBUG_W("Closing because got other net error %d", local_errno);
        handle_close(1);
    } else if (local_errno == ETIMEDOUT) {
        DEBUG_W("Closing socket on conn timeout");
        handle_close(3);
    } else if (local_errno == EPIPE) {
        DEBUG_W("Closing socket on broken pipe");
        handle_close(4);
    } else {
        DEBUG_F("Socket error [%d] in client %ld [%s] ::: %s", local_errno, m_conn_id, ip_addr().c_str(),
                strerror(local_errno));
        perror("Other Detail");
    }
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

void TcpConnection::connection_destroyed() {
//    if(m_state == kDisconnected){
//        return;
//    }
    m_loop->assertInLoopThread();
    assert(m_state == kConnected /*|| m_state == kDisconnecting*/);//FIXME:
    m_state = kDisconnected;
    m_channel->disable_all();
    m_connection_state_change_cb(shared_from_this());
    //FIXME: n'est pas bien écrit comme ça, et ne devrait pas retourner un pointeur nu, mais il n'y aura pas de fuite de mémoire, car il n'y a pas de suppression ailleurs. Peut être remplacé par un pointeur partagé.
    m_loop->remove_channel(m_channel.get());
}

void TcpConnection::write_buffer(ProtoBuffer *buffer) {
    if (m_state == kConnected) {
        if (m_loop->isInLoopThread()) {
            write_buffer_internal(buffer);
        } else {
            m_loop->run([this, buffer] { write_buffer_internal(buffer); });
        }
    } else {
        DEBUG_E("WRITE BUFF CALLED WHEN not connected. state is %s", state_str().c_str());
    }
}

void TcpConnection::write_buffer_internal(ProtoBuffer *buffer) {
    m_loop->assertInLoopThread();
    m_outgoing_byte_stream->append(buffer);
    if (!m_channel->has_write_op() && m_outgoing_byte_stream->has_data()) {
        m_channel->enable_writing();
    }
}

//why is close not called directly https://stackoverflow.com/a/23483487/2413201
void TcpConnection::shutdown() {
    DEBUG_D("Shutdown called on conn %ld. state is %d", conn_id(), m_state);
    if (m_state == kConnected) {
        m_state = kDisconnecting;
        m_loop->run([this] { shutdown_internal(); });
    } else {
        DEBUG_W("YOU have bad state. pls check it. state is %s", state_str().c_str());
    }
}

/**
 * an RST packet will be sent to the other side. This is good for errors.
 * For example, if you think the other party provided wrong data or it refused to provide data (DOS attack?).
 */
void TcpConnection::brute_close() {
    DEBUG_F("BRUTE CLOSE CALLED");
    m_loop->run([this] { handle_close(-1); });
}

//why close is not called directly https://stackoverflow.com/a/23483487/2413201
void TcpConnection::shutdown_internal() {
    m_loop->assertInLoopThread();
    if (!m_channel->has_write_op()) {
        //fermer le socket avec élégance
        if (::shutdown(m_channel->fd(), SHUT_WR) < 0) {
            DEBUG_F("socket shutdown error in client %ld [%s]: %s", m_conn_id, ip_addr().c_str(), strerror(errno));
        }
    }
}

void TcpConnection::set_timeout(time_t timeout) {
    m_timeout = timeout;
    m_last_event_time = TimeUtils::current_time_in_millis();
}

bool TcpConnection::is_connected() const {
    return m_state == kConnected;
}

void TcpConnection::check_timeout(uint64_t now) {
    uint64_t diff = now - m_last_event_time;
    if (m_timeout != 0 && diff > (int64_t) m_timeout * 1000) {
        DEBUG_W("Shut client on timeout state %s timout is %ld diff is %ld state is %s | %s %hd", state_str().c_str(),
                m_timeout, diff, state_str().c_str(), ip_addr().c_str(), port());
        if (diff > ((int64_t) m_timeout * 1000) + 10000) {
            DEBUG_E("HAMMER CALLED");
            brute_close();
        } else {
            shutdown();
        }
    }
}
