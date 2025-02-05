/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
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

int main() 
{
    std::cout << "Server start!" << std::endl;

    NetworkMgr::Instance().Create();
    NetworkMgr::Instance().Start();
    AlarmMgr::Instance();
    ServerApp::Instance();

    // Let client and server communicate
    std::this_thread::sleep_for(std::chrono::seconds(30));

    NetworkMgr::Instance().Stop();
    return 0;
}