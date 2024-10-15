//
// Created by Steve Tchatchouang
//

#ifndef TKS_TCP_CONN
#define TKS_TCP_CONN

#include "fastlog/not_copyable.hpp"
#include "TcpConnContext.hpp"
#include <memory>
#include <string>
#include <functional>
#include <any>

class ProtoBuffer;

class ByteStream;

class EventLoop;

class Channel;

class TcpConnection : notcopyable, public std::enable_shared_from_this<TcpConnection> {
private:
    enum StateE {
        kConnecting,
        kConnected,
        kDisconnecting,
        kDisconnected,
    };

    EventLoop *m_loop;
    int m_fd;
    std::string m_ip;
    int16_t m_port;
    int m_family;
    long m_conn_id;
    StateE m_state;

    std::unique_ptr<Channel> m_channel;

    ByteStream *m_outgoing_byte_stream;

    // in sec
    time_t m_timeout{15};
    int64_t m_last_event_time{0};

    std::unique_ptr<TcpConnContext> m_context{nullptr};

    void handle_read(int64_t receiveTime);

    void handle_write();

    void handle_close(int reason);

    void handle_error(bool check_sock_err = true);

    void shutdown_internal();

    void on_periodic_notification(int64_t now);


    std::function<void(std::shared_ptr<TcpConnection> const &)> m_connection_state_change_cb;
    std::function<void(std::shared_ptr<TcpConnection> const &)> m_write_completed_cb; // unused
    std::function<void(std::shared_ptr<TcpConnection> const &)> m_connection_close_cb;
    std::function<void(std::shared_ptr<TcpConnection> const &, ProtoBuffer *buf, int64_t time)> m_data_received_cb;
public:

    std::string state_str(){
        return m_state == kConnecting ? "Connecting" : m_state == kConnected ? "Cted" : m_state == kDisconnecting ? "Dicting" : m_state == kDisconnected ? "DCT" : "UNK";
    }
    TcpConnection(EventLoop *loop, int sock_fd, std::string ip, int16_t port, int family, long conn_id);

    ~TcpConnection();

    void write_buffer(ProtoBuffer *buffer);

    void write_buffer_internal(ProtoBuffer *buffer);

    void connection_established();

    void connection_destroyed();

    void shutdown();

    void brute_close();

    inline long conn_id() const { return m_conn_id; }

    inline EventLoop *event_loop() { return m_loop; }

    inline std::string ip_addr() const { return m_ip; }

    inline int16_t port() const{ return m_port;}

    void set_timeout(time_t timeout); // in sec
    bool is_connected() const;

    inline void set_context(TcpConnContext *ctx) {
        m_context = std::unique_ptr<TcpConnContext>(ctx);
    }

    inline bool has_context() {
        return m_context != nullptr;
    }

    // void set_context(std::function<void(std::shared_ptr<TcpConnection> const &, ProtoBuffer *, uint32_t, bool)> const &cb)
    // {
    //     m_context = std::make_unique<GatewayContext>(cb);
    // }
    TcpConnContext *get_mutable_context() { return m_context == nullptr ? nullptr : m_context.get(); }

    void set_on_connection_state_change(
            std::function<void(std::shared_ptr<TcpConnection> const &)> const &osc) { m_connection_state_change_cb = osc; }

    void set_on_write_complete(
            std::function<void(std::shared_ptr<TcpConnection> const &)> const &owc) { m_write_completed_cb = owc; }

    void set_on_connection_closed(
            std::function<void(std::shared_ptr<TcpConnection> const &)> const &occ) { m_connection_close_cb = occ; }

    void set_on_data_received(std::function<void(std::shared_ptr<TcpConnection> const &, ProtoBuffer *buf,
                                                 int64_t time)> const &odd) { m_data_received_cb = odd; }
protected:
    void check_timeout(uint64_t now);
};

#endif // TKS_TCP_CONN
