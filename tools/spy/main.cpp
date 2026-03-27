// main.cpp
// @see https://github.com/DelegateMQ/DelegateMQ
// Main entry point for the DelegateMQ Spy Console TUI application.

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <ctime>
#include <algorithm>

#include "UdpSocket.h"
#include "predef/databus/SpyPacket.h"
#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>
#include <bitsery/traits/vector.h>
#include <bitsery/traits/string.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "predef/util/NetworkConnect.h"

using namespace ftxui;

struct LogEntry {
    uint64_t timestamp;
    std::string topic;
    std::string value;
};

std::vector<LogEntry> g_messages;
std::mutex g_msgMutex;
std::atomic<bool> g_running{true};
std::string g_statusMessage = "Waiting for data...";
std::string g_lastError = "None";
std::atomic<uint32_t> g_packetCount{0};
std::string g_filter;
std::shared_ptr<spdlog::logger> g_fileLogger;

void ReceiverThread(uint16_t port, std::string multicastGroup, std::string localInterface) {
    UdpSocket socket;
    if (!socket.Create()) {
        g_statusMessage = "ERROR: Failed to create socket";
        return;
    }

    // For Multicast, always bind to 0.0.0.0
    if (!socket.Bind(port, "0.0.0.0")) {
        g_statusMessage = "ERROR: Could not bind to port " + std::to_string(port);
        return;
    }

    if (!multicastGroup.empty()) {
        if (!socket.JoinGroup(multicastGroup, localInterface.empty() ? "0.0.0.0" : localInterface)) {
            g_statusMessage = "ERROR: Could not join group " + multicastGroup;
            return;
        }
        g_statusMessage = "Joined Group: " + multicastGroup + " Port: " + std::to_string(port);
    } else {
        g_statusMessage = "Listening on Unicast Port: " + std::to_string(port);
    }

    socket.SetReceiveTimeout(100);

    std::vector<uint8_t> buffer(16384);

    while (g_running) {
        int received = socket.Receive(buffer.data(), (int)buffer.size());
        if (received > 0) {
            // INCREMENT IMMEDIATELY to confirm network connectivity
            g_packetCount++;

            dmq::SpyPacket packet;
            auto state = bitsery::quickDeserialization<bitsery::InputBufferAdapter<std::vector<uint8_t>>>(
                {buffer.begin(), static_cast<size_t>(received)}, packet);

            if (state.first == bitsery::ReaderError::NoError) {
                if (g_fileLogger) {
                    g_fileLogger->info("[{}] {}", packet.topic, packet.value);
                }

                std::lock_guard<std::mutex> lock(g_msgMutex);
                g_messages.push_back({packet.timestamp_us, packet.topic, packet.value});
                if (g_messages.size() > 1000) {
                    g_messages.erase(g_messages.begin());
                }
            } else {
                g_lastError = "Bitsery Error: " + std::to_string((int)state.first) + " (Size: " + std::to_string(received) + ")";
            }
        }
    }
}

void PrintUsage() {
    std::cout << "Usage: dmq-spy [port] [--log <filename>] [--multicast <group_addr>] [--interface <addr>]" << std::endl;
    std::cout << "  port: The UDP port to listen on (default: 9999)" << std::endl;
    std::cout << "  --log <filename>: Log all received messages to the specified file" << std::endl;
    std::cout << "  --multicast <addr>: Join a multicast group (e.g. 239.1.1.1)" << std::endl;
    std::cout << "  --interface <addr>: The local interface IP to use (default: auto-detect)" << std::endl;
}

int main(int argc, char* argv[]) {
    uint16_t port = 9999;
    std::string logFile;
    std::string multicastGroup;
    std::string localInterface;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--log" && i + 1 < argc) {
            logFile = argv[++i];
        } else if (arg == "--multicast" && i + 1 < argc) {
            multicastGroup = argv[++i];
        } else if (arg == "--interface" && i + 1 < argc) {
            localInterface = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        } else if (isdigit(arg[0])) {
            port = static_cast<uint16_t>(std::stoi(arg));
        }
    }

    NetworkContext winsock;
    // Auto-detect physical IP if not provided and multicast is used
    if (localInterface.empty() && !multicastGroup.empty()) {
        localInterface = NetworkContext::GetLocalAddress();
    }

    if (!logFile.empty()) {
        try {
            g_fileLogger = spdlog::basic_logger_mt("spy_logger", logFile);
            g_fileLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%f] %v");
            g_fileLogger->info("DelegateMQ Spy logging started. Port: {}", port);
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
            return 1;
        }
    }

    auto screen = ScreenInteractive::Fullscreen();

    std::thread receiver(ReceiverThread, port, multicastGroup, localInterface);

    Component input_filter = Input(&g_filter, "topic filter...");

    auto renderer = Renderer(input_filter, [&] {
        Elements message_elements;
        int filtered_count = 0;
        {
            std::lock_guard<std::mutex> lock(g_msgMutex);
            for (auto it = g_messages.rbegin(); it != g_messages.rend(); ++it) {
                if (g_filter.empty() || it->topic.find(g_filter) != std::string::npos) {
                    filtered_count++;
                    auto t = std::chrono::system_clock::time_point(std::chrono::microseconds(it->timestamp));
                    auto tt = std::chrono::system_clock::to_time_t(t);
                    struct tm tm_buf;
#ifdef _WIN32
                    localtime_s(&tm_buf, &tt);
#else
                    localtime_r(&tt, &tm_buf);
#endif

                    std::stringstream ss;
                    ss << std::put_time(&tm_buf, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << (it->timestamp / 1000 % 1000);

                    message_elements.push_back(hbox({
                        text(ss.str()) | color(Color::GrayDark),
                        separator(),
                        text(it->topic) | color(Color::Yellow) | size(WIDTH, EQUAL, 20),
                        separator(),
                        text(it->value) | color(Color::White)
                    }));
                }
            }
        }

        std::string title = "DelegateMQ Spy Console (Port: " + std::to_string(port);
        if (!multicastGroup.empty()) title += ", Group: " + multicastGroup;
        if (!localInterface.empty()) title += ", IF: " + localInterface;
        title += ")";

        auto header = vbox({
            text(title) | bold | color(Color::Green) | center,
            hbox(text(" Filter: "), input_filter->Render()) | border,
        });

        auto statusColor = g_statusMessage.find("ERROR") != std::string::npos ? Color::Red : Color::Blue;
        auto statusArea = vbox({
            text(" Status: " + g_statusMessage) | color(statusColor),
        }) | border;

        std::string footerText = " Messages: " + std::to_string(g_messages.size()) + " (Total Raw Packets: " + std::to_string(g_packetCount) + ")";
        if (!logFile.empty()) footerText += " [Log: " + logFile + "]";

        return vbox({
            header,
            statusArea,
            vbox(std::move(message_elements)) | yframe | flex | border,
            hbox({
                text(footerText),
                filler(),
                text("Live Feed (Newest at Top) | 'q' to quit ") | dim
            })
        }) | flex;
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q')) {
            g_running = false;
            screen.Exit();
            return true;
        }
        return false;
    });

    std::thread refresher([&] {
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            screen.PostEvent(Event::Custom);
        }
    });

    screen.Loop(component);

    g_running = false;
    if (receiver.joinable()) receiver.join();
    if (refresher.joinable()) refresher.join();

    if (g_fileLogger) g_fileLogger->flush();
    std::cout << "\r" << std::flush;

    return 0;
}
