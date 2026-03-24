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
#include "predef/util/NetworkConnect.h"
#else
#include "predef/transport/linux-udp/MulticastTransport.h"
#include "predef/util/NetworkConnect.h"
#endif

using namespace ftxui;

// State shared between DataBus callbacks and UI thread
struct GlobalState {
    std::mutex mutex;
    std::map<std::string, ShapeMsg> shapes;
    ScreenInteractive* screen = nullptr;
} g_state;

int main() {
    NetworkContext winsock;
    std::string localIP = NetworkContext::GetLocalAddress();

    // 1. Initialize Multicast Transport (Group: 239.1.1.1, Port: 8000)
    MulticastTransport transport;
    if (transport.Create(MulticastTransport::Type::SUB, "239.1.1.1", 8000, localIP.c_str()) != 0) {
        std::cerr << "Failed to create Multicast transport" << std::endl;
        return -1;
    }

    auto screen = ScreenInteractive::Fullscreen();
    g_state.screen = &screen;

    // 2. Setup Participant and Handlers
    auto group = std::make_shared<dmq::Participant>(transport);
    static Serializer<void(ShapeMsg)> serializer;

    auto shapeHandler = [](const std::string& topic, ShapeMsg msg) {
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            g_state.shapes[topic] = msg;
        }
        if (g_state.screen) {
            g_state.screen->PostEvent(Event::Custom);
        }
    };

    // Use explicit std::function to match RegisterHandler signature
    using HandlerFunc = std::function<void(ShapeMsg)>;
    group->RegisterHandler<ShapeMsg>(SystemTopic::SquareId, serializer, HandlerFunc([&](ShapeMsg m) { shapeHandler(SystemTopic::Square, m); }));
    group->RegisterHandler<ShapeMsg>(SystemTopic::CircleId, serializer, HandlerFunc([&](ShapeMsg m) { shapeHandler(SystemTopic::Circle, m); }));
    group->RegisterHandler<ShapeMsg>(SystemTopic::TriangleId, serializer, HandlerFunc([&](ShapeMsg m) { shapeHandler(SystemTopic::Triangle, m); }));

    // 3. Network processing thread
    std::atomic<bool> running{true};
    std::thread netThread([&]() {
        while (running) {
            group->ProcessIncoming();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // 4. FTXUI Render Loop
    auto renderer = Renderer([&] {
        auto c = Canvas(200, 100);
        
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            for (auto const& [topic, shape] : g_state.shapes) {
                if (topic == SystemTopic::Square) {
                    for(int x=0; x<10; ++x) 
                        for(int y=0; y<10; ++y) c.DrawBlock(shape.x * 2 + x, shape.y * 2 + y, true, Color::Blue);
                } else if (topic == SystemTopic::Circle) {
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
                text("Data-Driven Rendering | 'q' to quit") | dim,
                filler()
            })
        });
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(component);

    running = false;
    netThread.join();
    transport.Close();

    std::cout << "\r" << std::flush; 

    return 0;
}
