//
// Created by Steve Tchatchouang on 04/10/2022.
//

#ifndef TKS_EVENT_OBJECT_H
#define TKS_EVENT_OBJECT_H

#include <cstdint>

class EventObject {
public:
    explicit EventObject(void *object);

    void on_event();

    [[nodiscard]] int64_t time() const;

    void time(int64_t);

private:
    int64_t m_tme{};
    void *m_event_object;
};

#endif //TKS_EVENT_OBJECT_H
