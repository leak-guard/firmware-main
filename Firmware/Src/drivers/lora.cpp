#include <drivers/lora.hpp>

#include <array>
#include <cstring>

#include <FreeRTOS.h>
#include <portmacro.h>
#include <task.h>

namespace lg {

static const std::array<uint8_t, 4> SX1278_Power = {
    0xFF, // 20dbm
    0xFC, // 17dbm
    0xF9, // 14dbm
    0xF6, // 11dbm
};

static const std::array<uint8_t, 7> SX1278_SpreadFactor = {
    6, 7, 8, 9, 10, 11, 12
};

static const std::array<uint8_t, 10> SX1278_LoRaBandwidth = {
    0, //   7.8KHz,
    1, //  10.4KHz,
    2, //  15.6KHz,
    3, //  20.8KHz,
    4, //  31.2KHz,
    5, //  41.7KHz,
    6, //  62.5KHz,
    7, // 125.0KHz,
    8, // 250.0KHz,
    9 // 500.0KHz
};

static const std::array<uint8_t, 4> SX1278_CodingRate = {
    0x01, 0x02, 0x03, 0x04
};

static const std::array<uint8_t, 2> SX1278_CRC_Sum = {
    0x01, 0x00
};

void LoraDriver::loraDriverEntryPoint(void* params)
{
    auto instance = reinterpret_cast<LoraDriver*>(params);
    instance->loraDriverMain();
}

void LoraDriver::initialize()
{
    m_loraDriverTaskHandle = xTaskCreateStatic(
        &LoraDriver::loraDriverEntryPoint /* Task function */,
        "LoRa Driver" /* Task name */,
        m_loraDriverTaskStack.size() /* Stack size */,
        this /* Parameters */,
        7 /* Priority */,
        m_loraDriverTaskStack.data() /* Task stack address */,
        &m_loraDriverTaskTcb /* Task control block */
    );

    initializeHardware(
        LoraDriver::LoraFrequency::FREQ_433MHz,
        LoraDriver::LoraPower::POWER_11DBM,
        LoraDriver::LoraSpreadFactor::SF_8,
        LoraDriver::LoraBandwidth::BW_125KHZ,
        LoraDriver::LoraCodingRate::CR_4_5,
        false,
        m_initialMsgSize,
        LoraDriver::SYNC_WORD);
}

void LoraDriver::loraDriverMain()
{
    std::array<std::uint8_t, MAX_PACKET> buffer {};

    while (true) {
        xTaskNotifyWait(0, LORA_MESSAGE_EVENT, nullptr, portMAX_DELAY);

        auto packetSize = receivePacket();
        if (packetSize > 0) {
            if (readBytes(buffer.data(), packetSize) == packetSize) {
                if (onPacket) {
                    onPacket(buffer.data(), packetSize, getLastPacketRssi());
                }
            }
        }
    }
}

int32_t LoraDriver::getLastPacketRssi()
{
    int32_t registerValue = readSpi(LR_RegPktRssiValue);
    return registerValue - 137;
}

void LoraDriver::dio0RisingEdgeIsr()
{
    xTaskNotifyFromISR(m_loraDriverTaskHandle, LORA_MESSAGE_EVENT, eSetBits, nullptr);
}

void LoraDriver::configure()
{
    setSleepMode(); // Change modem mode Must in Sleep mode
    hardwareDelayMs(15);

    entryLora();
    // SX1278_SPIWrite(module, 0x5904); //?? Change digital regulator form 1.6V
    // to 1.47V: see errata note

    uint64_t freq = (static_cast<uint64_t>(m_frequency) << 19) / 32000000;
    std::array<uint8_t, 3> freq_reg = { (uint8_t)(freq >> 16), (uint8_t)(freq >> 8), (uint8_t)(freq >> 0) };

    writeSpiBurst(LR_RegFrMsb, (uint8_t*)freq_reg.data(), 3); // setting frequency parameter

    writeSpi(LR_RegSyncWord, m_syncWord);

    // setting base parameter
    writeSpi(LR_RegPaConfig, SX1278_Power.at(m_power)); // Setting output power parameter

    writeSpi(LR_RegOcp, 0x0B); // RegOcp,Close Ocp
    writeSpi(LR_RegLna, 0x23); // RegLNA,High & LNA Enable

    if (SX1278_SpreadFactor.at(m_sf) == 6) { // SFactor=6
        uint8_t tmp {};

        // Implicit Enable CRC Enable(0x02) & Error Coding
        // rate 4/5(0x01), 4/6(0x02), 4/7(0x03), 4/8(0x04)
        writeSpi(LR_RegModemConfig1,
            ((SX1278_LoRaBandwidth.at(m_bw) << 4) + (SX1278_CodingRate.at(m_cr) << 1) + 0x01));

        writeSpi(LR_RegModemConfig2,
            ((SX1278_SpreadFactor.at(m_sf) << 4) + (SX1278_CRC_Sum.at(m_crcSum) << 2) + 0x03));

        tmp = readSpi(LR_RegDetectOptimize);
        tmp &= 0xF8;
        tmp |= 0x05;
        writeSpi(LR_RegDetectOptimize, tmp);
        writeSpi(LR_RegDetectionThreshold, 0x0C);
    } else {
        // Explicit Enable CRC Enable(0x02) & Error Coding
        // rate 4/5(0x01), 4/6(0x02), 4/7(0x03), 4/8(0x04)
        writeSpi(LR_RegModemConfig1,
            ((SX1278_LoRaBandwidth.at(m_bw) << 4) + (SX1278_CodingRate.at(m_cr) << 1) + 0x00));

        // SFactor & LNA gain set by the internal AGC loop
        writeSpi(LR_RegModemConfig2,
            ((SX1278_SpreadFactor.at(m_sf) << 4) + (SX1278_CRC_Sum.at(m_crcSum) << 2) + 0x00));
    }

    writeSpi(LR_RegModemConfig3, 0x04);
    writeSpi(LR_RegSymbTimeoutLsb, 0x08); // RegSymbTimeoutLsb Timeout = 0x3FF(Max)
    writeSpi(LR_RegPreambleMsb, 0x00); // RegPreambleMsb
    writeSpi(LR_RegPreambleLsb, 8); // RegPreambleLsb 8+4=12byte Preamble
    writeSpi(REG_LR_DIOMAPPING2, 0x01); // RegDioMapping2 DIO5=00, DIO4=01
    m_readBytes = 0;
    setStandbyMode(); // Entry standby mode
}

void LoraDriver::setStandbyMode()
{
    writeSpi(LR_RegOpMode, 0x09);
    m_status = Status::STANDBY;
}

void LoraDriver::setSleepMode()
{
    writeSpi(LR_RegOpMode, 0x08);
    m_status = Status::SLEEP;
}

void LoraDriver::entryLora()
{
    writeSpi(LR_RegOpMode, 0x88);
}

void LoraDriver::clearLoraIrq()
{
    writeSpi(LR_RegIrqFlags, 0xFF);
}

int LoraDriver::entryLoraRx(uint8_t length, uint32_t timeout)
{
    uint8_t addr {};

    m_packetLength = length;

    configure(); // Setting base parameter
    writeSpi(REG_LR_PADAC, 0x84); // Normal and RX
    writeSpi(LR_RegHopPeriod, 0xFF); // No FHSS
    writeSpi(REG_LR_DIOMAPPING1, 0x01); // DIO=00,DIO1=00,DIO2=00, DIO3=01
    writeSpi(LR_RegIrqFlagsMask, 0x3F); // Open RxDone interrupt & Timeout
    clearLoraIrq();
    writeSpi(LR_RegPayloadLength, length); // Payload Length 21byte(this register must difine
                                           // when the data long of one byte in SF is 6)
    addr = readSpi(LR_RegFifoRxBaseAddr); // Read RxBaseAddr
    writeSpi(LR_RegFifoAddrPtr, addr); // RxBaseAddr->FiFoAddrPtr
    writeSpi(LR_RegOpMode, 0x8d); // Mode//Low Frequency Mode

    // High Frequency Mode
    m_readBytes = 0;

    while (true) {
        if ((readSpi(LR_RegModemStat) & 0x04) == 0x04) { // Rx-on going RegModemStat
            m_status = Status::RX;
            return 1;
        }
        if (--timeout == 0) {
            hardwareReset();
            configure();
            return 0;
        }
        hardwareDelayMs(1);
    }
}

uint8_t LoraDriver::receivePacket()
{
    unsigned char addr {};
    unsigned char packet_size {};

    if (hardwareGetDio0()) {
        memset(m_rxBuffer.data(), LR_RegFifo, MAX_PACKET);

        addr = readSpi(LR_RegFifoRxCurrentaddr); // last packet addr
        writeSpi(LR_RegFifoAddrPtr, addr); // RxBaseAddr -> FiFoAddrPtr

        if (m_sf == static_cast<int>(LoraDriver::LoraSpreadFactor::SF_6)) {
            // When SpreadFactor is six, will used Implicit
            // Header mode(Excluding internal packet length)
            packet_size = m_packetLength;
        } else {
            packet_size = readSpi(LR_RegRxNbBytes); // Number for received bytes
        }

        readSpiBurst(LR_RegFifo, m_rxBuffer.data(), packet_size);
        m_readBytes = packet_size;
        clearLoraIrq();
    }

    return m_readBytes;
}

int LoraDriver::entryLoraTx(uint8_t length, uint32_t timeout)
{
    uint8_t addr {};
    uint8_t temp {};

    m_packetLength = length;

    configure(); // setting base parameter
    writeSpi(REG_LR_PADAC, 0x87); // Tx for 20dBm
    writeSpi(LR_RegHopPeriod, 0x00); // RegHopPeriod NO FHSS
    writeSpi(REG_LR_DIOMAPPING1, 0x41); // DIO0=01, DIO1=00,DIO2=00, DIO3=01
    clearLoraIrq();
    writeSpi(LR_RegIrqFlagsMask, 0xF7); // Open TxDone interrupt
    writeSpi(LR_RegPayloadLength, length); // RegPayloadLength 21byte
    addr = readSpi(LR_RegFifoTxBaseAddr); // RegFiFoTxBaseAddr
    writeSpi(LR_RegFifoAddrPtr, addr); // RegFifoAddrPtr

    while (true) {
        temp = readSpi(LR_RegPayloadLength);
        if (temp == length) {
            m_status = Status::TX;
            return 1;
        }

        if (--timeout == 0) {
            hardwareReset();
            configure();
            return 0;
        }
    }
}

int LoraDriver::transmitPacket(uint8_t* txBuffer, uint8_t length, uint32_t timeout)
{
    writeSpiBurst(LR_RegFifo, txBuffer, length);
    writeSpi(LR_RegOpMode, 0x8b); // Tx Mode
    while (1) {
        if (hardwareGetDio0()) { // if(Get_NIRQ()) //Packet send over
            readSpi(LR_RegIrqFlags);
            clearLoraIrq(); // Clear irq
            setStandbyMode(); // Entry Standby mode
            return 1;
        }

        if (--timeout == 0) {
            hardwareReset();
            configure();
            return 0;
        }
        hardwareDelayMs(1);
    }
}

void LoraDriver::initializeHardware(LoraFrequency frequency, LoraPower power,
    LoraSpreadFactor sf, LoraBandwidth bw, LoraCodingRate cr,
    bool crcEnabled, uint8_t packetLength, uint8_t syncWord)
{
    hardwareInit();
    m_frequency = static_cast<std::uint32_t>(frequency);
    m_power = static_cast<std::uint8_t>(power);
    m_sf = static_cast<std::uint8_t>(sf);
    m_bw = static_cast<std::uint8_t>(bw);
    m_cr = static_cast<std::uint8_t>(cr);
    m_crcSum = static_cast<std::uint8_t>(!crcEnabled);
    m_packetLength = packetLength;
    m_syncWord = syncWord;
    configure();
}

int LoraDriver::setModeTx(uint8_t* txBuf, uint8_t length, uint32_t timeout)
{
    if (entryLoraTx(length, timeout)) {
        return transmitPacket(txBuf, length, timeout);
    }

    return 0;
}

int LoraDriver::setModeRx(uint8_t length, uint32_t timeout)
{
    return entryLoraRx(length, timeout);
}

uint8_t LoraDriver::isPacketAvailable()
{
    return receivePacket();
}

uint8_t LoraDriver::readBytes(uint8_t* rxBuf, uint8_t length)
{
    if (length != m_readBytes) {
        length = m_readBytes;
    }
    memcpy(rxBuf, m_rxBuffer.data(), length);
    m_readBytes = 0;
    return length;
}

uint8_t LoraDriver::getLoraRssi()
{
    uint32_t temp = 10;
    temp = readSpi(LR_RegRssiValue); // Read RegRssiValue, Rssi value
    temp = temp + 127 - 137; // 127:Max RSSI, 137:RSSI offset
    return (uint8_t)temp;
}

uint8_t LoraDriver::getFskRssi()
{
    uint8_t temp = 0xff;
    temp = readSpi(FSK_RegRssiValue);
    temp = 127 - (temp >> 1); // 127:Max RSSI
    return temp;
}

std::uint8_t LoraDriver::readSpi(uint8_t addr)
{
    std::uint8_t temp {};

    hardwareSpiCommand(addr);
    temp = hardwareSpiReadByte();
    hardwareSetNss(1);

    return temp;
}

void LoraDriver::writeSpi(uint8_t addr, uint8_t cmd)
{
    hardwareSetNss(0);
    hardwareSpiCommand(addr | 0x80);
    hardwareSpiCommand(cmd);
    hardwareSetNss(1);
}

void LoraDriver::readSpiBurst(uint8_t addr, uint8_t* rxBuf, uint8_t length)
{
    if (length <= 1) {
        return;
    } else {
        hardwareSetNss(0);
        hardwareSpiCommand(addr);

        HAL_SPI_Receive(m_spi, rxBuf, length, 1000);

        hardwareSetNss(1);
    }
}

void LoraDriver::writeSpiBurst(uint8_t addr, uint8_t* txBuf, uint8_t length)
{
    if (length <= 1) {
        return;
    } else {
        hardwareSetNss(0);
        hardwareSpiCommand(addr | 0x80);

        HAL_SPI_Receive(m_spi, txBuf, length, 1000);

        hardwareSetNss(1);
    }
}

// Hardware functions

void LoraDriver::hardwareInit()
{
    hardwareSetNss(1);
    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_SET);
}

void LoraDriver::hardwareSetNss(int value)
{
    HAL_GPIO_WritePin(m_nssPort, m_nssPin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LoraDriver::hardwareReset()
{
    hardwareSetNss(1);
    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_RESET);

    hardwareDelayMs(1);

    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_SET);

    hardwareDelayMs(100);
}

void LoraDriver::hardwareSpiCommand(uint8_t cmd)
{
    hardwareSetNss(0);
    HAL_SPI_Transmit(m_spi, &cmd, 1, 1000);

    while (HAL_SPI_GetState(m_spi) != HAL_SPI_STATE_READY)
        ;
}

uint8_t LoraDriver::hardwareSpiReadByte()
{
    uint8_t txByte = 0x00;
    uint8_t rxByte = 0x00;

    hardwareSetNss(0);
    HAL_SPI_TransmitReceive(m_spi, &txByte, &rxByte, 1, 1000);
    while (HAL_SPI_GetState(m_spi) != HAL_SPI_STATE_READY)
        ;

    return rxByte;
}

void LoraDriver::hardwareDelayMs(std::uint32_t msec)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(msec);
    } else {
        HAL_Delay(msec);
    }
}

int LoraDriver::hardwareGetDio0()
{
    return (HAL_GPIO_ReadPin(m_dio0Port, m_dio0Pin) == GPIO_PIN_SET);
}

};
