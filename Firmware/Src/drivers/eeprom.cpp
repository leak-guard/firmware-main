#include <drivers/eeprom.hpp>

#include <device.hpp>

#include <array>
#include <cstdint>

#include <stm32f7xx_ll_i2c.h>

namespace lg {

void EepromDriver::initialize()
{
    disableWrites();

    // Test if the EEPROM is actually working

    [[maybe_unused]] std::array<uint8_t, EEPROM_PAGE_SIZE_BYTES> buffer {};

    if (HAL_I2C_Mem_Read(m_i2c, I2C_ADDRESS, 0, sizeof(std::uint16_t),
            buffer.data(), EEPROM_PAGE_SIZE_BYTES, OP_TIMEOUT_MS)
        != HAL_OK) {
        Device::get().setError(Device::ErrorCode::EEPROM_ERROR);
    }
}

bool EepromDriver::readBytes(uint16_t eepromAddress, uint8_t* out, size_t count)
{
    if (eepromAddress + count >= EEPROM_SIZE_BYTES) {
        return false;
    }

    if (count >= MIN_DMA_TRANSFER_SIZE && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        return readBytesDma(eepromAddress, out, count);
    } else {
        return readBytesDirect(eepromAddress, out, count);
    }
}

bool EepromDriver::writeBytes(uint16_t eepromAddress, const uint8_t* in, size_t count)
{
    auto result = performWriteBytes(eepromAddress, in, count);
    return result;
}

bool EepromDriver::writePage(uint16_t pageNumber, const uint8_t* in, size_t count)
{
    auto result = performWritePage(pageNumber, in, count);
    return result;
}

bool EepromDriver::writeMultiplePages(uint16_t startPageNumber, const uint8_t* in, size_t count)
{
    std::uint32_t endAddress = startPageNumber * EEPROM_PAGE_SIZE_BYTES + count;

    if (endAddress >= EEPROM_SIZE_BYTES) {
        return false;
    }

    while (count > 0) {
        size_t toWrite = count;
        if (toWrite > EEPROM_PAGE_SIZE_BYTES) {
            toWrite = EEPROM_PAGE_SIZE_BYTES;
        }

        if (!performWritePage(startPageNumber, in, toWrite)) {
            return false;
        }

        ++startPageNumber;
        in += toWrite;
        count -= toWrite;
    }

    return true;
}

BaseType_t EepromDriver::notifyDmaFinishedFromIsr(bool tx)
{
    auto task = m_suspendedTask;

    if (task) {
        BaseType_t higherPriorityWoken = 0;
        xTaskNotifyFromISR(task, tx ? I2C_TX : I2C_RX, eSetBits, &higherPriorityWoken);
        return higherPriorityWoken;
    }

    return 0;
}

void EepromDriver::disableWrites()
{
    HAL_GPIO_WritePin(m_wpGpioPort, m_wpGpioPin, GPIO_PIN_SET);
}

void EepromDriver::enableWrites()
{
    HAL_GPIO_WritePin(m_wpGpioPort, m_wpGpioPin, GPIO_PIN_RESET);
}

bool EepromDriver::readBytesDirect(uint16_t eepromAddress, uint8_t* out, size_t count)
{
    auto result = HAL_I2C_Mem_Read(m_i2c, I2C_ADDRESS, eepromAddress,
        sizeof(std::uint16_t), out, count, OP_TIMEOUT_MS);
    return result == HAL_OK;
}

bool EepromDriver::readBytesDma(uint16_t eepromAddress, uint8_t* out, size_t count)
{
    m_suspendedTask = xTaskGetCurrentTaskHandle();

    xTaskNotifyWait(0, I2C_RX, nullptr, 0);

    auto result = HAL_I2C_Mem_Read_DMA(m_i2c, I2C_ADDRESS, eepromAddress,
        sizeof(std::uint16_t), out, count);
    if (result != HAL_OK) {
        return false;
    }

    // Wait for the DMA transaction to finish
    xTaskNotifyWait(0, I2C_RX, NULL, portMAX_DELAY);

    return true;
}

bool EepromDriver::performWriteBytes(uint16_t eepromAddress, const uint8_t* in, size_t count)
{
    if (eepromAddress + count >= EEPROM_SIZE_BYTES) {
        return false;
    }

    if (count >= EEPROM_PAGE_SIZE_BYTES) {
        return false;
    }

    if (count >= MIN_DMA_TRANSFER_SIZE && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        return writeBytesDma(eepromAddress, in, count);
    } else {
        return writeBytesDirect(eepromAddress, in, count);
    }
}

bool EepromDriver::performWritePage(uint16_t pageNumber, const uint8_t* in, size_t count)
{
    return performWriteBytes(pageNumber * EEPROM_PAGE_SIZE_BYTES, in, count);
}

// NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)

bool EepromDriver::writeBytesDirect(uint16_t eepromAddress, const uint8_t* in, size_t count)
{
    auto result = HAL_I2C_Mem_Write(m_i2c, I2C_ADDRESS, eepromAddress,
        sizeof(std::uint16_t), const_cast<uint8_t*>(in), count, OP_TIMEOUT_MS);
    waitForOperation();
    return result == HAL_OK;
}

bool EepromDriver::writeBytesDma(uint16_t eepromAddress, const uint8_t* in, size_t count)
{
    m_suspendedTask = xTaskGetCurrentTaskHandle();

    xTaskNotifyWait(0, I2C_TX, nullptr, 0);

    auto result = HAL_I2C_Mem_Write_DMA(m_i2c, I2C_ADDRESS, eepromAddress,
        sizeof(std::uint16_t), const_cast<uint8_t*>(in), count);
    if (result != HAL_OK) {
        return false;
    }

    // Wait for the DMA transaction to finish
    xTaskNotifyWait(0, I2C_TX, NULL, portMAX_DELAY);

    waitForOperation();
    return true;
}

void EepromDriver::waitForOperation()
{
    auto i2c = m_i2c->Instance;
    LL_I2C_SetSlaveAddr(i2c, I2C_ADDRESS);
    LL_I2C_SetTransferRequest(i2c, LL_I2C_REQUEST_READ);
    LL_I2C_SetTransferSize(i2c, 0);
    LL_I2C_DisableAutoEndMode(i2c);

    while (true) {
        LL_I2C_GenerateStartCondition(i2c);

        while (!LL_I2C_IsActiveFlag_TC(i2c) && !LL_I2C_IsActiveFlag_NACK(i2c))
            ;

        if (LL_I2C_IsActiveFlag_NACK(i2c)) {
            if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
                vTaskDelay(1);
            }

            LL_I2C_ClearFlag_NACK(i2c);
        }

        if (LL_I2C_IsActiveFlag_TC(i2c)) {
            LL_I2C_GenerateStopCondition(i2c);
            break;
        }
    }

    LL_I2C_EnableAutoEndMode(i2c);
}

// NOLINTEND(cppcoreguidelines-pro-type-const-cast)

};
