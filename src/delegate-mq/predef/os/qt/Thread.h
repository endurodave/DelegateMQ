#ifndef _QT_THREAD_H
#define _QT_THREAD_H

/// @file QtThread.h
/// @brief Qt implementation of the DelegateMQ IThread interface.

#include "delegate/IThread.h"
#include <QThread>
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <memory>
#include <string>

// Ensure DelegateMsg is known to Qt MetaType system
Q_DECLARE_METATYPE(std::shared_ptr<dmq::DelegateMsg>)

class Worker;

class Thread : public QObject, public dmq::IThread
{
    Q_OBJECT

public:
    /// Constructor
    /// @param threadName Name for debugging (QObject::objectName)
    Thread(const std::string& threadName);

    /// Destructor
    ~Thread();

    /// Create and start the internal QThread
    bool CreateThread();

    /// Stop the QThread
    void ExitThread();

    /// Get the QThread pointer (used as the ID)
    QThread* GetThreadId();

    /// Get the current executing QThread pointer
    static QThread* GetCurrentThreadId();

    std::string GetThreadName() const { return m_threadName; }

    // IThread Interface Implementation
    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

signals:
    // Internal signal to bridge threads
    void SignalDispatch(std::shared_ptr<dmq::DelegateMsg> msg);

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    std::string m_threadName;
    QThread* m_thread = nullptr;
    Worker* m_worker = nullptr;
};

// ----------------------------------------------------------------------------
// Worker Object
// Lives on the target QThread and executes the slots
// ----------------------------------------------------------------------------
class Worker : public QObject
{
    Q_OBJECT
public slots:
    void OnDispatch(std::shared_ptr<dmq::DelegateMsg> msg)
    {
        if (msg) {
            auto invoker = msg->GetInvoker();
            if (invoker) {
                invoker->Invoke(msg);
            }
        }
    }
};

#endif // _QT_THREAD_H