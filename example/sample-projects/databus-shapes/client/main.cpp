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
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>

#include "extras/util/NetworkConnect.h"

#ifdef DMQ_DATABUS_TOOLS
#include "NodeBridge.h"
#endif

using namespace ftxui;

// State shared between DataBus callbacks and the UI thread.
struct GlobalState {
    std::mutex mutex;
    std::map<std::string, ShapeMsg> shapes;
    ScreenInteractive* screen = nullptr;
} g_state;

// Holds all network and DataBus infrastructure for the client.
struct ClientState {
    MulticastTransport transport;
    std::shared_ptr<dmq::Participant> group;
    Serializer<void(ShapeMsg)> serializer;
};

// Create the multicast SUB transport.
static bool SetupTransport(ClientState& s, const std::string& localIP) {
    if (s.transport.Create(MulticastTransport::Type::SUB, "239.1.1.1", 8000, localIP.c_str()) != 0) {
        std::cerr << "Failed to create Multicast transport" << std::endl;
        return false;
    }
    return true;
}

// Register participant and per-shape handlers that update g_state and trigger a UI redraw.
// NOTE: g_state.screen must be assigned before calling this so the PostEvent call is valid.
static void SetupDataBus(ClientState& s) {
    s.group = std::make_shared<dmq::Participant>(s.transport);

    auto shapeHandler = [](const std::string& topic, ShapeMsg msg) {
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            g_state.shapes[topic] = msg;
        }
        if (g_state.screen) {
            g_state.screen->PostEvent(Event::Custom);
        }
    };

    s.group->RegisterHandler<ShapeMsg>(SystemTopic::SquareId,   s.serializer, [shapeHandler](ShapeMsg m) { shapeHandler(SystemTopic::Square,   m); });
    s.group->RegisterHandler<ShapeMsg>(SystemTopic::CircleId,   s.serializer, [shapeHandler](ShapeMsg m) { shapeHandler(SystemTopic::Circle,   m); });
    s.group->RegisterHandler<ShapeMsg>(SystemTopic::TriangleId, s.serializer, [shapeHandler](ShapeMsg m) { shapeHandler(SystemTopic::Triangle, m); });
}

int main(int argc, char* argv[]) {
    int duration = 0;
    if (argc > 1) duration = atoi(argv[1]);

    NetworkContext winsock;
    std::string localIP = NetworkContext::GetLocalAddress();

#ifdef DMQ_DATABUS_TOOLS
    NodeBridge::StartMulticast("ShapesClient", "239.1.1.1", 9998, localIP);
#endif

    ClientState s;
    if (!SetupTransport(s, localIP)) return -1;

    // Assign screen pointer before SetupDataBus so handlers can post UI events.
    auto screen = ScreenInteractive::Fullscreen();
    g_state.screen = &screen;

    SetupDataBus(s);

    // Background thread: process incoming multicast shape data
    std::atomic<bool> running{ true };
    std::thread netThread([&]() {
        while (running) {
            s.group->ProcessIncoming();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // FTXUI render loop
    auto renderer = Renderer([&] {
        auto c = Canvas(200, 100);

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            for (auto const& [topic, shape] : g_state.shapes) {
                if (topic == SystemTopic::Square) {
                    for (int x = 0; x < 10; ++x)
                        for (int y = 0; y < 10; ++y) c.DrawBlock(shape.x * 2 + x, shape.y * 2 + y, true, Color::Blue);
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
                text("Data-Driven Rendering | 'q' or Ctrl+C to quit") | dim,
                filler()
            })
        });
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('\x03')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    if (duration > 0) {
        std::thread([&screen, duration]() {
            std::this_thread::sleep_for(std::chrono::seconds(duration));
            screen.Exit();
        }).detach();
    }

    screen.Loop(component);

    running = false;
    netThread.join();
    s.transport.Close();

#ifdef DMQ_DATABUS_TOOLS
    NodeBridge::Stop();
#endif

    std::cout << "\r" << std::flush;

    return 0;
}
