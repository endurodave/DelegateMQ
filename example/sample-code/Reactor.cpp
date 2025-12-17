/// @file
/// @brief Implement the reactor pattern using a delegate to store and 
/// invoke the event handlers.

#include "Reactor.h"
#include "DelegateMQ.h"
#include <iostream>
#include <map>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace dmq;
using namespace std;

namespace Example
{
    // Event types
    enum class EventType {
        Timer,
        IORead,
        IOWrite
    };

    // Base class for event handlers
    class EventHandler {
    public:
        virtual ~EventHandler() = default;
        virtual void handleEvent() = 0;
    };

    // TimerEventHandler: Simulates a timeout event handler
    class TimerEventHandler : public EventHandler {
    public: 
        void handleEvent() override {
            std::cout << "Timer event triggered!" << std::endl;
        }
    };

    // IOReadEventHandler: Simulates a read event handler
    class IOReadEventHandler : public EventHandler {
    public:
        void handleEvent() override {
            std::cout << "Read event triggered!" << std::endl;
        }
    };

    // IOWriteEventHandler: Simulates a write event handler
    class IOWriteEventHandler : public EventHandler {
    public:
        void handleEvent() override {
            std::cout << "Write event triggered!" << std::endl;
        }
    };

    // Reactor class to manage event handling
    class Reactor {
    public:
        using EventHandlerPtr = std::shared_ptr<EventHandler>;

        void registerEvent(EventType eventType, EventHandlerPtr handler) {
            std::lock_guard<std::mutex> lock(mutex_);
            eventHandlers_[eventType] = MakeDelegate(handler, &EventHandler::handleEvent);
        }

        void deregisterEvent(EventType eventType) {
            std::lock_guard<std::mutex> lock(mutex_);
            eventHandlers_.erase(eventType);
        }

        void dispatchEvents() {
            // Simulate a simple event dispatch loop
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));  // Wait for events

                // For demonstration, just call each handler in the event map
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& [eventType, handler] : eventHandlers_) {
                    handler();  // Trigger the event handler
                }

                // Exit the loop after some number of iterations (to avoid an infinite loop in this example)
                static int count = 0;
                if (++count > 5) break;
            }
        }

    private:
        std::map<EventType, DelegateMemberSp<EventHandler, void(void)>> eventHandlers_;
        std::mutex mutex_;
    };


    void ReactorExample() {
        Reactor reactor;

        // Register event handlers
        reactor.registerEvent(EventType::Timer, std::make_shared<TimerEventHandler>());
        reactor.registerEvent(EventType::IORead, std::make_shared<IOReadEventHandler>());
        reactor.registerEvent(EventType::IOWrite, std::make_shared<IOWriteEventHandler>());

        // Start the reactor to dispatch events
        reactor.dispatchEvents();
    }
}
