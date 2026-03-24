// main.cpp
// DataBus Shapes Demo Client (Subscriber).
// 
// Receives shape positions via Multicast and renders them in a TUI using FTXUI.

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>

#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>

#if defined(_WIN32) || defined(_WIN64)
#include "predef/transport/win32-udp/MulticastTransport.h"
#include "predef/util/WinsockConnect.h"
#else
#include "predef/transport/linux-udp/MulticastTransport.h"
#endif

using namespace ftxui;

// State shared between DataBus callbacks and UI thread
struct GlobalState {
    std::mutex mutex;
    std::map<std::string, ShapeMsg> shapes;
} g_state;

int main() {
#ifdef _WIN32
    WinsockContext winsock;
    std::string localIP = WinsockContext::GetLocalAddress();
#else
    std::string localIP = "0.0.0.0";
#endif

    // 1. Initialize Multicast Transport (Group: 239.1.1.1, Port: 8000)
    MulticastTransport transport;
    if (transport.Create(MulticastTransport::Type::SUB, "239.1.1.1", 8000, localIP.c_str()) != 0) {
        std::cerr << "Failed to create Multicast transport" << std::endl;
        return -1;
    }

    // 2. Setup Participant and Handlers
    auto group = std::make_shared<dmq::Participant>(transport);
    static Serializer<void(ShapeMsg)> serializer;

    auto shapeHandler = [](const std::string& topic, ShapeMsg msg) {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        g_state.shapes[topic] = msg;
    };

    group->RegisterHandler<ShapeMsg>(SystemTopic::SquareId, serializer, [&](ShapeMsg m) { shapeHandler(SystemTopic::Square, m); });
    group->RegisterHandler<ShapeMsg>(SystemTopic::CircleId, serializer, [&](ShapeMsg m) { shapeHandler(SystemTopic::Circle, m); });
    group->RegisterHandler<ShapeMsg>(SystemTopic::TriangleId, serializer, [&](ShapeMsg m) { shapeHandler(SystemTopic::Triangle, m); });

    // 3. Network processing thread
    std::atomic<bool> running{true};
    std::thread netThread([&]() {
        while (running) {
            group->ProcessIncoming();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // 4. FTXUI Render Loop
    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer([&] {
        // Create a canvas that fills the available space
        auto c = Canvas(200, 100);
        
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            for (auto const& [topic, shape] : g_state.shapes) {
                if (topic == SystemTopic::Square) {
                    // Draw Square (using block coordinates)
                    for(int x=0; x<10; ++x) 
                        for(int y=0; y<10; ++y) c.DrawBlock(shape.x * 2 + x, shape.y * 2 + y, true, Color::Blue);
                } else if (topic == SystemTopic::Circle) {
                    // Draw Circle
                    c.DrawBlockCircle(shape.x * 2, shape.y * 2, 10, Color::Red);
                }
            }
        }

        return vbox({
            text("DelegateMQ DataBus - Multicast Shapes Demo") | bold | color(Color::Green) | center,
            text("Interface: " + localIP + " | Group: 239.1.1.1:8000") | dim | center,
            separator(),
            canvas(std::move(c)) | flex | border,
            hbox({
                filler(),
                text("Newest data received via DataBus Multicast | 'q' to quit") | dim,
                filler()
            })
        });
    });

    // Handle 'q' to quit and sink all other events to prevent character artifacts
    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q')) {
            screen.Exit();
            return true;
        }
        return true; // Sink all events
    });

    // Refresh UI at 20fps to reduce flickering
    std::thread refresher([&] {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            screen.PostEvent(Event::Custom);
        }
    });

    screen.Loop(component);

    running = false;
    netThread.join();
    refresher.join();
    transport.Close();

    std::cout << "\r" << std::flush; 

    return 0;
}
