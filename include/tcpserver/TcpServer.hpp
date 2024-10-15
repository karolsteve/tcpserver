//
// Created by Steve Tchatchouang
//

#if !defined(TCP_SERVER)
#define TCP_SERVER

#include "fastlog/not_copyable.hpp"

#include <string>
#include <memory>
#include <map>
#include <functional>

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
    // Le gestionnaire de connexion du serveur pour les nouvelles connexions
    void on_new_connection(int sock_fd, const std::string &ip, int16_t port, int family);

    EventLoop *m_loop; // Objet de boucle de thread principal, utilisé pour gérer accept
    std::unique_ptr<Acceptor> m_acceptor;
    bool m_started;
    long m_next_conn_id;
    std::unique_ptr<EventLoopThreadPool> m_thread_pool;
    std::string m_name;
    int32_t m_server_id;

    // cb
    std::function<void(const std::shared_ptr<TcpConnection> &)> m_connection_state_change_cb;
    std::function<void(const std::shared_ptr<TcpConnection> &, ProtoBuffer *buf, int64_t time)> m_data_received_cb;
    std::function<void(std::shared_ptr<TcpConnection> const &)> m_write_complete_cb;

    // dictionnaire de connexion tcp
    std::map<long, std::shared_ptr<TcpConnection>> m_connections;

    // supprimer la fonction de connexion tcp
    void remove_connection(std::shared_ptr<TcpConnection> const &conn);
    void remove_connection_internal(std::shared_ptr<TcpConnection> const &conn);

public:
    TcpServer(EventLoop *loop, uint16_t listen_port, std::string name, int server_id,
              int32_t snd_buff, int32_t rcv_buff, int32_t keep_alive, int32_t backlog, int32_t linger_enabled);
    ~TcpServer();

    // démarrer le serveur
    void start();

    void set_pool_size(uint32_t num_threads);

    int listen_port();

    [[nodiscard]] uint32_t pool_size() const;

    EventLoop *get_loop() { return m_loop; }

    void set_on_connection_state_change(std::function<void(std::shared_ptr<TcpConnection> const &)> const &cb) { m_connection_state_change_cb = cb; }

    void set_on_data_received(std::function<void(const std::shared_ptr<TcpConnection> &, ProtoBuffer *buf, int64_t time)> const &cb) { m_data_received_cb = cb; }

    void set_on_write_complete(std::function<void(std::shared_ptr<TcpConnection> const &)> const &cb) { m_write_complete_cb = cb; }

    [[nodiscard]] inline std::string name()const{
        return m_name;
    }

    [[nodiscard]] int32_t server_id() const;

    std::shared_ptr<TcpConnection> conn(long id);
};

#endif // TCP_SERVER
