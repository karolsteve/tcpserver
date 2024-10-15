//
// Created by Steve Tchatchouang
//

#include "fastlog/not_copyable.hpp"
#include <vector>
#include <memory>

class EventLoop;

class EventLoopThread;

class EventLoopThreadPool : notcopyable {
public:
    explicit EventLoopThreadPool(EventLoop *base_loop) noexcept;

    ~EventLoopThreadPool();

    void set_pool_size(uint32_t num_threads) { m_num_threads = num_threads; }

    [[nodiscard]] uint32_t pool_size() const { return m_num_threads; }

    void start();

    EventLoop *get_next_loop();

private:
    EventLoop *m_base_loop;
    bool m_started;
    uint32_t m_num_threads;
    uint32_t m_next;

    std::vector<std::unique_ptr<EventLoopThread>> m_threads;
    std::vector<EventLoop *> m_loops;
};