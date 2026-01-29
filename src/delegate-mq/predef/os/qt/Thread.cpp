#include "QtThread.h"
#include <QDebug>

// Register the metatype ID once
static int registerId = qRegisterMetaType<std::shared_ptr<dmq::DelegateMsg>>();

//----------------------------------------------------------------------------
// Thread Constructor
//----------------------------------------------------------------------------
Thread::Thread(const std::string& threadName)
    : m_threadName(threadName)
{
}

//----------------------------------------------------------------------------
// Thread Destructor
//----------------------------------------------------------------------------
Thread::~Thread()
{
    ExitThread();
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread()
{
    if (!m_thread)
    {
        m_thread = new QThread();
        m_thread->setObjectName(QString::fromStdString(m_threadName));

        // Create worker and move it to the new thread
        m_worker = new Worker();
        m_worker->moveToThread(m_thread);

        // Connect the Dispatch signal to the Worker's slot.
        // Qt::QueuedConnection is mandatory for cross-thread communication,
        // but Qt defaults to AutoConnection which handles this correctly.
        connect(this, &Thread::SignalDispatch, 
                m_worker, &Worker::OnDispatch, 
                Qt::QueuedConnection);

        // Ensure worker is deleted when thread finishes
        connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
        
        // Also delete the QThread object itself when finished (optional, depending on ownership)
        // connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

        m_thread->start();
    }
    return true;
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
        
        // Cleanup manually if not using deleteLater
        delete m_thread; 
        m_thread = nullptr;
        
        // Worker is usually deleted by deleteLater, but we can force null here
        m_worker = nullptr;
    }
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
QThread* Thread::GetThreadId()
{
    return m_thread;
}

//----------------------------------------------------------------------------
// GetCurrentThreadId
//----------------------------------------------------------------------------
QThread* Thread::GetCurrentThreadId()
{
    return QThread::currentThread();
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
    // Safety check: Don't emit if thread is tearing down
    if (m_thread && m_thread->isRunning()) {
        emit SignalDispatch(msg);
    }
}