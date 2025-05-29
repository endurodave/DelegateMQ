/// @file main.cpp
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ and msgpack for C++ with remote delegates examples. 
/// https://github.com/msgpack/msgpack-c/tree/cpp_master
/// https://github.com/zeromq
/// 
/// ClientApp and ServerApp communicate using ZeroMQ transport, msgpack 
/// serialization, and DelegateMQ dispatching.
/// 
/// The ServerApp collects data locally and remotely from sensors and 
/// actuators. Start both applications to run sample. 

#include "NetworkMgr.h"
#include "AlarmMgr.h"
#include "ServerApp.h"
#include <thread>

#ifdef DMQ_LOG
#ifdef _WIN32
// https://github.com/gabime/spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>  // MSVC sink
#endif
#endif

std::atomic<bool> processTimerExit = false;
static void ProcessTimers()
{
    while (!processTimerExit.load())
    {
        // Process all delegate-based timers
        Timer::ProcessTimers();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

int main() 
{
#ifdef DMQ_LOG
#ifdef _WIN32
    // Create the MSVC sink (multi-threaded)
    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

    // Optional: Set the log level for the sink
    msvc_sink->set_level(spdlog::level::debug);

    // Create a logger using the MSVC sink
    auto logger = std::make_shared<spdlog::logger>("msvc_logger", msvc_sink);

    // Set as default logger (optional)
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug); // Show debug and above
#endif
#endif

    std::cout << "Server start!" << std::endl;

    // Start the thread that will run ProcessTimers
    std::thread timerThread(ProcessTimers);

    int err = NetworkMgr::Instance().Create();
    if (err)
        std::cout << "NetworkMgr::Create() error: " << err << std::endl;
    NetworkMgr::Instance().Start();
    AlarmMgr::Instance();
    ServerApp::Instance();

    // Let client and server communicate
    std::this_thread::sleep_for(std::chrono::seconds(45));

    NetworkMgr::Instance().Stop();

    // Ensure the timer thread completes before main exits
    processTimerExit.store(true);
    if (timerThread.joinable())
        timerThread.join();

    return 0;
}