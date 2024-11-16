#include "main.h"
#include "stm32f7xx_hal_gpio.h"
#include "stm32f7xx_hal_i2c.h"
#include "u8g2.h"
#include <drivers/oled.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

#include <FreeRTOS.h>
#include <i2c.h>
#include <stm32f7xx_hal.h>
#include <task.h>

#include <cmsis_compiler.h>

namespace lg {

static constexpr uint8_t I2C_ADDRESS = 0x78;
static constexpr uint8_t SSD1306_COLUMNADDR = 0x21;
static constexpr uint8_t SSD1306_PAGEADDR = 0x22;
static constexpr uint8_t SSD1306_SETSTARTLINE = 0x40;

static uint8_t u8x8_byte_stm32hal_hw_i2c(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
    auto display = reinterpret_cast<OledDriver::DisplayInfo*>(u8x8);

    static std::array<uint8_t, 256> buffer {};
    static uint8_t buf_idx;
    uint8_t* data = nullptr;

    switch (msg) {
    case U8X8_MSG_BYTE_SEND: {
        data = reinterpret_cast<uint8_t*>(arg_ptr);
        while (arg_int > 0) {
            buffer.at(buf_idx++) = *data;
            data++;
            arg_int--;
        }
    } break;
    case U8X8_MSG_BYTE_INIT:
        break;
    case U8X8_MSG_BYTE_SET_DC:
        break;
    case U8X8_MSG_BYTE_START_TRANSFER: {
        buf_idx = 0;
    } break;
    case U8X8_MSG_BYTE_END_TRANSFER: {
        uint8_t iaddress = I2C_ADDRESS;
        auto result = HAL_I2C_Master_Transmit(
            display->i2c, I2C_ADDRESS, &buffer[0], buf_idx, 1000u);

        if (result != HAL_OK) {
            // TODO: device error
        }
    } break;
    default:
        return 0;
    }
    return 1;
}

static uint8_t psoc_gpio_and_delay_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;
    case U8X8_MSG_DELAY_NANO: {
        for (volatile uint32_t i = 1; i <= arg_int * 10; i = i + 1)
            ;
        break;
    }

    case U8X8_MSG_DELAY_10MICRO:
        break;
    case U8X8_MSG_DELAY_100NANO:
        break;
    case U8X8_MSG_DELAY_MILLI:
        HAL_Delay(arg_int);
        break;
    case U8X8_MSG_DELAY_I2C:
        break;
    case U8X8_MSG_GPIO_I2C_CLOCK:
        break;
    default:
        u8x8_SetGPIOResult(u8x8, 1);
        break;
    }
    return 1;
}

void OledDriver::initialize()
{
    auto u8g2 = &m_displayInfo.u8g2;
    auto u8x8 = reinterpret_cast<u8x8_t*>(u8g2);

    ::u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        u8g2,
        U8G2_R0,
        u8x8_byte_stm32hal_hw_i2c,
        psoc_gpio_and_delay_cb);
    u8x8_SetI2CAddress(u8x8, I2C_ADDRESS);
    ::u8x8_InitDisplay(u8x8);
    ::u8x8_SetPowerSave(u8x8, false);
    ::u8g2_ClearDisplay(u8g2);
    ::u8g2_SetDrawColor(u8g2, 1);
    ::u8g2_SetFontDirection(u8g2, 0);

    ::u8g2_ClearBuffer(u8g2);
}

static inline HAL_StatusTypeDef oledSendCommand(I2C_HandleTypeDef* i2c, std::uint8_t cmd)
{
    return HAL_I2C_Mem_Write(i2c, I2C_ADDRESS, 0, 1, &cmd, 1, 100);
}

void OledDriver::sendBufferDma()
{
    while (HAL_I2C_GetState(m_displayInfo.i2c) != HAL_I2C_STATE_READY) {
        vTaskDelay(1);
    }

    const auto buffer = u8g2_GetBufferPtr(&m_displayInfo.u8g2);
    const auto size = m_txBuffer.size();

    std::copy(buffer, buffer + size, m_txBuffer.data());

    oledSendCommand(m_displayInfo.i2c, SSD1306_PAGEADDR);
    oledSendCommand(m_displayInfo.i2c, 0x00);
    oledSendCommand(m_displayInfo.i2c, 0xFF);
    oledSendCommand(m_displayInfo.i2c, SSD1306_COLUMNADDR);
    oledSendCommand(m_displayInfo.i2c, 0x00);
    oledSendCommand(m_displayInfo.i2c, WIDTH - 1);

    HAL_I2C_Mem_Write_DMA(m_displayInfo.i2c,
        I2C_ADDRESS, SSD1306_SETSTARTLINE, 1, m_txBuffer.data(), m_txBuffer.size());
}

};
