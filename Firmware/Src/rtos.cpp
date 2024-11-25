#include <rtos.hpp>

#include <device.hpp>

#include <FreeRTOS.h>
#include <gpio.h>
#include <stm32f7xx_hal.h>
#include <task.h>

extern "C" void rtos_main()
{
    static const auto INITIAL_DELAY_MS = 100;
    HAL_Delay(INITIAL_DELAY_MS);

    lg::Device::get().initializeDrivers();

    vTaskStartScheduler();
}
