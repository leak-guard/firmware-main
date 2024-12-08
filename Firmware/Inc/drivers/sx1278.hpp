#pragma once
#include <array>
#include <cstdint>
#include "stm32f7xx_hal.h"

namespace lg {

class Sx1278Driver {
public:
    enum class SpreadingFactor {
        SF_6 = 6,
        SF_7 = 7,
        SF_8 = 8,
        SF_9 = 9,
        SF_10 = 10,
        SF_11 = 11,
        SF_12 = 12
    };

    enum class Bandwidth {
        BW_7_8 = 0,
        BW_10_4 = 1,
        BW_15_6 = 2,
        BW_20_8 = 3,
        BW_31_25 = 4,
        BW_41_7 = 5,
        BW_62_5 = 6,
        BW_125 = 7,
        BW_250 = 8,
        BW_500 = 9
    };

    enum class CodingRate {
        CR_4_5 = 1,
        CR_4_6 = 2,
        CR_4_7 = 3,
        CR_4_8 = 4
    };

    enum class Status {
        SLEEP,
        STANDBY,
        TX,
        RX
    };

    enum class Register : uint8_t {
        FIFO = 0x00,
        OP_MODE = 0x01,
        FR_MSB = 0x06,
        FR_MID = 0x07,
        FR_LSB = 0x08,
        PA_CONFIG = 0x09,
        OCP = 0x0B,
        LNA = 0x0C,
        FIFO_ADDR_PTR = 0x0D,
        FIFO_TX_BASE_ADDR = 0x0E,
        FIFO_RX_BASE_ADDR = 0x0F,
        FIFO_RX_CURRENT_ADDR = 0x10,
        IRQ_FLAGS_MASK = 0x11,
        IRQ_FLAGS = 0x12,
        RX_NB_BYTES = 0x13,
        MODEM_STAT = 0x18,
        MODEM_CONFIG1 = 0x1D,
        MODEM_CONFIG2 = 0x1E,
        SYMB_TIMEOUT_LSB = 0x1F,
        PREAMBLE_MSB = 0x20,
        PREAMBLE_LSB = 0x21,
        PAYLOAD_LENGTH = 0x22,
        MODEM_CONFIG3 = 0x26,
        RSSI_VALUE = 0x1B,
        DIO_MAPPING1 = 0x40,
        DIO_MAPPING2 = 0x41,
        DETECT_OPTIMIZE = 0x31,
        DETECTION_THRESHOLD = 0x37,
        SYNC_WORD = 0x39,
        PA_DAC = 0x4D,
        HOP_PERIOD = 0x24,
    };

    struct DelayTimer {
        uint32_t start_tick{0};
        uint32_t delay{0};
        bool active{false};
    };

    static constexpr uint8_t MAX_PACKET = 255;
    static constexpr uint8_t POWER_VALUES[] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87};

    explicit Sx1278Driver(SPI_HandleTypeDef* spi,
        GPIO_TypeDef* nssPort, uint16_t nssPin,
        GPIO_TypeDef* resetPort, uint16_t resetPin,
        GPIO_TypeDef* dio0Port, uint16_t dio0Pin,
        TIM_HandleTypeDef* loraTimer);

    void initialize();
    bool transmit(const uint8_t* data, uint8_t length, uint32_t timeout = 1000);
    bool receive(uint8_t length, uint32_t timeout = 1000);
    uint8_t available();
    uint8_t read(uint8_t* buffer, uint8_t length);
    uint8_t getRSSI();
    void sleep();
    void standby();
    void reset();

    static void handleTimerInterrupt(TIM_HandleTypeDef* htim);

    // Configuration methods
    void setLoraFrequency(uint64_t freq) { m_frequency = freq; }
    void setLoraPower(uint8_t power) { m_power = power; }
    void setLoraSpreadingFactor(SpreadingFactor sf) { m_spreadingFactor = sf; }
    void setLoraBandwidth(Bandwidth bw) { m_bandwidth = bw; }
    void setLoraCodingRate(CodingRate cr) { m_codingRate = cr; }
    void setLoraCrcEnable(bool enable) { m_crcEnabled = enable; }
    void setLoraSyncWord(uint8_t syncWord) { m_syncWord = syncWord; }
    void setLoraPacketLength(uint8_t length) { m_packetLength = length; }

private:
    static volatile uint32_t m_timerTick;

    SPI_HandleTypeDef* m_spi;
    GPIO_TypeDef* m_nssPort;
    uint16_t m_nssPin;
    GPIO_TypeDef* m_resetPort;
    uint16_t m_resetPin;
    GPIO_TypeDef* m_dio0Port;
    uint16_t m_dio0Pin;
    TIM_HandleTypeDef* m_loraTimer;
    std::array<uint8_t, MAX_PACKET> m_rxBuffer;

    uint64_t m_frequency;
    uint8_t m_power;
    SpreadingFactor m_spreadingFactor;
    Bandwidth m_bandwidth;
    CodingRate m_codingRate;
    bool m_crcEnabled;
    uint8_t m_packetLength;
    uint8_t m_syncWord;
    uint8_t m_readBytes;
    Status m_status;
    DelayTimer m_delayTimer;

    void initializeHardware();
    void setNSS(bool value);
    void writeCommand(uint8_t cmd);
    uint8_t readByte();
    void delay(uint32_t ms);
    bool getDIO0();
    uint8_t readRegister(Register reg);
    void writeRegister(Register reg, uint8_t value);
    void burstRead(Register reg, uint8_t* data, uint8_t length);
    void burstWrite(Register reg, const uint8_t* data, uint8_t length);
    void configure();
    void clearIrqFlags();
    bool enterTxMode(uint8_t length, uint32_t timeout);
    void configureModemSF6();
    void configureModemNormal();
    void timerInit();
    bool isDelayExpired();
};

} // namespace lg