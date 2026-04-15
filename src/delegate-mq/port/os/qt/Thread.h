#ifndef _QT_THREAD_H
#define _QT_THREAD_H

/// @file QtThread.h
/// @brief Qt implementation of the DelegateMQ IThread interface.
///
/// @note This implementation is a basic port. For reference, the stdlib and win32
/// implementations provide additional features:
/// 1. Priority Support: Uses a priority queue to respect dmq::Priority.
/// 2. Watchdog: Includes a ThreadCheck() heartbeat mechanism.
/// 3. Synchronized Startup: CreateThread() blocks until the worker thread is ready.
///
/// **Key Features:**
/// * **QThread Integration:** Wraps `QThread` and uses a Worker object to execute 
///   delegates in the target thread's event loop.
/// * **FullPolicy Support:** Configurable back-pressure (BLOCK or DROP) using 
///   `QMutex` and `QWaitCondition`.
/// * **Signal/Slot Dispatch:** Uses Qt's meta-object system to bridge delegate 
///   execution across thread boundaries.
///
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

/// @brief Policy applied when the thread message queue is full.
/// @details Only meaningful when maxQueueSize > 0.
///   - BLOCK: DispatchDelegate() blocks the caller until space is available (back pressure).
///   - DROP:  DispatchDelegate() silently discards the message and returns immediately.
///
/// Use DROP for high-rate best-effort topics (sensor telemetry, display updates) where
/// a stale sample is preferable to stalling the publisher. Use BLOCK for critical topics
/// (commands, state transitions) where no message may be lost.
enum class FullPolicy { BLOCK, DROP };

class Thread : public QObject, public dmq::IThread
{
    Q_OBJECT

public:
    /// Default queue size if 0 is passed
    static const size_t DEFAULT_QUEUE_SIZE = 20;

    /// Constructor
    /// @param threadName Name for debugging (QObject::objectName)
    /// @param maxQueueSize Max number of messages in queue (0 = Default 20)
    /// @param fullPolicy Action when queue is full: BLOCK the caller or DROP the message.
    Thread(const std::string& threadName, size_t maxQueueSize = 0, FullPolicy fullPolicy = FullPolicy::BLOCK);

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

    /// Returns true if the calling thread is this thread
    virtual bool IsCurrentThread() override;

    std::string GetThreadName() const { return m_threadName; }

    /// Get current queue size
    size_t GetQueueSize() const { return m_queueSize.load(); }

    // IThread Interface Implementation
    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

signals:
    // Internal signal to bridge threads
    void SignalDispatch(std::shared_ptr<dmq::DelegateMsg> msg);

private slots:
    void OnMessageProcessed() { 
        m_queueSize--; 
        m_cvNotFull.wakeAll();
    }

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    const std::string m_threadName;
    const size_t m_maxQueueSize;
    const FullPolicy m_fullPolicy;
    QThread* m_thread = nullptr;
    Worker* m_worker = nullptr;
    std::atomic<size_t> m_queueSize{0};
    QMutex m_mutex;
    QWaitCondition m_cvNotFull;
};

// ----------------------------------------------------------------------------
// Worker Object
// Lives on the target QThread and executes the slots
// ----------------------------------------------------------------------------
class Worker : public QObject
{
    Q_OBJECT
signals:
    void MessageProcessed();

public slots:
    void OnDispatch(std::shared_ptr<dmq::DelegateMsg> msg)
    {
        if (msg) {
            auto invoker = msg->GetInvoker();
            if (invoker) {
                invoker->Invoke(msg);
            }
        }
        emit MessageProcessed();
    }
};

#endif // _QT_THREAD_H