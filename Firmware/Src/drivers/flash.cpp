#include <drivers/flash.hpp>

#include <device.hpp>

#include <FreeRTOS.h>
#include <task.h>

#include <stm32f7xx_hal.h>

// Some of the code is based on:
// https://controllerstech.com/w25q-flash-series-part-7-quadspi-write-read-memory-mapped-mode/

namespace lg {

static constexpr auto CHIP_ERASE_CMD = 0xC7;
static constexpr auto READ_STATUS_REG_CMD = 0x05;
static constexpr auto WRITE_ENABLE_CMD = 0x06;
static constexpr auto VOLATILE_SR_WRITE_ENABLE = 0x50;
static constexpr auto READ_STATUS_REG2_CMD = 0x35;
static constexpr auto WRITE_STATUS_REG2_CMD = 0x31;
static constexpr auto READ_STATUS_REG3_CMD = 0x15;
static constexpr auto WRITE_STATUS_REG3_CMD = 0x11;
static constexpr auto SECTOR_ERASE_CMD = 0x20;
static constexpr auto BLOCK_ERASE_CMD = 0xD8;
static constexpr auto QUAD_IN_FAST_PROG_CMD = 0x32;
static constexpr auto FAST_PROG_CMD = 0x02;
static constexpr auto QUAD_OUT_FAST_READ_CMD = 0x6B;
static constexpr auto DUMMY_CLOCK_CYCLES_READ_QUAD = 8;
static constexpr auto QUAD_IN_OUT_FAST_READ_CMD = 0xEB;
static constexpr auto RESET_ENABLE_CMD = 0x66;
static constexpr auto RESET_EXECUTE_CMD = 0x99;

void FlashDriver::initialize()
{
    if (!resetChip()) {
        return Device::get().setError(Device::ErrorCode::FLASH_ERROR);
    }

    delay(1);

    if (!autoPollingMemReady()) {
        return Device::get().setError(Device::ErrorCode::FLASH_ERROR);
    }

    if (!writeEnable()) {
        return Device::get().setError(Device::ErrorCode::FLASH_ERROR);
    }

    if (!configureQspi()) {
        return Device::get().setError(Device::ErrorCode::FLASH_ERROR);
    }

    delay(100);

    if (!enableMemoryMappedMode()) {
        return Device::get().setError(Device::ErrorCode::FLASH_ERROR);
    }

    if (!testFlash()) {
        return Device::get().setError(Device::ErrorCode::FLASH_ERROR);
    }
}

void* FlashDriver::getBasePtr() const
{
    return reinterpret_cast<void*>(0x90000000);
}

void* FlashDriver::getPageAddress(std::uint32_t page) const
{
    return reinterpret_cast<std::uint8_t*>(getBasePtr()) + page * FLASH_PAGE_SIZE;
}

bool FlashDriver::writePage(std::uint32_t pageId, const std::uint8_t* data, std::size_t size)
{
    if (pageId >= FLASH_PAGE_COUNT) {
        return false;
    }

    if (!disableMemoryMappedMode()) {
        Device::get().setError(Device::ErrorCode::FLASH_ERROR);
        return false;
    }

    if (!doWritePage(pageId, data, size)) {
        Device::get().setError(Device::ErrorCode::FLASH_ERROR);
        enableMemoryMappedMode();
        return false;
    }

    return enableMemoryMappedMode();
}

bool FlashDriver::eraseSector(std::uint32_t sectorId)
{
    if (sectorId >= FLASH_SECTOR_COUNT) {
        return false;
    }

    if (!disableMemoryMappedMode()) {
        Device::get().setError(Device::ErrorCode::FLASH_ERROR);
        return false;
    }

    if (!doEraseSector(sectorId)) {
        Device::get().setError(Device::ErrorCode::FLASH_ERROR);
        enableMemoryMappedMode();
        return false;
    }

    return enableMemoryMappedMode();
}

void FlashDriver::delay(std::uint32_t millis)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(millis);
    } else {
        HAL_Delay(millis);
    }
}

bool FlashDriver::testFlash()
{
    auto ptr = reinterpret_cast<std::uint32_t*>(getBasePtr());
    std::uint32_t value = *ptr;

    return value != 0;
}

bool FlashDriver::resetChip()
{
    QSPI_CommandTypeDef sCommand = { 0 };
    uint32_t temp = 0;

    /* Enable Reset --------------------------- */
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = RESET_ENABLE_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = QSPI_DATA_NONE;
    sCommand.DummyCycles = 0;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    /* Reset Device --------------------------- */
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = RESET_EXECUTE_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = QSPI_DATA_NONE;
    sCommand.DummyCycles = 0;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    for (temp = 0; temp < 500000; temp++) {
        __NOP();
    }

    return true;
}

bool FlashDriver::eraseChip()
{
    QSPI_CommandTypeDef sCommand;

    if (!writeEnable()) {
        return false;
    }

    /* Erasing Sequence --------------------------------- */
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = CHIP_ERASE_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = QSPI_DATA_NONE;
    sCommand.DummyCycles = 0;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_MAX_DELAY) != HAL_OK) {
        return HAL_ERROR;
    }

    if (!autoPollingMemReady()) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

bool FlashDriver::enableMemoryMappedMode()
{
    static constexpr auto OP_DELAY_MS = 100;

    QSPI_CommandTypeDef sCommand;
    QSPI_MemoryMappedTypeDef sMemMappedCfg;

    /* Enable Memory-Mapped mode-------------------------------------------------- */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = QUAD_IN_OUT_FAST_READ_CMD;
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;
    sCommand.AddressMode = QSPI_ADDRESS_4_LINES;
    sCommand.Address = 0;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_4_LINES;
    sCommand.AlternateBytes = 0xFF;
    sCommand.AlternateBytesSize = 1;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    sCommand.DataMode = QSPI_DATA_4_LINES;
    sCommand.NbData = 0;
    sCommand.DummyCycles = 4;

    sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    sMemMappedCfg.TimeOutPeriod = 0;

    if (HAL_QSPI_MemoryMapped(m_qspi, &sCommand, &sMemMappedCfg) != HAL_OK) {
        return false;
    }

    delay(OP_DELAY_MS);
    return true;
}

bool FlashDriver::disableMemoryMappedMode()
{
    return HAL_QSPI_Abort(m_qspi) == HAL_OK;
}

bool FlashDriver::writeEnable()
{
    QSPI_CommandTypeDef sCommand = { 0 };
    QSPI_AutoPollingTypeDef sConfig = { 0 };

    /* Enable write operations ------------------------------------------ */
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = WRITE_ENABLE_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = QSPI_DATA_NONE;
    sCommand.DummyCycles = 0;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {

        return false;
    }

    /* Configure automatic polling mode to wait for write enabling ---- */
    sConfig.Match = 0x02;
    sConfig.Mask = 0x02;
    sConfig.MatchMode = QSPI_MATCH_MODE_AND;
    sConfig.StatusBytesSize = 1;
    sConfig.Interval = 0x10;
    sConfig.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

    sCommand.Instruction = READ_STATUS_REG_CMD;
    sCommand.DataMode = QSPI_DATA_1_LINE;

    if (HAL_QSPI_AutoPolling(m_qspi, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    return true;
}

bool FlashDriver::autoPollingMemReady()
{
    QSPI_CommandTypeDef sCommand = { 0 };
    QSPI_AutoPollingTypeDef sConfig = { 0 };

    /* Configure automatic polling mode to wait for memory ready ------ */
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = READ_STATUS_REG_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = QSPI_DATA_1_LINE;
    sCommand.DummyCycles = 0;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    sConfig.Match = 0x00;
    sConfig.Mask = 0x01;
    sConfig.MatchMode = QSPI_MATCH_MODE_AND;
    sConfig.StatusBytesSize = 1;
    sConfig.Interval = 0x10;
    sConfig.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_QSPI_AutoPolling(m_qspi, &sCommand, &sConfig, HAL_MAX_DELAY)) {
        return false;
    }

    return true;
}

bool FlashDriver::configureQspi()
{
    QSPI_CommandTypeDef sCommand = { 0 };
    uint8_t reg {};

    /* Read Volatile Configuration register 2 --------------------------- */
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = READ_STATUS_REG2_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = QSPI_DATA_1_LINE;
    sCommand.DummyCycles = 0;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    sCommand.NbData = 1;

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    if (HAL_QSPI_Receive(m_qspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    /* Enable Volatile Write operations ---------------------------------------- */
    sCommand.DataMode = QSPI_DATA_NONE;
    sCommand.Instruction = VOLATILE_SR_WRITE_ENABLE;

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    /* Write Volatile Configuration register 2 (QE = 1) -- */
    sCommand.DataMode = QSPI_DATA_1_LINE;
    sCommand.Instruction = WRITE_STATUS_REG2_CMD;
    reg |= 2; // QE bit

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    if (HAL_QSPI_Transmit(m_qspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    /* Read Volatile Configuration register 3 --------------------------- */
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = READ_STATUS_REG3_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = QSPI_DATA_1_LINE;
    sCommand.DummyCycles = 0;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    sCommand.NbData = 1;

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    if (HAL_QSPI_Receive(m_qspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    /* Write Volatile Configuration register 2 (DRV1:2 = 00) -- */
    sCommand.Instruction = WRITE_STATUS_REG3_CMD;
    reg &= 0x9f; // DRV1:2 bit

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    if (HAL_QSPI_Transmit(m_qspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    return true;
}

bool FlashDriver::doWritePage(
    std::uint32_t pageId, const std::uint8_t* data, std::size_t count)
{
    QSPI_CommandTypeDef sCommand;

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = QUAD_IN_FAST_PROG_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    sCommand.DataMode = QSPI_DATA_4_LINES;
    sCommand.NbData = count;
    sCommand.Address = pageId * FLASH_PAGE_SIZE;
    sCommand.DummyCycles = 0;

    if (!writeEnable()) {
        return false;
    }

    if (HAL_QSPI_Command(m_qspi, &sCommand,
            HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
        != HAL_OK) {

        return false;
    }

    // NOLINTBEGIN(*-const-cast)
    if (HAL_QSPI_Transmit(m_qspi, const_cast<std::uint8_t*>(data),
            HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
        != HAL_OK) {

        return false;
    }

    if (!autoPollingMemReady()) {
        return false;
    }
    // NOLINTEND(*-const-cast)

    return true;
}

bool FlashDriver::doEraseSector(std::uint32_t sectorId)
{
    QSPI_CommandTypeDef sCommand;

    /* Erasing Sequence -------------------------------------------------- */
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = SECTOR_ERASE_CMD;
    sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    sCommand.DataMode = QSPI_DATA_NONE;
    sCommand.DummyCycles = 0;

    sCommand.Address = sectorId * FLASH_SECTOR_SIZE;

    if (!writeEnable()) {
        return false;
    }

    if (HAL_QSPI_Command(m_qspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    if (!autoPollingMemReady()) {
        return false;
    }

    return true;
}

};
