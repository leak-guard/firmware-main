#include <rtos.hpp>

#include <device.hpp>

#include <FreeRTOS.h>
#include <gpio.h>
#include <stm32f7xx_hal.h>
#include <task.h>

extern "C" void rtos_main()
{
#ifdef _DEBUG
    // NOLINTBEGIN(*)

    // https://community.st.com/t5/stm32-mcus-products/
    // stm32f7-debugging-stepping-impossible-ide-jumps-into-interrupts/m-p/447639/highlight/true#M143597

    DBGMCU->APB1FZ = 0x7E01BFF;
    DBGMCU->APB2FZ = 0x70003;

// NOLINTEND(*)
#endif

    static const auto INITIAL_DELAY_MS = 100;
    HAL_Delay(INITIAL_DELAY_MS);

    lg::Device::get().initializeDrivers();

    vTaskStartScheduler();
}
