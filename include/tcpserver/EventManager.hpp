//
// Created by Steve Tchatchouang
//

#if !defined(TKS_EVENT_MANAGER)
#define TKS_EVENT_MANAGER

#include <map>
#include <vector>
#include <cstdint>

class EventLoop;
class Channel;

class EventManager
{
private:
    EventLoop *m_owner_loop;
    int m_epoll_fd;
    std::vector<struct epoll_event> m_event_list;
    std::map<int, Channel *> m_channels;
    std::map<int, Channel *> m_periodic_notification_observers;

public:
    explicit EventManager(EventLoop *loop);
    ~EventManager();

    int64_t epoll(int timeout_ms, std::vector<Channel *> *channels);

    void updateChannel(Channel *channel);
    void remove_channel(Channel *channel);

    void check_periodic_observers();

private:
    void apply_ops(int operation, Channel *channel) const;
};

#endif // TKS_EVENT_MANAGER
