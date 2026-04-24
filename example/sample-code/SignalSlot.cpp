/// @file SignalSlot.cpp
/// @brief Examples of the Signal-Slot pattern in DelegateMQ.
///
/// @details This file demonstrates:
/// 1. Basic Signal creation as a local variable (stack allocation, no shared_ptr needed).
/// 2. RAII Connection Management using `ScopedConnection`.
/// 3. The Publisher-Subscriber pattern where subscribers are automatically
///    disconnected when they go out of scope.
/// 4. Thread-safe communication between a Publisher and multiple Subscribers
///    using `Signal<>` (inherently thread-safe).
///
/// **Signal<Sig>** is the single thread-safe signal type. It can be used as a local
/// variable, class member, or static member. No shared_ptr management required.
/// Disconnect() from any thread is always safe.
///
/// @see https://github.com/DelegateMQ/DelegateMQ

#include "DelegateMQ.h"
#include <iostream>
#include <string>
#include <thread>

using namespace dmq;
using namespace dmq::os;
using namespace dmq::util;

namespace Example {

    // ----------------------------------------------------------------------------
    // Example 1: Basic Signal and Slot (Lambda)
    // Signal is a plain local variable — no shared_ptr required.
    // ----------------------------------------------------------------------------
    void RunBasicSignalExample()
    {
        std::cout << "--- Basic Signal Example ---\n";

        // Signal can be declared directly as a local variable or class member.
        // No shared_ptr management required.
        Signal<void(std::string)> OnMessage;

        // Create a scoped connection handle
        ScopedConnection conn;

        {
            // Connect a lambda
            conn = OnMessage.Connect(MakeDelegate(+[](std::string msg) {
                std::cout << "Lambda received: " << msg << std::endl;
                }));

            // Fire the signal
            OnMessage("Hello from Signal!");
        }
        // 'conn' is still valid here — Signal is still in scope
        OnMessage("Second message");

        // Manually disconnect
        conn.Disconnect();
        OnMessage("This will not be heard");
    }

    // ----------------------------------------------------------------------------
    // Example 2: Publisher / Subscriber (RAII Lifetime Management)
    // Signal<> is thread-safe: ScopedConnection::~ScopedConnection() may be
    // called safely from a different thread than the one owning the Publisher.
    // ----------------------------------------------------------------------------

    class NewsPublisher
    {
    public:
        // Signal<> is a direct member — no shared_ptr required.
        dmq::Signal<void(const std::string&)> OnBreakingNews;

        void Broadcast(const std::string& news)
        {
            std::cout << "[Publisher] Broadcasting: " << news << std::endl;
            OnBreakingNews(news);
        }
    };

    class NewsSubscriber
    {
    public:
        NewsSubscriber(const std::string& name, NewsPublisher& pub)
            : m_name(name)
            , m_thread("SubThread_" + name)
        {
            m_thread.CreateThread();

            m_connection = pub.OnBreakingNews.Connect(
                MakeDelegate(this, &NewsSubscriber::OnNews, m_thread)
            );
        }

        ~NewsSubscriber()
        {
            std::cout << "[Subscriber] " << m_name << " is shutting down (auto-disconnecting).\n";

            // Explicitly disconnect BEFORE stopping the thread.
            // This ensures no new messages are enqueued while the thread is exiting.
            m_connection.Disconnect();

            // Now stop the thread safely.
            m_thread.ExitThread();
        }

    private:
        void OnNews(const std::string& news)
        {
            std::cout << "  -> [Subscriber] " << m_name << " received: " << news << std::endl;
        }

        std::string m_name;
        Thread m_thread;

        // Holds the connection alive. Auto-disconnects on destructor.
        ScopedConnection m_connection;
    };

    void RunPublisherSubscriberExample()
    {
        std::cout << "\n--- Publisher / Subscriber RAII Example ---\n";

        NewsPublisher cnn;

        {
            NewsSubscriber alice("Alice", cnn);
            cnn.Broadcast("Market is up!");

            {
                NewsSubscriber bob("Bob", cnn);
                cnn.Broadcast("Breaking: Aliens found!"); // Alice and Bob hear it
            }
            // Bob goes out of scope — ScopedConnection destructor disconnects him.

            cnn.Broadcast("Market is down!"); // Only Alice hears it
        }
        // Alice goes out of scope — auto-disconnected.

        cnn.Broadcast("Hello? Is this thing on?"); // No one hears it. Safe.
    }

    // ----------------------------------------------------------------------------
    // Entry Point
    // ----------------------------------------------------------------------------
    void RunSignalSlotExamples()
    {
        RunBasicSignalExample();
        RunPublisherSubscriberExample();
    }

} // namespace Example
