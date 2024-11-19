#include <device.hpp>
#include <optional>

#include <gpio.h>
#include <i2c.h>
#include <usart.h>

#include <stm32f7xx_ll_i2c.h>

namespace lg {

std::optional<Device> Device::m_instance;

Device::Device()
    : m_eepromDriver(&hi2c2, EEPROM_WP_GPIO_Port, EEPROM_WP_Pin)
    , m_espDriver(&huart1)
    , m_oledDriver(&hi2c1)
{
}

Device& Device::get()
{
    if (!m_instance.has_value()) {
        m_instance.emplace();
    }

    return m_instance.value();
}

void Device::initializeDrivers()
{
    m_eepromDriver->initialize();
    m_espDriver.initialize();
    m_oledDriver->initialize();

    m_configService->initialize();
    m_networkManager->initialize();
    m_server->initialize();
    m_uiService->initialize();
}

void Device::setError(ErrorCode code)
{
    m_error = code;
}

BaseType_t Device::notifyEepromFromIsr(bool tx)
{
    if (hi2c2.State == HAL_I2C_STATE_READY) {
        return m_eepromDriver->notifyDmaFinishedFromIsr(tx);
    }

    return 0;
}

};

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while,cppcoreguidelines-pro-type-cstyle-cast)

extern "C" void I2C2_EV_IRQHandler(void)
{
    bool tc = LL_I2C_IsActiveFlag_TC(hi2c2.Instance);
    bool tx = HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_BUSY_TX;

    HAL_I2C_EV_IRQHandler(&hi2c2);

    if (tc) {
        auto higherPriorityWoken = lg::Device::get().notifyEepromFromIsr(tx);
        portYIELD_FROM_ISR(higherPriorityWoken);
    }
}

// NOLINTEND(cppcoreguidelines-avoid-do-while,cppcoreguidelines-pro-type-cstyle-cast)
