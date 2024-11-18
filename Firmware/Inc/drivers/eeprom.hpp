#pragma once
#include <FreeRTOS.h>
#include <task.h>

#include <stm32f7xx_hal.h>

namespace lg {

class EepromDriver {
public:
    // 24LC512 EEPROM

    static constexpr auto I2C_ADDRESS = 0xA0;
    static constexpr auto EEPROM_SIZE_BYTES = 65536U;
    static constexpr auto EEPROM_PAGE_SIZE_BYTES = 128U;

    explicit EepromDriver(I2C_HandleTypeDef* i2c, GPIO_TypeDef* wpPort, uint16_t wpPin)
        : m_i2c(i2c)
        , m_wpGpioPort(wpPort)
        , m_wpGpioPin(wpPin)
    {
    }

    void initialize();
    bool readBytes(uint16_t eepromAddress, uint8_t* out, size_t count);

    template <typename T>
    bool readObject(uint16_t eepromAddress, T& out)
    {
        return readBytes(eepromAddress, reinterpret_cast<uint8_t*>(&out), sizeof(T));
    }

    bool writeBytes(uint16_t eepromAddress, const uint8_t* in, size_t count);
    bool writePage(uint16_t pageNumber, const uint8_t* in, size_t count);
    bool writeMultiplePages(uint16_t startPageNumber, const uint8_t* in, size_t count);

    template <typename T>
    bool writeObject(uint16_t startPageNumber, const T& in)
    {
        return writeMultiplePages(startPageNumber,
            reinterpret_cast<const uint8_t*>(&in), sizeof(T));
    }

    BaseType_t notifyDmaFinishedFromIsr(bool tx);

private:
    static constexpr auto I2C_TX = (1 << 1);
    static constexpr auto I2C_RX = (1 << 2);
    static constexpr auto MIN_DMA_TRANSFER_SIZE = 64U;
    static constexpr auto OP_TIMEOUT_MS = 1000;

    void disableWrites();
    void enableWrites();

    bool readBytesDirect(uint16_t eepromAddress, uint8_t* out, size_t count);
    bool readBytesDma(uint16_t eepromAddress, uint8_t* out, size_t count);
    bool performWriteBytes(uint16_t eepromAddress, const uint8_t* in, size_t count);
    bool performWritePage(uint16_t pageNumber, const uint8_t* in, size_t count);
    bool writeBytesDirect(uint16_t eepromAddress, const uint8_t* in, size_t count);
    bool writeBytesDma(uint16_t eepromAddress, const uint8_t* in, size_t count);

    I2C_HandleTypeDef* m_i2c;
    GPIO_TypeDef* m_wpGpioPort;
    uint16_t m_wpGpioPin;

    volatile TaskHandle_t m_suspendedTask {};
};

};
