#include <rtos.hpp>

#include <device.hpp>

#include <FreeRTOS.h>
#include <gpio.h>
#include <stm32f7xx_hal.h>
#include <task.h>

TaskHandle_t blink1_task_handle;
StaticTask_t blink1_task_tcb;
uint32_t blink1_task_stack[64], blink2_task_stack[64];

void blink1_task(void* params)
{
    while (true) {
        HAL_GPIO_TogglePin(LED_VALVE_GPIO_Port, LED_VALVE_Pin);
        vTaskDelay(500);
    }
}

extern "C" void rtos_main()
{
    static const auto INITIAL_DELAY_MS = 100;
    HAL_Delay(INITIAL_DELAY_MS);

    lg::Device::get().initializeDrivers();

    blink1_task_handle = xTaskCreateStatic(
        blink1_task /* Task function */,
        "BLINK1" /* Task name */,
        64 /* Stack size */,
        NULL /* parameters */,
        1 /* Prority */,
        blink1_task_stack /* Task stack address */,
        &blink1_task_tcb /* Task control block */
    );

    vTaskStartScheduler();
}
