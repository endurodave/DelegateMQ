/**
  ******************************************************************************
  * @file           : main.cpp
  * @brief          : DelegateMQ + FreeRTOS Integration Demo on STM32F4-Discovery
  * @version        : 1.1.0
  *
  * @details
  * This application demonstrates the integration of the DelegateMQ messaging
  * library with the FreeRTOS real-time operating system on an STM32F407.
  *
  * Tests included:
  * 1. Unicast Delegates (Free functions)
  * 2. Multicast Delegates (Broadcasts)
  * 3. Thread-Safe Delegates (Mutex protected)
  * 4. Asynchronous Timers
  * 5. Signals & Slots (RAII Connection Management)
  *
  * ============================================================================
  * LED STATUS INDICATORS
  * ============================================================================
  * - ORANGE (LD3):  System Initialization.
  * - GREEN  (LD4):  FreeRTOS Scheduler Started.
  * - RED    (LD5):  ERROR / FAULT.
  * - BLUE   (LD6):  HEARTBEAT (Blinks if successful).
  *
  ******************************************************************************
  */
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "DelegateMQ.h"
#include <memory> // Required for std::make_shared
#include <stdio.h>

using namespace dmq;

// ============================================================================
// CONFIGURATION
// ============================================================================

// Set to 1 to enable printf via SWV/ITM
#define ENABLE_LOGGING  1

#if ENABLE_LOGGING
    #include <stdio.h>
    #define PRINTF(...)  printf(__VA_ARGS__)
#else
    #define PRINTF(...)  do {} while (0)
#endif

// Redirects printf to the SWV Data Console
extern "C" int __io_putchar(int ch) {
    // ITM_SendChar is a CMSIS function that sends data via the SWO pin
    ITM_SendChar(ch);
    return ch;
}

// ============================================================================
// DELEGATEMQ & FREERTOS GLOBALS
// ============================================================================

#if defined(DMQ_THREAD_FREERTOS)
    // Defines the frequency for the DelegateMQ timer tick
    #define mainTIMER_FREQUENCY_MS pdMS_TO_TICKS(10UL)

    static TimerHandle_t xSystemTimer = nullptr;

    // Called by FreeRTOS Timer Task to drive DelegateMQ timers
    static void TimerCallback(TimerHandle_t xTimerHandle) {
        Timer::ProcessTimers();
    }
#endif

// ============================================================================
// PROTOTYPES
// ============================================================================
static void SystemClock_Config(void);
static void Error_Handler(void);
static void ExecuteAllTests(void);

// ============================================================================
// TEST CALLBACKS & CLASSES
// ============================================================================

// A simple free function callback
void FreeFunction(int val) {
    PRINTF("  [Callback] FreeFunction: %d\n", val);
}

// A class method callback
class TestHandler {
public:
    void MemberFunc(int val) {
        PRINTF("  [Callback] MemberFunc: %d\n", val);
    }

    void OnTimerExpired() {
        PRINTF("  [Callback] Timer Expired!\n");
        BSP_LED_Toggle(LED6); // Toggle Blue LED
    }
};

// ============================================================================
// FREERTOS TASKS
// ============================================================================

// The task that runs our C++ tests
void RunTestsTask(void* pvParameters) {
    BSP_LED_On(LED4); // Green LED ON = Task Started

    PRINTF("--- RunTestsTask Started ---\n");

    // 1. Sanity Check: Test vTaskDelay
    PRINTF("Waiting 1 second (Testing SysTick)...\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 2. Run the actual C++ logic
    PRINTF("Running DelegateMQ Tests...\n");
    ExecuteAllTests();

    PRINTF("Tests Complete. Task entering idle loop.\n");
    BSP_LED_Off(LED4); // Green OFF = Finished

    // Blink Blue LED forever to show system is alive
    for(;;) {
        BSP_LED_Toggle(LED6);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ============================================================================
// MAIN
// ============================================================================
int main(void)
{
  // 1. Hardware Initialization
  HAL_Init();
  SystemClock_Config();

  // CRITICAL: FreeRTOS requires Priority Group 4 on STM32
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  // 2. Initialize LEDs
  BSP_LED_Init(LED3); // Orange
  BSP_LED_Init(LED4); // Green
  BSP_LED_Init(LED5); // Red
  BSP_LED_Init(LED6); // Blue

  BSP_LED_On(LED3);   // Orange ON = Main Reached

// Uncommnet to run stress tests
//#define STRESS_TEST
#ifdef STRESS_TEST
  extern void StartStressTest();
  StartStressTest();

#else
  // 3. Initialize DelegateMQ System Timer (if using FreeRTOS mode)
  #if defined(DMQ_THREAD_FREERTOS)
      xSystemTimer = xTimerCreate("SysTimer", mainTIMER_FREQUENCY_MS, pdTRUE, NULL, TimerCallback);
      if (xSystemTimer) {
          xTimerStart(xSystemTimer, 0);
      } else {
          PRINTF("Error: Failed to create xSystemTimer\n");
          Error_Handler();
      }
  #endif

  // 4. Create the main application task
  BaseType_t status = xTaskCreate(RunTestsTask, "MainTask", 2048, NULL, 2, NULL);
  if (status != pdPASS) {
      PRINTF("Error: Failed to create MainTask (Heap too small?)\n");
      Error_Handler();
  }
#endif

  // 5. Start the Scheduler (Should never return)
  PRINTF("Starting Scheduler...\n");
  vTaskStartScheduler();

  // 6. We only get here if the Scheduler failed to start (usually Out of Heap)
  while (1)
  {
      BSP_LED_Toggle(LED5); // Fast Red Blink = Error
      HAL_Delay(100);
  }
}

// ============================================================================
// TEST LOGIC
// ============================================================================
static void ExecuteAllTests(void) {
    // Disable buffering to ensure printf prints immediately
    setvbuf(stdout, NULL, _IONBF, 0);

    PRINTF("\n=== STARTING DELEGATE MQ TESTS ===\n");

    // --- Test 1: Unicast ---
    PRINTF("[1] Unicast Delegate\n");
    UnicastDelegate<void(int)> unicast;
    unicast = MakeDelegate(FreeFunction);
    unicast(100);

    // --- Test 2: Multicast ---
    PRINTF("[2] Multicast Delegate\n");
    MulticastDelegate<void(int)> multicast;
    TestHandler handler;
    multicast += MakeDelegate(&handler, &TestHandler::MemberFunc);
    multicast += MakeDelegate(FreeFunction);
    multicast(200);

    // --- Test 3: Thread-Safe (Only valid if OS is defined) ---
    #if defined(DMQ_THREAD_FREERTOS)
    PRINTF("[3] Thread-Safe Delegate\n");
    MulticastDelegateSafe<void(int)> safeMulticast;
    safeMulticast += MakeDelegate(FreeFunction);
    safeMulticast(300);

    // --- Test 4: Timer ---
    PRINTF("[4] Timer Delegate (Wait 500ms)\n");
    Timer myTimer;
    (*myTimer.OnExpired) += MakeDelegate(&handler, &TestHandler::OnTimerExpired);

    myTimer.Start(std::chrono::milliseconds(200));

    // Block this task to let the timer expire
    vTaskDelay(pdMS_TO_TICKS(500));
    myTimer.Stop();
    #endif

    // --- Test 5: Signals & Slots (RAII) ---
    PRINTF("[5] Signals & Slots (RAII)\n");
    auto signal = std::make_shared<Signal<void(int)>>();
    {
        // Connect a delegate. 'ScopedConnection' manages the lifetime.
        ScopedConnection conn = signal->Connect(MakeDelegate(FreeFunction));

        PRINTF("  Firing Signal inside scope (Expect Callback: 555)\n");
        // Invoke the signal
        (*signal)(555);

        PRINTF("  Exiting scope (Connection will auto-disconnect)...\n");
    }
    // 'conn' has been destroyed here.
    PRINTF("  Firing Signal outside scope (Expect NO Callback)\n");
    (*signal)(666);

    PRINTF("=== TESTS FINISHED ===\n");
}

// ============================================================================
// SYSTEM BOILERPLATE
// ============================================================================

/**
  * @brief  System Clock Configuration
  * The system Clock is configured as follow :
  * System Clock source            = PLL (HSE)
  * SYSCLK(Hz)                     = 168000000
  * HCLK(Hz)                       = 168000000
  * AHB Prescaler                  = 1
  * APB1 Prescaler                 = 4
  * APB2 Prescaler                 = 2
  * HSE Frequency(Hz)              = 8000000
  * PLL_M                          = 8
  * PLL_N                          = 336
  * PLL_P                          = 2
  * PLL_Q                          = 7
  * VDD(V)                         = 3.3
  * Main regulator output voltage  = Scale1 mode
  * Flash Latency(WS)              = 5
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  
  /* Select PLL as system clock source and configure clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  /* Enable the Flash prefetch if supported */
  if (HAL_GetREVID() >= 0x1001)
  {
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
  }
}

static void Error_Handler(void)
{
  /* Turn LED5 (Red) on */
  BSP_LED_On(LED5);
  while(1) {}
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
  while (1) {}
}
#endif
