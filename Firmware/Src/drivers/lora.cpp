#include <drivers/lora.hpp>

#include <FreeRTOS.h>
#include <cstring>
#include <task.h>

namespace lg {

static const uint8_t SX1278_Power[4] = {
    0xFF, // 20dbm
    0xFC, // 17dbm
    0xF9, // 14dbm
    0xF6, // 11dbm
};

static const uint8_t SX1278_SpreadFactor[7] = { 6, 7, 8, 9, 10, 11, 12 };

static const uint8_t SX1278_LoRaBandwidth[10] = {
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

static const uint8_t SX1278_CodingRate[4] = { 0x01, 0x02, 0x03, 0x04 };

static const uint8_t SX1278_CRC_Sum[2] = { 0x01, 0x00 };

std::uint8_t LoraDriver::SX1278_SPIRead(uint8_t addr)
{
    std::uint8_t temp;

    hwSPICommand(addr);
    temp = hwSPIReadByte();
    hwSetNSS(1);

    return temp;
}

void LoraDriver::SX1278_SPIWrite(uint8_t addr, uint8_t cmd)
{
    hwSetNSS(0);
    hwSPICommand(addr | 0x80);
    hwSPICommand(cmd);
    hwSetNSS(1);
}

void LoraDriver::SX1278_SPIBurstRead(uint8_t addr, uint8_t* rxBuf, uint8_t length)
{
    uint8_t i;

    // TODO: clean up this shit

    if (length <= 1) {
        return;
    } else {
        hwSetNSS(0);
        hwSPICommand(addr);

        for (i = 0; i < length; i++) {
            *(rxBuf + i) = hwSPIReadByte();
        }

        hwSetNSS(1);
    }
}

void LoraDriver::SX1278_SPIBurstWrite(uint8_t addr, uint8_t* txBuf, uint8_t length)
{
    unsigned char i;

    // TODO: clean up this shit

    if (length <= 1) {
        return;
    } else {
        hwSetNSS(0);
        hwSPICommand(addr | 0x80);
        for (i = 0; i < length; i++) {
            hwSPICommand(*(txBuf + i));
        }
        hwSetNSS(1);
    }
}

void LoraDriver::SX1278_config()
{
    SX1278_sleep(); // Change modem mode Must in Sleep mode
    hwDelayMs(15);

    SX1278_entryLoRa();
    // SX1278_SPIWrite(module, 0x5904); //?? Change digital regulator form 1.6V
    // to 1.47V: see errata note

    uint64_t freq = (static_cast<uint64_t>(m_frequency) << 19) / 32000000;
    uint8_t freq_reg[3];
    freq_reg[0] = (uint8_t)(freq >> 16);
    freq_reg[1] = (uint8_t)(freq >> 8);
    freq_reg[2] = (uint8_t)(freq >> 0);
    SX1278_SPIBurstWrite(LR_RegFrMsb, (uint8_t*)freq_reg, 3); // setting frequency parameter

    SX1278_SPIWrite(LR_RegSyncWord, m_syncWord);

    // setting base parameter
    SX1278_SPIWrite(LR_RegPaConfig, SX1278_Power[m_power]); // Setting output power parameter

    SX1278_SPIWrite(LR_RegOcp, 0x0B); // RegOcp,Close Ocp
    SX1278_SPIWrite(LR_RegLna, 0x23); // RegLNA,High & LNA Enable

    if (SX1278_SpreadFactor[m_sf] == 6) { // SFactor=6
        uint8_t tmp;

        // Implicit Enable CRC Enable(0x02) & Error Coding
        // rate 4/5(0x01), 4/6(0x02), 4/7(0x03), 4/8(0x04)
        SX1278_SPIWrite(LR_RegModemConfig1,
            ((SX1278_LoRaBandwidth[m_bw] << 4) + (SX1278_CodingRate[m_cr] << 1) + 0x01));

        SX1278_SPIWrite(LR_RegModemConfig2,
            ((SX1278_SpreadFactor[m_sf] << 4) + (SX1278_CRC_Sum[m_crcSum] << 2) + 0x03));

        tmp = SX1278_SPIRead(LR_RegDetectOptimize);
        tmp &= 0xF8;
        tmp |= 0x05;
        SX1278_SPIWrite(LR_RegDetectOptimize, tmp);
        SX1278_SPIWrite(LR_RegDetectionThreshold, 0x0C);
    } else {
        // Explicit Enable CRC Enable(0x02) & Error Coding
        // rate 4/5(0x01), 4/6(0x02), 4/7(0x03), 4/8(0x04)
        SX1278_SPIWrite(LR_RegModemConfig1,
            ((SX1278_LoRaBandwidth[m_bw] << 4) + (SX1278_CodingRate[m_cr] << 1) + 0x00));

        // SFactor & LNA gain set by the internal AGC loop
        SX1278_SPIWrite(LR_RegModemConfig2,
            ((SX1278_SpreadFactor[m_sf] << 4) + (SX1278_CRC_Sum[m_crcSum] << 2) + 0x00));
    }

    SX1278_SPIWrite(LR_RegModemConfig3, 0x04);
    SX1278_SPIWrite(LR_RegSymbTimeoutLsb, 0x08); // RegSymbTimeoutLsb Timeout = 0x3FF(Max)
    SX1278_SPIWrite(LR_RegPreambleMsb, 0x00); // RegPreambleMsb
    SX1278_SPIWrite(LR_RegPreambleLsb, 8); // RegPreambleLsb 8+4=12byte Preamble
    SX1278_SPIWrite(REG_LR_DIOMAPPING2, 0x01); // RegDioMapping2 DIO5=00, DIO4=01
    m_readBytes = 0;
    SX1278_standby(); // Entry standby mode
}

void LoraDriver::SX1278_standby()
{
    SX1278_SPIWrite(LR_RegOpMode, 0x09);
    m_status = Status::STANDBY;
}

void LoraDriver::SX1278_sleep()
{
    SX1278_SPIWrite(LR_RegOpMode, 0x08);
    m_status = Status::SLEEP;
}

void LoraDriver::SX1278_entryLoRa()
{
    SX1278_SPIWrite(LR_RegOpMode, 0x88);
}

void LoraDriver::SX1278_clearLoRaIrq()
{
    SX1278_SPIWrite(LR_RegIrqFlags, 0xFF);
}

int LoraDriver::SX1278_LoRaEntryRx(uint8_t length, uint32_t timeout)
{
    uint8_t addr;

    m_packetLength = length;

    SX1278_config(); // Setting base parameter
    SX1278_SPIWrite(REG_LR_PADAC, 0x84); // Normal and RX
    SX1278_SPIWrite(LR_RegHopPeriod, 0xFF); // No FHSS
    SX1278_SPIWrite(REG_LR_DIOMAPPING1, 0x01); // DIO=00,DIO1=00,DIO2=00, DIO3=01
    SX1278_SPIWrite(LR_RegIrqFlagsMask, 0x3F); // Open RxDone interrupt & Timeout
    SX1278_clearLoRaIrq();
    SX1278_SPIWrite(LR_RegPayloadLength, length); // Payload Length 21byte(this register must difine
                                                  // when the data long of one byte in SF is 6)
    addr = SX1278_SPIRead(LR_RegFifoRxBaseAddr); // Read RxBaseAddr
    SX1278_SPIWrite(LR_RegFifoAddrPtr, addr); // RxBaseAddr->FiFoAddrPtr
    SX1278_SPIWrite(LR_RegOpMode, 0x8d); // Mode//Low Frequency Mode

    // High Frequency Mode
    m_readBytes = 0;

    while (1) {
        if ((SX1278_SPIRead(LR_RegModemStat) & 0x04) == 0x04) { // Rx-on going RegModemStat
            m_status = Status::RX;
            return 1;
        }
        if (--timeout == 0) {
            hwReset();
            SX1278_config();
            return 0;
        }
        hwDelayMs(1);
    }
}

uint8_t LoraDriver::SX1278_LoRaRxPacket()
{
    unsigned char addr;
    unsigned char packet_size;

    if (hwGetDIO0()) {
        memset(m_rxBuffer.data(), LR_RegFifo, MAX_PACKET);

        addr = SX1278_SPIRead(LR_RegFifoRxCurrentaddr); // last packet addr
        SX1278_SPIWrite(LR_RegFifoAddrPtr, addr); // RxBaseAddr -> FiFoAddrPtr

        if (m_sf == static_cast<int>(LoraDriver::LoraSpreadFactor::SF_6)) {
            // When SpreadFactor is six, will used Implicit
            // Header mode(Excluding internal packet length)
            packet_size = m_packetLength;
        } else {
            packet_size = SX1278_SPIRead(LR_RegRxNbBytes); // Number for received bytes
        }

        SX1278_SPIBurstRead(LR_RegFifo, m_rxBuffer.data(), packet_size);
        m_readBytes = packet_size;
        SX1278_clearLoRaIrq();
    }

    return m_readBytes;
}

int LoraDriver::SX1278_LoRaEntryTx(uint8_t length, uint32_t timeout)
{
    uint8_t addr;
    uint8_t temp;

    m_packetLength = length;

    SX1278_config(); // setting base parameter
    SX1278_SPIWrite(REG_LR_PADAC, 0x87); // Tx for 20dBm
    SX1278_SPIWrite(LR_RegHopPeriod, 0x00); // RegHopPeriod NO FHSS
    SX1278_SPIWrite(REG_LR_DIOMAPPING1, 0x41); // DIO0=01, DIO1=00,DIO2=00, DIO3=01
    SX1278_clearLoRaIrq();
    SX1278_SPIWrite(LR_RegIrqFlagsMask, 0xF7); // Open TxDone interrupt
    SX1278_SPIWrite(LR_RegPayloadLength, length); // RegPayloadLength 21byte
    addr = SX1278_SPIRead(LR_RegFifoTxBaseAddr); // RegFiFoTxBaseAddr
    SX1278_SPIWrite(LR_RegFifoAddrPtr, addr); // RegFifoAddrPtr

    while (1) {
        temp = SX1278_SPIRead(LR_RegPayloadLength);
        if (temp == length) {
            m_status = Status::TX;
            return 1;
        }

        if (--timeout == 0) {
            hwReset();
            SX1278_config();
            return 0;
        }
    }
}

int LoraDriver::SX1278_LoRaTxPacket(uint8_t* txBuffer, uint8_t length, uint32_t timeout)
{
    SX1278_SPIBurstWrite(LR_RegFifo, txBuffer, length);
    SX1278_SPIWrite(LR_RegOpMode, 0x8b); // Tx Mode
    while (1) {
        if (hwGetDIO0()) { // if(Get_NIRQ()) //Packet send over
            SX1278_SPIRead(LR_RegIrqFlags);
            SX1278_clearLoRaIrq(); // Clear irq
            SX1278_standby(); // Entry Standby mode
            return 1;
        }

        if (--timeout == 0) {
            hwReset();
            SX1278_config();
            return 0;
        }
        hwDelayMs(1);
    }
}

void LoraDriver::SX1278_init(LoraFrequency frequency, LoraPower power,
    LoraSpreadFactor sf, LoraBandwidth bw, LoraCodingRate cr,
    bool crcEnabled, uint8_t packetLength, uint8_t syncWord)
{
    hwInit();
    m_frequency = static_cast<std::uint32_t>(frequency);
    m_power = static_cast<std::uint8_t>(power);
    m_sf = static_cast<std::uint8_t>(sf);
    m_bw = static_cast<std::uint8_t>(bw);
    m_cr = static_cast<std::uint8_t>(cr);
    m_crcSum = static_cast<std::uint8_t>(!crcEnabled);
    m_packetLength = packetLength;
    m_syncWord = syncWord;
    SX1278_config();
}

int LoraDriver::SX1278_transmit(uint8_t* txBuf, uint8_t length, uint32_t timeout)
{
    if (SX1278_LoRaEntryTx(length, timeout)) {
        return SX1278_LoRaTxPacket(txBuf, length, timeout);
    }

    return 0;
}

int LoraDriver::SX1278_receive(uint8_t length, uint32_t timeout)
{
    return SX1278_LoRaEntryRx(length, timeout);
}

uint8_t LoraDriver::SX1278_available()
{
    return SX1278_LoRaRxPacket();
}

uint8_t LoraDriver::SX1278_read(uint8_t* rxBuf, uint8_t length)
{
    if (length != m_readBytes) {
        length = m_readBytes;
    }
    memcpy(rxBuf, m_rxBuffer.data(), length);
    rxBuf[length] = '\0';
    m_readBytes = 0;
    return length;
}

uint8_t LoraDriver::SX1278_RSSI_LoRa()
{
    uint32_t temp = 10;
    temp = SX1278_SPIRead(LR_RegRssiValue); // Read RegRssiValue, Rssi value
    temp = temp + 127 - 137; // 127:Max RSSI, 137:RSSI offset
    return (uint8_t)temp;
}

uint8_t LoraDriver::SX1278_RSSI()
{
    uint8_t temp = 0xff;
    temp = SX1278_SPIRead(FSK_RegRssiValue);
    temp = 127 - (temp >> 1); // 127:Max RSSI
    return temp;
}

// Hardware functions

void LoraDriver::hwInit()
{
    hwSetNSS(1);
    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_SET);
}

void LoraDriver::hwSetNSS(int value)
{
    HAL_GPIO_WritePin(m_nssPort, m_nssPin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LoraDriver::hwReset()
{
    hwSetNSS(1);
    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_RESET);

    hwDelayMs(1);

    HAL_GPIO_WritePin(m_resetPort, m_resetPin, GPIO_PIN_SET);

    hwDelayMs(100);
}

void LoraDriver::hwSPICommand(uint8_t cmd)
{
    hwSetNSS(0);
    HAL_SPI_Transmit(m_spi, &cmd, 1, 1000);

    while (HAL_SPI_GetState(m_spi) != HAL_SPI_STATE_READY)
        ;
}

uint8_t LoraDriver::hwSPIReadByte()
{
    uint8_t txByte = 0x00;
    uint8_t rxByte = 0x00;

    hwSetNSS(0);
    HAL_SPI_TransmitReceive(m_spi, &txByte, &rxByte, 1, 1000);
    while (HAL_SPI_GetState(m_spi) != HAL_SPI_STATE_READY)
        ;

    return rxByte;
}

void LoraDriver::hwDelayMs(std::uint32_t msec)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(msec);
    } else {
        HAL_Delay(msec);
    }
}

int LoraDriver::hwGetDIO0()
{
    return (HAL_GPIO_ReadPin(m_dio0Port, m_dio0Pin) == GPIO_PIN_SET);
}

};
