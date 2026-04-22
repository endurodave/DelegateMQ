#include "UI.h"
#include "messages/CentrifugeStatusMsg.h"
#include "messages/CentrifugeSpeedMsg.h"
#include "messages/RunStatusMsg.h"
#include "messages/StartProcessMsg.h"
#include "messages/StopProcessMsg.h"
#include "messages/FaultMsg.h"
#include "Constants.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>

using namespace dmq;
using namespace ftxui;
using namespace cellutron;

// --- Global state for UI ---
static std::atomic<uint16_t> g_currentRpm{0};
static std::atomic<RunStatus> g_runStatus{RunStatus::IDLE};
static std::mutex g_uiMutex;
static std::vector<std::string> g_logs;

static void AddLog(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    g_logs.push_back(msg);
    if (g_logs.size() > 50) {
        g_logs.erase(g_logs.begin());
    }
}

UI::~UI() {
    Shutdown();
}

void UI::Start() {
    // 1. Start the DelegateMQ worker thread with Watchdog
    m_thread.CreateThread(std::chrono::seconds(2));

    // 2. Setup DataBus Subscriptions
    auto statusConn = DataBus::Subscribe<CentrifugeStatusMsg>("cell/status/centrifuge", [](CentrifugeStatusMsg msg) {
        auto* screen = ScreenInteractive::Active();
        if (screen) screen->PostEvent(Event::Custom);
    }, &m_thread);

    auto cmdConn = DataBus::Subscribe<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", [](CentrifugeSpeedMsg msg) {
        g_currentRpm = msg.rpm;
        auto* screen = ScreenInteractive::Active();
        if (screen) screen->PostEvent(Event::Custom);
    }, &m_thread);

    auto runConn = DataBus::Subscribe<RunStatusMsg>("cell/status/run", [](RunStatusMsg msg) {
        std::string status_text;
        switch (msg.status) {
            case RunStatus::IDLE: status_text = "IDLE"; break;
            case RunStatus::PROCESSING: status_text = "PROCESSING"; break;
            case RunStatus::ABORTING: status_text = "ABORTING"; break;
            case RunStatus::FAULT: status_text = "FAULT"; break;
        }
        AddLog("Status Changed: " + status_text);
        g_runStatus = msg.status;
        auto* screen = ScreenInteractive::Active();
        if (screen) screen->PostEvent(Event::Custom);
    }, &m_thread);

    auto faultConn = DataBus::Subscribe<FaultMsg>("cell/fault", [](FaultMsg msg) {
        AddLog(">>> CRITICAL FAULT RECEIVED <<<");
        auto* screen = ScreenInteractive::Active();
        if (screen) screen->PostEvent(Event::Custom);
    }, &m_thread);

    // 3. Build FTXUI Components
    std::string btnLabel = " START ";
    
    static auto lastClickTime = std::chrono::steady_clock::now();

    auto btnControl = Button(&btnLabel, [] {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClickTime).count() < 500) {
            return;
        }
        lastClickTime = now;

        if (g_runStatus.load() == RunStatus::IDLE) {
            AddLog("Command: START Process");
            DataBus::Publish<StartProcessMsg>("cell/cmd/run", {});
        } else if (g_runStatus.load() == RunStatus::PROCESSING) {
            AddLog("Command: ABORT Process");
            DataBus::Publish<StopProcessMsg>("cell/cmd/abort", {});
        }
    });

    auto component = Container::Vertical({ btnControl });

    auto renderer = Renderer(component, [&] {
        std::lock_guard<std::mutex> lock(g_uiMutex);
        
        uint16_t rpm = g_currentRpm.load();
        float rpm_fraction = static_cast<float>(rpm) / static_cast<float>(MAX_CENTRIFUGE_RPM);
        std::string status_text;
        Color status_color = Color::White;

        RunStatus currentStatus = g_runStatus.load();

        switch (currentStatus) {
            case RunStatus::IDLE: status_text = "IDLE"; status_color = Color::GrayDark; break;
            case RunStatus::PROCESSING: status_text = "PROCESSING"; status_color = Color::Green; break;
            case RunStatus::ABORTING: status_text = "ABORTING"; status_color = Color::Red; break;
            case RunStatus::FAULT: status_text = "FAULT"; status_color = Color::Red; break;
        }

        if (currentStatus == RunStatus::PROCESSING) btnLabel = " STOP  ";
        else if (currentStatus == RunStatus::ABORTING) btnLabel = " STOP  ";
        else if (currentStatus == RunStatus::FAULT) btnLabel = "LOCKED ";
        else btnLabel = " START ";

        auto rpm_gauge = gauge(rpm_fraction) | vscroll_indicator | frame | size(HEIGHT, EQUAL, 10) | color(Color::Blue);
        
        Elements log_elements;
        int start_idx = std::max(0, (int)g_logs.size() - 8);
        for (size_t i = start_idx; i < g_logs.size(); ++i) {
            log_elements.push_back(text(g_logs[i]));
        }
        auto log_box = vbox(std::move(log_elements)) | size(HEIGHT, EQUAL, 8) | frame | border;

        return vbox({
            // TOP SECTION
            vbox({
                text("Cellutron GUI") | bold | center,
                separator(),
                hbox({
                    vbox({
                        text("Centrifuge RPM") | center,
                        rpm_gauge,
                        text(std::to_string(rpm) + " RPM") | center | bold,
                    }) | flex,
                    separator(),
                    vbox({
                        text("Control") | center,
                        btnControl->Render() | center,
                        filler(),
                        hbox({
                            text(" Status: "),
                            text(status_text) | color(status_color) | bold,
                        }) | center,
                    }) | flex,
                }),
            }),
            // FILLER to push logs to bottom
            filler(),
            // BOTTOM SECTION
            vbox({
                text("System Logs") | dim,
                log_box,
            }),
        }) | border;
    });

    // 4. Run Screen (blocks)
    try {
        auto screen = ScreenInteractive::TerminalOutput();
        screen.Loop(renderer);
    } catch (const std::exception&) {
    }

    // 5. Cleanup after UI exit
    Shutdown();
}

void UI::Shutdown() {
    m_thread.ExitThread();
}
