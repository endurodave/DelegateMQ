#include "DelegateMQ.h"
#include "main.h"
#include <stdio.h>

// External reference to the UART handle initialized in main.cpp
extern UART_HandleTypeDef huart6;
extern NetworkEngine* g_netEngine; // Defined in main.cpp

using namespace dmq;

class NetworkHandler {
public:
    // This executes on the target thread
    void RemoteFunc(int val) {
        printf("  [Network] RemoteFunc Executed: %d\n", val);
    }
};

void StartNetworkTests() {
    printf("\n=== STARTING NETWORK TESTS ===\n");

    // 1. Initialize Network Engine (Lazy Init)
    static NetworkEngine engine;
    g_netEngine = &engine;
    
    printf("[Network] Initializing UART Transport...\n");
    if (g_netEngine->Initialize(&huart6) == 0) {
        g_netEngine->Start();
        printf("[Network] Engine Started.\n");
    } else {
        printf("[Network] Engine Init FAILED!\n");
        return;
    }

    // 2. Async Delegate Test (Cross-Thread)
    printf("[Async] Dispatching to worker thread...\n");
    NetworkHandler handler;
    Thread worker("Worker");
    worker.CreateThread();

    // Bind function to run on 'worker' thread
    auto asyncDelegate = MakeDelegate(&handler, &NetworkHandler::RemoteFunc, worker);
    asyncDelegate(777); // Fire and forget

    vTaskDelay(pdMS_TO_TICKS(100)); // Allow time to process
    worker.ExitThread();

    printf("=== NETWORK TESTS COMPLETE ===\n");
}