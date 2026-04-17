/**
 * @file safety/main.cpp
 * @brief Safety CPU — Independent Hardware Monitor & Interlock
 * 
 * ROLE:
 * This node provides an autonomous layer of protection, independent of the 
 * main Controller logic. It is responsible for:
 * 1. Passive monitoring of the high-rate centrifuge RPM topic.
 * 2. Verifying hardware safety limits (e.g., overspeed detection).
 * 3. Publishing system-wide FAULT events that override all other process 
 *    logic to ensure a safe instrument state.
 * 
 * ENVIRONMENT: FreeRTOS (Win32 Simulator)
 */

#include "DelegateMQ.h"
#include "Network.h"
#include "RemoteConfig.h"
#include "Constants.h"
#include <iostream>
#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

using namespace dmq;
using namespace cellutron;

// ---------------------------------------------------------------------------
// Global Objects
// ---------------------------------------------------------------------------
static dmq::ScopedConnection g_speedConn;
static bool g_faulted = false;

// ---------------------------------------------------------------------------
// Heap initialisation
// ---------------------------------------------------------------------------
#define mainREGION_1_SIZE   (512 * 1024)
#define mainREGION_2_SIZE   (256 * 1024)
#define mainREGION_3_SIZE   (256 * 1024)

static uint8_t ucHeap1[mainREGION_1_SIZE];
static uint8_t ucHeap2[mainREGION_2_SIZE];
static uint8_t ucHeap3[mainREGION_3_SIZE];

static void prvInitialiseHeap(void) {
    const HeapRegion_t xHeapRegions[] = {
        { ucHeap1, sizeof(ucHeap1) },
        { ucHeap2, sizeof(ucHeap2) },
        { ucHeap3, sizeof(ucHeap3) },
        { NULL,    0               }
    };
    vPortDefineHeapRegions(xHeapRegions);
}

// ---------------------------------------------------------------------------
// FreeRTOS hooks
// ---------------------------------------------------------------------------
extern "C" void vApplicationIdleHook(void) { Sleep(1); }
extern "C" void vApplicationTickHook(void) {}
extern "C" void vApplicationMallocFailedHook(void) { printf("FreeRTOS Safety: Malloc Failed!\n"); for (;;); }
extern "C" void vApplicationDaemonTaskStartupHook(void) {}
extern "C" void vAssertCalled(unsigned long ulLine, const char* const pcFileName) {
    printf("FreeRTOS Safety: ASSERT FAIL at %s:%lu\n", pcFileName, ulLine);
    for (;;);
}
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** p, StackType_t** s, configSTACK_DEPTH_TYPE* sz) {
    static StaticTask_t x; static StackType_t st[configMINIMAL_STACK_SIZE];
    *p = &x; *s = st; *sz = configMINIMAL_STACK_SIZE;
}
extern "C" void vApplicationGetTimerTaskMemory(StaticTask_t** p, StackType_t** s, configSTACK_DEPTH_TYPE* sz) {
    static StaticTask_t x; static StackType_t st[configTIMER_TASK_STACK_DEPTH];
    *p = &x; *s = st; *sz = configTIMER_TASK_STACK_DEPTH;
}

// ---------------------------------------------------------------------------
// Safety Monitor Task
// ---------------------------------------------------------------------------

static void vSafetyTask(void* /*pvParams*/) {
    printf("Safety: Monitor task started.\n");

    // Standardized thread name for Active Object subsystem with Watchdog
    static Thread m_thread{"SafetyThread"};
    m_thread.CreateThread(std::chrono::seconds(2));

    // 1. Subscribe to centrifuge speed
    g_speedConn = DataBus::Subscribe<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", [](CentrifugeSpeedMsg msg) {
        if (!g_faulted && msg.rpm > MAX_CENTRIFUGE_RPM) {
            g_faulted = true;
            printf("Safety: CRITICAL - Centrifuge speed exceeded limit! (%u RPM). TRIGGERING FAULT.\n", msg.rpm);
            // Notify Controller to go into permanent Fault state
            DataBus::Publish<FaultMsg>("cell/fault", { 1 });
        }
    }, &m_thread);

    // 2. Setup Network Outgoing to Controller and GUI
    Network::GetInstance().AddRemoteNode("Controller", "127.0.0.1", 5011);
    Network::GetInstance().MapTopicToRemote("cell/fault", RID_FAULT_EVENT, "Controller");
    
    Network::GetInstance().AddRemoteNode("GUI", "127.0.0.1", 5010);
    Network::GetInstance().MapTopicToRemote("cell/fault", RID_FAULT_EVENT, "GUI");

    Network::GetInstance().RegisterOutgoingTopic<FaultMsg>("cell/fault", serFault);

    printf("Safety: Configuration complete. Monitoring centrifuge RPM.\n");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int main(void) {
    prvInitialiseHeap();
    static NetworkContext networkContext;

    printf("Cellutron Safety Processor starting (FreeRTOS Win32)...\n");

    Network::GetInstance().Initialize(5013); 
    Network::GetInstance().RegisterIncomingTopic<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", RID_CENTRIFUGE_SPEED, serSpeed);

    TimerHandle_t sysTimer = xTimerCreate("SysTimer", pdMS_TO_TICKS(10), pdTRUE, NULL, [](TimerHandle_t) { Timer::ProcessTimers(); });
    xTimerStart(sysTimer, 0);

    xTaskCreate(vSafetyTask, "Safety", 2048, NULL, 5, NULL);

    vTaskStartScheduler();
    for (;;);
}
