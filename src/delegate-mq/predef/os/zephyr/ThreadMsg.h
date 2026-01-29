#ifndef _THREAD_MSG_H
#define _THREAD_MSG_H

#include "DelegateMQ.h"
#include <memory>

// Message IDs
#define MSG_DISPATCH_DELEGATE   1
#define MSG_EXIT_THREAD         2

class ThreadMsg
{
public:
    // Constructor for generic messages
    ThreadMsg(int id, std::shared_ptr<dmq::DelegateMsg> data = nullptr)
        : m_id(id), m_data(data) {
    }

    virtual ~ThreadMsg() = default;

    int GetId() const { return m_id; }
    std::shared_ptr<dmq::DelegateMsg> GetData() const { return m_data; }

private:
    int m_id;
    std::shared_ptr<dmq::DelegateMsg> m_data;

    XALLOCATOR
};

#endif