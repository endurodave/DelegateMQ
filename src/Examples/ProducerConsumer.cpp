/// @file
/// @brief Implement the consumer-producer pattern using asynchronous delegates. 

#include "ProducerConsumer.h"
#include "DelegateLib.h"
#include "WorkerThreadStd.h"
#include <queue>
#include <iostream>

using namespace DelegateLib;
using namespace std;

namespace Example
{
    /// @brief Consume data from one or more producers
    class Consumer
    {
    public:
        static Consumer& Instance() {
            static Consumer instance;
            return instance;
        }

        Consumer() : m_thread("Consumer") { m_thread.CreateThread(); }
        ~Consumer() { m_thread.ExitThread(); }

        /// Process data from any producer
        void Process(int data) {
            // Is the producer executing on m_thread?
            if (m_thread.GetThreadId() != WorkerThread::GetCurrentThreadId()) {
                // Reinvoke Process() on m_thread; non-blocking call (caller does not wait)
                MakeDelegate(this, &Consumer::Process, m_thread).AsyncInvoke(data);
                return;
            }
            cout << "Process: " << data << endl;
        }

    private:
        /// Consumer worker thread
        WorkerThread m_thread;
    };

    /// @brief Produce data for the consumer
    class Producer
    {
    public:
        // Send data to the consumer
        void Produce() { Consumer::Instance().Process(++data); }

    private:
        int data = 0;
    };

    void ProducerConsumerExample()
    {
        Producer producer;
        for (int i = 0; i < 10; i++) {
            producer.Produce();
        }
    }
}