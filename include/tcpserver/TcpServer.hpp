//
// Created by Steve Tchatchouang
//

#if !defined(TCP_SERVER)
#define TCP_SERVER

#include "fastlog/not_copyable.hpp"

#include <string>
#include <memory>
#include <functional>
#include <thread>

class Acceptor;
class EventLoop;
class EventLoopThreadPool;
class TcpConnection;
class ProtoBuffer;

// La classe TcpServer est principalement utilisée pour l'établissement, la maintenance et la destruction des connexions Tcp
// Il gère la classe Acceptor pour obtenir la connexion tcp, puis établit la classe TcpConnection pour gérer la connexion tcp
class TcpServer : notcopyable
{
private:

    EventLoop *m_loop; // Objet de boucle de thread principal, utilisé pour gérer accept
    uint16_t m_listen_port;
    bool m_started;
    std::unique_ptr<EventLoopThreadPool> m_thread_pool;
    std::string m_name;
    int32_t m_server_id;
    int32_t m_snd_buff;
    int32_t m_rcv_buff;
    std::vector<std::unique_ptr<Acceptor>> m_acceptors;

    // cb
    std::function<void(const std::shared_ptr<TcpConnection> &)> m_connection_state_change_cb;
    std::function<void(const std::shared_ptr<TcpConnection> &, ProtoBuffer *buf, int64_t time)> m_data_received_cb;
    std::function<void(std::shared_ptr<TcpConnection> const &)> m_write_complete_cb;

public:
    TcpServer(EventLoop *loop, uint16_t listen_port, std::string name, int server_id, int32_t snd_buff, int32_t rcv_buff, uint32_t num_threads);
    ~TcpServer();

    // démarrer le serveur
    void start();

    [[nodiscard]] int listen_port() const;

    [[nodiscard]] uint32_t pool_size() const;

    EventLoop *get_loop() { return m_loop; }

    void set_on_connection_state_change(std::function<void(std::shared_ptr<TcpConnection> const &)> const &cb) { m_connection_state_change_cb = cb; }

    void set_on_data_received(std::function<void(const std::shared_ptr<TcpConnection> &, ProtoBuffer *buf, int64_t time)> const &cb) { m_data_received_cb = cb; }

    void set_on_write_complete(std::function<void(std::shared_ptr<TcpConnection> const &)> const &cb) { m_write_complete_cb = cb; }

    [[nodiscard]] inline std::string name()const{
        return m_name;
    }

    [[nodiscard]] int32_t server_id() const;
};

#endif // TCP_SERVER
