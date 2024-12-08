#include "drivers/sx1278.hpp"
#include "tim.h"
#include <cstring>

namespace lg {

volatile uint32_t Sx1278Driver::m_timerTick { 0 };

Sx1278Driver::Sx1278Driver(SPI_HandleTypeDef* spi,
    GPIO_TypeDef* nssPort, uint16_t nssPin,
    GPIO_TypeDef* resetPort, uint16_t resetPin,
    GPIO_TypeDef* dio0Port, uint16_t dio0Pin,
    TIM_HandleTypeDef* loraTimer)
    : m_spi(spi)
    , m_nssPort(nssPort)
    , m_nssPin(nssPin)
    , m_resetPort(resetPort)
    , m_resetPin(resetPin)
    , m_dio0Port(dio0Port)
    , m_dio0Pin(dio0Pin)
    , m_loraTimer(loraTimer)
    // Default configuration values
    , m_frequency(915000000)  // 915 MHz
    , m_power(17)            // 17 dBm
    , m_spreadingFactor(SpreadingFactor::SF_7)
    , m_bandwidth(Bandwidth::BW_125)
    , m_codingRate(CodingRate::CR_4_5)
    , m_crcEnabled(true)
    , m_packetLength(255)
    , m_syncWord(0x12)
    , m_readBytes(0)
    , m_status(Status::SLEEP)
{
    m_rxBuffer.fill(0);
    m_delayTimer = {0, 0, false};
}

void Sx1278Driver::initialize()
{
    initializeHardware();
    timerInit();

    configure();
}

void Sx1278Driver::initializeHardware()
{
    setNSS(true);
    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_SET);
}

void Sx1278Driver::setNSS(bool value)
{
    HAL_GPIO_WritePin(m_nssPort, m_nssPin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Sx1278Driver::reset()
{
    setNSS(true);
    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_RESET);
    delay(1);
    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_SET);
    delay(100);
}

void Sx1278Driver::writeCommand(uint8_t cmd)
{
    setNSS(false);
    HAL_SPI_Transmit(m_spi, &cmd, 1, 1000);
    while (HAL_SPI_GetState(m_spi) != HAL_SPI_STATE_READY);
}

uint8_t Sx1278Driver::readByte()
{
    uint8_t txByte = 0x00;
    uint8_t rxByte = 0x00;

    setNSS(false);
    HAL_SPI_TransmitReceive(m_spi, &txByte, &rxByte, 1, 1000);
    while (HAL_SPI_GetState(m_spi) != HAL_SPI_STATE_READY);
    return rxByte;
}

void Sx1278Driver::delay(uint32_t ms)
{
    m_delayTimer.start_tick = m_timerTick;
    m_delayTimer.delay = ms;
    m_delayTimer.active = true;
}

bool Sx1278Driver::getDIO0()
{
    return HAL_GPIO_ReadPin(m_dio0Port, m_dio0Pin) == GPIO_PIN_SET;
}

uint8_t Sx1278Driver::readRegister(Register reg)
{
    writeCommand(static_cast<uint8_t>(reg));
    uint8_t value = readByte();
    setNSS(true);
    return value;
}

void Sx1278Driver::writeRegister(Register reg, uint8_t value)
{
    setNSS(false);
    writeCommand(static_cast<uint8_t>(reg) | 0x80);
    writeCommand(value);
    setNSS(true);
}

void Sx1278Driver::burstRead(Register reg, uint8_t* data, uint8_t length)
{
    if (length <= 1) return;

    setNSS(false);
    writeCommand(static_cast<uint8_t>(reg));
    for (uint8_t i = 0; i < length; i++) {
        data[i] = readByte();
    }
    setNSS(true);
}

void Sx1278Driver::burstWrite(Register reg, const uint8_t* data, uint8_t length)
{
    if (length <= 1) return;

    setNSS(false);
    writeCommand(static_cast<uint8_t>(reg) | 0x80);
    for (uint8_t i = 0; i < length; i++) {
        writeCommand(data[i]);
    }
    setNSS(true);
}

void Sx1278Driver::configure()
{
    sleep();
    delay(15);

    writeRegister(Register::OP_MODE, 0x88);

    // Configure frequency
    uint64_t freq = (m_frequency << 19) / 32000000;
    uint8_t freqRegs[3] = {
        static_cast<uint8_t>(freq >> 16),
        static_cast<uint8_t>(freq >> 8),
        static_cast<uint8_t>(freq)
    };
    burstWrite(Register::FR_MSB, freqRegs, 3);

    // Set sync word
    writeRegister(Register::SYNC_WORD, m_syncWord);

    // Configure power and other base settings
    writeRegister(Register::PA_CONFIG, POWER_VALUES[m_power]);
    writeRegister(Register::OCP, 0x0B);
    writeRegister(Register::LNA, 0x23);

    // Configure modulation settings
    if (m_spreadingFactor == SpreadingFactor::SF_6) {
        configureModemSF6();
    } else {
        configureModemNormal();
    }

    writeRegister(Register::MODEM_CONFIG3, 0x04);
    writeRegister(Register::SYMB_TIMEOUT_LSB, 0x08);
    writeRegister(Register::PREAMBLE_MSB, 0x00);
    writeRegister(Register::PREAMBLE_LSB, 8);
    writeRegister(Register::DIO_MAPPING2, 0x01);

    m_readBytes = 0;
    standby();
}

bool Sx1278Driver::transmit(const uint8_t* data, uint8_t length, uint32_t timeout)
{
    if (!enterTxMode(length, timeout)) {
        return false;
    }

    burstWrite(Register::FIFO, data, length);
    writeRegister(Register::OP_MODE, 0x8b);  // Tx Mode

    while (timeout > 0) {
        if (getDIO0()) {
            readRegister(Register::IRQ_FLAGS);
            clearIrqFlags();
            standby();
            return true;
        }
        delay(1);
        timeout--;
    }

    reset();
    configure();
    return false;
}

bool Sx1278Driver::receive(uint8_t length, uint32_t timeout)
{
    m_packetLength = length;

    configure();
    writeRegister(Register::PA_DAC, 0x84);
    writeRegister(Register::HOP_PERIOD, 0xFF);
    writeRegister(Register::DIO_MAPPING1, 0x01);
    writeRegister(Register::IRQ_FLAGS_MASK, 0x3F);
    clearIrqFlags();
    writeRegister(Register::PAYLOAD_LENGTH, length);

    uint8_t addr = readRegister(Register::FIFO_RX_BASE_ADDR);
    writeRegister(Register::FIFO_ADDR_PTR, addr);
    writeRegister(Register::OP_MODE, 0x8d);

    m_readBytes = 0;

    while (timeout > 0) {
        if ((readRegister(Register::MODEM_STAT) & 0x04) == 0x04) {
            m_status = Status::RX;
            return true;
        }
        delay(1);
        timeout--;
    }

    reset();
    configure();
    return false;
}

uint8_t Sx1278Driver::available()
{
    if (!getDIO0()) {
        return 0;
    }

    std::memset(m_rxBuffer.data(), 0, MAX_PACKET);

    uint8_t addr = readRegister(Register::FIFO_RX_CURRENT_ADDR);
    writeRegister(Register::FIFO_ADDR_PTR, addr);

    uint8_t packetSize;
    if (m_spreadingFactor == SpreadingFactor::SF_6) {
        packetSize = m_packetLength;
    } else {
        packetSize = readRegister(Register::RX_NB_BYTES);
    }

    burstRead(Register::FIFO, m_rxBuffer.data(), packetSize);
    m_readBytes = packetSize;
    clearIrqFlags();

    return m_readBytes;
}

uint8_t Sx1278Driver::read(uint8_t* buffer, uint8_t length)
{
    if (length != m_readBytes) {
        length = m_readBytes;
    }
    std::memcpy(buffer, m_rxBuffer.data(), length);
    buffer[length] = '\0';
    m_readBytes = 0;
    return length;
}

uint8_t Sx1278Driver::getRSSI()
{
    uint32_t rssi = readRegister(Register::RSSI_VALUE);
    return static_cast<uint8_t>(rssi + 127 - 137);
}

void Sx1278Driver::sleep()
{
    writeRegister(Register::OP_MODE, 0x08);
    m_status = Status::SLEEP;
}

void Sx1278Driver::standby()
{
    writeRegister(Register::OP_MODE, 0x09);
    m_status = Status::STANDBY;
}

void Sx1278Driver::clearIrqFlags()
{
    writeRegister(Register::IRQ_FLAGS, 0xFF);
}

bool Sx1278Driver::enterTxMode(uint8_t length, uint32_t timeout)
{
    m_packetLength = length;

    configure();
    writeRegister(Register::PA_DAC, 0x87);
    writeRegister(Register::HOP_PERIOD, 0x00);
    writeRegister(Register::DIO_MAPPING1, 0x41);
    clearIrqFlags();
    writeRegister(Register::IRQ_FLAGS_MASK, 0xF7);
    writeRegister(Register::PAYLOAD_LENGTH, length);

    uint8_t addr = readRegister(Register::FIFO_TX_BASE_ADDR);
    writeRegister(Register::FIFO_ADDR_PTR, addr);

    while (timeout > 0) {
        if (readRegister(Register::PAYLOAD_LENGTH) == length) {
            m_status = Status::TX;
            return true;
        }
        timeout--;
    }

    reset();
    configure();
    return false;
}

void Sx1278Driver::configureModemSF6()
{
    writeRegister(Register::MODEM_CONFIG1,
        (static_cast<uint8_t>(m_bandwidth) << 4) |
            (static_cast<uint8_t>(m_codingRate) << 1) |
            0x01);

    writeRegister(Register::MODEM_CONFIG2,
        (static_cast<uint8_t>(m_spreadingFactor) << 4) |
            (m_crcEnabled ? 0x04 : 0x00) |
            0x03);

    uint8_t tmp = readRegister(Register::DETECT_OPTIMIZE);
    tmp &= 0xF8;
    tmp |= 0x05;
    writeRegister(Register::DETECT_OPTIMIZE, tmp);
    writeRegister(Register::DETECTION_THRESHOLD, 0x0C);
}

void Sx1278Driver::configureModemNormal()
{
    writeRegister(Register::MODEM_CONFIG1,
        (static_cast<uint8_t>(m_bandwidth) << 4) |
            (static_cast<uint8_t>(m_codingRate) << 1) |
            0x00);

    writeRegister(Register::MODEM_CONFIG2,
        (static_cast<uint8_t>(m_spreadingFactor) << 4) |
            (m_crcEnabled ? 0x04 : 0x00) |
            0x00);
}

void Sx1278Driver::timerInit()
{
    if (m_loraTimer != nullptr)
    {
        HAL_TIM_Base_Start_IT(m_loraTimer);
    }
}

bool Sx1278Driver::isDelayExpired()
{
    if (!m_delayTimer.active) {
        return false;
    }

    if ((m_timerTick - m_delayTimer.start_tick) >= m_delayTimer.delay) {
        m_delayTimer.active = false;
        return true;
    }

    return false;
}

void Sx1278Driver::handleTimerInterrupt(TIM_HandleTypeDef* htim)
{
    uint32_t current = m_timerTick;
    m_timerTick = current + 1;
}

} // namespace lg

extern "C" void TIM3_IRQHandler(void)
{
    lg::Sx1278Driver::handleTimerInterrupt(&htim3);
}