/**
 * @file controller/main.cpp
 * @brief Controller CPU — Process Sequencing & Hardware Control
 * 
 * ROLE:
 * This node is the "Brain" of the instrument. It orchestrates the complex 
 * automated cell-processing sequence. It is responsible for:
 * 1. Running the high-level CellProcess state machine.
 * 2. Simulating centrifuge physics via a secondary state machine.
 * 3. Commanding hardware actuators (valves/pumps) via blocking synchronous APIs.
 * 4. Responding to user commands and safety fault overrides.
 * 
 * ENVIRONMENT: FreeRTOS (Win32 Simulator)
 */

#include "DelegateMQ.h"
#include "process/CellProcess.h"
#include "process/Centrifuge.h"
#include "actuators/Actuators.h"
#include "sensors/Sensors.h"
#include "messages/CentrifugeStatusMsg.h"
#include "messages/CentrifugeSpeedMsg.h"
#include "messages/StartProcessMsg.h"
#include "messages/StopProcessMsg.h"
#include "messages/RunStatusMsg.h"
#include "messages/FaultMsg.h"
#include "messages/ActuatorStatusMsg.h"
#include "messages/SensorStatusMsg.h"
#include "RemoteConfig.h"
#include "Network.h"
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
static Thread* g_thread = nullptr;
static Centrifuge* g_centrifuge = nullptr;
static CellProcess* g_process = nullptr;

static dmq::ScopedConnection g_startConn;
static dmq::ScopedConnection g_stopConn;
static dmq::ScopedConnection g_faultConn;
static dmq::ScopedConnection g_statusConn;

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
extern "C" void vApplicationMallocFailedHook(void) { printf("FreeRTOS: Malloc Failed!\n"); for (;;); }
extern "C" void vApplicationDaemonTaskStartupHook(void) {}
extern "C" void vAssertCalled(unsigned long ulLine, const char* const pcFileName) {
    printf("FreeRTOS: ASSERT FAIL at %s:%lu\n", pcFileName, ulLine);
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
// Tasks
// ---------------------------------------------------------------------------

static void vControllerTask(void* /*pvParams*/) {
    printf("Controller: Task starting setup...\n");

    // 1. Create Communication/Sequencing Thread with Watchdog (Standardized)
    g_thread = new Thread("CommThread");
    if (!g_thread->CreateThread(std::chrono::seconds(2))) {
        printf("Controller: ERROR - Failed to create comm thread!\n");
        return;
    }

    // 2. Instantiate state machines
    g_centrifuge = new Centrifuge();
    g_process = new CellProcess(*g_centrifuge);
    g_centrifuge->SetThread(*g_thread);
    g_process->SetThread(*g_thread);

    // 3. Initialize Subsystems
    Actuators::GetInstance().Initialize();
    Sensors::GetInstance().Initialize();

    // 4. SUBSCRIBE LOCALLY FIRST
    g_startConn = DataBus::Subscribe<StartProcessMsg>("cell/cmd/run", [](StartProcessMsg msg) {
        printf("Controller: >>>> RECEIVED START COMMAND <<<<\n");
        if (g_process) g_process->StartProcess();
    }, g_thread);

    g_stopConn = DataBus::Subscribe<StopProcessMsg>("cell/cmd/abort", [](StopProcessMsg msg) {
        printf("Controller: >>>> RECEIVED ABORT COMMAND <<<<\n");
        if (g_process) g_process->AbortProcess();
    }, g_thread);

    g_faultConn = DataBus::Subscribe<FaultMsg>("cell/fault", [](FaultMsg msg) {
        printf("Controller: >>>> CRITICAL FAULT RECEIVED FROM SAFETY NODE <<<<\n");
        if (g_process) g_process->GenerateFault();
    }, g_thread);

    // 5. Initialize Network
    Network::GetInstance().Initialize(5011); 
    Network::GetInstance().RegisterIncomingTopic<StartProcessMsg>("cell/cmd/run", RID_START_PROCESS, serStart);
    Network::GetInstance().RegisterIncomingTopic<StopProcessMsg>("cell/cmd/abort", RID_STOP_PROCESS, serStop);
    Network::GetInstance().RegisterIncomingTopic<FaultMsg>("cell/fault", RID_FAULT_EVENT, serFault);

    // Setup Outgoing Topics
    Network::GetInstance().AddRemoteNode("GUI", "127.0.0.1", 5010);
    
    Network::GetInstance().MapTopicToRemote("cell/status/run", RID_RUN_STATUS, "GUI");
    Network::GetInstance().RegisterOutgoingTopic<RunStatusMsg>("cell/status/run", serRun);

    Network::GetInstance().MapTopicToRemote("cell/cmd/centrifuge_speed", RID_CENTRIFUGE_SPEED, "GUI");
    Network::GetInstance().RegisterOutgoingTopic<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", serSpeed);

    // Hardware status logging redirection to GUI
    Network::GetInstance().MapTopicToRemote("hw/status/actuator", RID_ACTUATOR_STATUS, "GUI");
    Network::GetInstance().RegisterOutgoingTopic<ActuatorStatusMsg>("hw/status/actuator", serActuator);

    Network::GetInstance().MapTopicToRemote("hw/status/sensor", RID_SENSOR_STATUS, "GUI");
    Network::GetInstance().RegisterOutgoingTopic<SensorStatusMsg>("hw/status/sensor", serSensor);

    // Safety Node (listens on 5013)
    Network::GetInstance().AddRemoteNode("Safety", "127.0.0.1", 5013);
    Network::GetInstance().MapTopicToRemote("cell/cmd/centrifuge_speed", RID_CENTRIFUGE_SPEED, "Safety");

    printf("Controller: Configuration complete. System ready.\n");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    prvInitialiseHeap();
    static NetworkContext networkContext;

    printf("Cellutron Controller Processor starting (FreeRTOS Win32)...\n");

    TimerHandle_t sysTimer = xTimerCreate("SysTimer", pdMS_TO_TICKS(10), pdTRUE, NULL, [](TimerHandle_t) { Timer::ProcessTimers(); });
    xTimerStart(sysTimer, 0);

    xTaskCreate(vControllerTask, "Controller", 4096, NULL, 5, NULL);

    vTaskStartScheduler();
    for (;;);
}
