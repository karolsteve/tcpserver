/*
* Created by Steve Tchatchouang
 *
 * Copyright (c) 2022 All rights reserved
 */

#if !defined(TKS_ASYNC_WAKER)
#define TKS_ASYNC_WAKER

#include <memory>

class EventLoop;
class Channel;

class AsyncWaker
{
private:
    /* data */
    int m_waker_fd;
    EventLoop *m_loop;
    std::unique_ptr<Channel> m_waker_channel;
    void handleRead(int64_t) const;

public:
    explicit AsyncWaker(EventLoop *loop);
    ~AsyncWaker();

    void wakeup() const;
};

#endif // TKS_ASYNC_WAKER
