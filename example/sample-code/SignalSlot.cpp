/// @file SignalSlot.cpp
/// @brief Examples of the Signal-Slot pattern in DelegateMQ.
///
/// @details This file demonstrates:
/// 1. Basic Signal creation and lambda connection using the '+' operator.
/// 2. RAII Connection Management using `ScopedConnection`.
/// 3. The Publisher-Subscriber pattern where subscribers are automatically 
///    disconnected when they go out of scope.
/// 4. Thread-safe communication between a Publisher and multiple Subscribers.
///
/// @see https://github.com/endurodave/DelegateMQ

#include "DelegateMQ.h"
#include <iostream>
#include <string>
#include <thread>

using namespace dmq;

namespace Example {

    // ----------------------------------------------------------------------------
    // Example 1: Basic Signal and Slot (Lambda)
    // ----------------------------------------------------------------------------
    void RunBasicSignalExample()
    {
        std::cout << "--- Basic Signal Example ---\n";

        // 1. Create the Signal
        //    Naming: "OnMessage" clearly indicates an event source.
        auto OnMessage = dmq::MakeSignal<void(std::string)>();

        // 2. Create a connection handle
        ScopedConnection conn;

        {
            // 3. Connect a lambda
            conn = OnMessage->Connect(MakeDelegate(+[](std::string msg) {
                std::cout << "Lambda received: " << msg << std::endl;
                }));

            // Fire the signal
            (*OnMessage)("Hello from Signal!");
        }
        // 'conn' is still valid here
        (*OnMessage)("Second message");

        // Manually disconnect
        conn.Disconnect();
        (*OnMessage)("This will not be heard");
    }

    // ----------------------------------------------------------------------------
    // Example 2: Publisher / Subscriber (RAII Lifetime Management)
    // ----------------------------------------------------------------------------

    class NewsPublisher
    {
    public:
        // Method 1: Use the dmq::SignalPtr alias (Cleanest)
        // Reduces "std::shared_ptr<SignalSafe<...>>" to just "dmq::SignalPtr<...>"
        // Naming: "OnBreakingNews"
        dmq::SignalPtr<void(const std::string&)> OnBreakingNews
            = dmq::MakeSignal<void(const std::string&)>();

        // Method 2: Use a signature alias (Best for complex signatures)
        // using NewsSig = void(const std::string&);
        // dmq::SignalPtr<NewsSig> OnBreakingNews = dmq::MakeSignal<NewsSig>();

        void Broadcast(const std::string& news)
        {
            std::cout << "[Publisher] Broadcasting: " << news << std::endl;
            // Dereference to invoke
            (*OnBreakingNews)(news);
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

            // Connect to the publisher.
            // Reads naturally: "Connect to pub.OnBreakingNews"
            m_connection = pub.OnBreakingNews->Connect(
                MakeDelegate(this, &NewsSubscriber::OnNews, m_thread)
            );
        }

        ~NewsSubscriber()
        {
            std::cout << "[Subscriber] " << m_name << " is shutting down (auto-disconnecting).\n";
        }

    private:
        void OnNews(const std::string& news)
        {
            std::cout << "  -> [Subscriber] " << m_name << " received: " << news << std::endl;
        }

        std::string m_name;
        Thread m_thread;

        // The "Magic" Link: Holds the connection alive.
        ScopedConnection m_connection;
    };

    void RunPublisherSubscriberExample()
    {
        std::cout << "\n--- Publisher / Subscriber RAII Example ---\n";

        NewsPublisher cnn;

        {
            // Create a subscriber
            NewsSubscriber alice("Alice", cnn);

            // Broadcast - Alice should hear it
            cnn.Broadcast("Market is up!");

            {
                // Create a short-lived subscriber
                NewsSubscriber bob("Bob", cnn);
                cnn.Broadcast("Breaking: Aliens found!"); // Alice and Bob hear it
            }
            // Bob goes out of scope here. His ScopedConnection destructor runs.

            cnn.Broadcast("Market is down!"); // Only Alice hears it
        }
        // Alice goes out of scope. She is removed.

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