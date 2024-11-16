#pragma once
#include <u8g2.h>

#include <array>
#include <cstdint>

#include <stm32f7xx_hal.h>

namespace lg {

class OledDriver {
public:
    static constexpr auto WIDTH = 128;
    static constexpr auto HEIGHT = 64;

    struct DisplayInfo {
        u8g2_t u8g2 {};
        I2C_HandleTypeDef* i2c {};
    };

    explicit OledDriver(I2C_HandleTypeDef* i2c)
        : m_displayInfo({ {}, i2c }) {};

    void initialize();
    void sendBufferDma();

    [[nodiscard]] u8g2_t* getU8g2Handle() { return &m_displayInfo.u8g2; };

private:
    DisplayInfo m_displayInfo {};
    std::array<std::uint8_t, WIDTH * HEIGHT / 8> m_txBuffer {};
};

};
