/**
  ******************************************************************************
  * @file           : main.cpp
  * @brief          : DelegateMQ + FreeRTOS Integration Demo on STM32F4-Discovery
  * @version        : 1.2.0
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
  * 6. Async Delegates (Cross-Thread Dispatch)
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
#include <stdio.h>

// ============================================================================
// TEST SELECTION (Uncomment ONE)
// ============================================================================
#define RUN_SIMPLE_TESTS
//#define RUN_STRESS_TESTS
// #define RUN_NETWORK_TESTS

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
extern void StartSimpleTests();
extern void StartStressTests();
extern void StartNetworkTests();

// Global Handles
UART_HandleTypeDef huart6;
NetworkEngine* g_netEngine = nullptr;

// ============================================================================
// SYSTEM CONFIGURATION (Clock, GPIO, UART, Timer)
// ============================================================================
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART6_UART_Init(void);
static void Error_Handler(void);

#if defined(DMQ_THREAD_FREERTOS)
    #define mainTIMER_FREQUENCY_MS pdMS_TO_TICKS(10UL)
    static void TimerCallback(TimerHandle_t xTimerHandle) { Timer::ProcessTimers(); }
#endif

// ============================================================================
// MAIN TASK
// ============================================================================
void MainTask(void* pvParameters) {
    BSP_LED_On(LED4); // Green LED ON = Task Started
    printf("--- MainTask Started ---\n");

    // Initialize DelegateMQ System Timer
    #if defined(DMQ_THREAD_FREERTOS)
        TimerHandle_t xSystemTimer = xTimerCreate("SysTimer", mainTIMER_FREQUENCY_MS, pdTRUE, NULL, TimerCallback);
        if (xSystemTimer) xTimerStart(xSystemTimer, 0);
    #endif

    // --- RUN SELECTED TEST SUITE ---
    #if defined(RUN_SIMPLE_TESTS)
        printf("Mode: SIMPLE TESTS\n");
        StartSimpleTests();
    #elif defined(RUN_STRESS_TESTS)
        printf("Mode: STRESS TESTS\n");
        StartStressTests();
    #elif defined(RUN_NETWORK_TESTS)
        printf("Mode: NETWORK TESTS\n");
        StartNetworkTests();
    #else
        #error "Please select a test mode in main.cpp"
    #endif

    // Idle Loop
    for(;;) {
        BSP_LED_Toggle(LED6); // Blue Heartbeat
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART6_UART_Init();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    BSP_LED_Init(LED3); BSP_LED_Init(LED4); BSP_LED_Init(LED5); BSP_LED_Init(LED6);
    BSP_LED_On(LED3); // Orange ON = Init Complete

    // Create Main Task
    xTaskCreate(MainTask, "MainTask", 2048, NULL, 2, NULL);

    vTaskStartScheduler();

    while (1) { BSP_LED_Toggle(LED5); HAL_Delay(100); } // Error Trap
}

// ============================================================================
// PERIPHERAL INITIALIZATION (Restored)
// ============================================================================

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
}

/**
  * @brief UART MSP Initialization
  * This is called implicitly by HAL_UART_Init
  */
extern "C" void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance == USART6)
  {
    /* 1. Enable Peripheral Clocks */
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* 2. Configure GPIO Pins: PC6 (TX) and PC7 (RX) */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6; // AF8 is USART6 for F4 series
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  }
}

extern "C" void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{
  if(uartHandle->Instance == USART6)
  {
    /* Disable Peripheral Clock */
    __HAL_RCC_USART6_CLK_DISABLE();

    /* DeInit GPIO Pins: PC6, PC7 */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6 | GPIO_PIN_7);
  }
}

// Redirect printf to ITM (SWV)
extern "C" int __io_putchar(int ch) {
    ITM_SendChar(ch);
    return ch;
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

/* USER CODE BEGIN 4 */
extern "C" {

// ✅ FIX: Increase Idle Stack to 256 or 512 words (was 128)
// Small stacks cause crashes during vTaskDelete or Context Switches
#define IDLE_STACK_SIZE 512
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ IDLE_STACK_SIZE ]; // Increased

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = IDLE_STACK_SIZE;
}

// ✅ FIX: Increase Timer Stack to 512 words (was 256)
// Timer task handles software timers and can be stack-heavy
#define TIMER_STACK_SIZE 512
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ TIMER_STACK_SIZE ];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = TIMER_STACK_SIZE;
}

} // extern "C"

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // TRAP: The system has run out of stack space for a specific task.
    // Look at pcTaskName to see which thread failed.
    printf("CRITICAL: Stack Overflow in task: %s\n", pcTaskName);

    // Stop here for debugger
    taskDISABLE_INTERRUPTS();
    for(;;);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
  while (1) {}
}
#endif
