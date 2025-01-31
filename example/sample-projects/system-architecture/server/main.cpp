/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ and MsgPack with remote delegates examples. 

#include "NetworkMgr.h"
#include "ServerApp.h"
#include <thread>

int main() 
{
    std::cout << "Server start!" << std::endl;

    NetworkMgr::Instance().Start();
    ServerApp::Instance();

    // Let client and server communicate
    std::this_thread::sleep_for(std::chrono::seconds(30));

    NetworkMgr::Instance().Stop();
    return 0;
}