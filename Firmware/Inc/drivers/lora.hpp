#pragma once
#include <array>
#include <cstdint>
#include <functional>

#include <stm32f7xx_hal.h>

#include <FreeRTOS.h>
#include <task.h>

namespace lg {

class LoraDriver {
public:
    enum class LoraFrequency {
        FREQ_433MHz = 433000000,
        FREQ_866MHz = 866000000,
        FREQ_915MHz = 915000000,
    };

    enum class LoraPower {
        POWER_20DBM = 0,
        POWER_17DBM = 1,
        POWER_14DBM = 2,
        POWER_11DBM = 3,
    };

    enum class LoraSpreadFactor {
        SF_6 = 0,
        SF_7 = 1,
        SF_8 = 2,
        SF_9 = 3,
        SF_10 = 4,
        SF_11 = 5,
        SF_12 = 6,
    };

    enum class LoraBandwidth {
        BW_7_8KHZ = 0,
        BW_10_4KHZ = 1,
        BW_15_6KHZ = 2,
        BW_20_8KHZ = 3,
        BW_31_2KHZ = 4,
        BW_41_7KHZ = 5,
        BW_62_5KHZ = 6,
        BW_125KHZ = 7,
        BW_250KHZ = 8,
        BW_500KHZ = 9,
    };

    enum class LoraCodingRate {
        CR_4_5 = 0,
        CR_4_6 = 1,
        CR_4_7 = 2,
        CR_4_8 = 3,
    };

    static constexpr auto MAX_PACKET = 256;
    static constexpr auto DEFAULT_TIMEOUT = 3000;
    static constexpr auto SYNC_WORD = 0x12; //  7     0     default LoRa sync word
    static constexpr auto SYNC_WORD_LORAWAN = 0x34; //  7     0     sync word reserved for LoRaWAN networks

    static void loraDriverEntryPoint(void* params);

    LoraDriver(SPI_HandleTypeDef* spi,
        GPIO_TypeDef* dio0Port, int dio0Pin,
        GPIO_TypeDef* nssPort, int nssPin,
        GPIO_TypeDef* resetPort, int resetPin,
        std::size_t initialMessageSize)
        : m_spi(spi)
        , m_dio0Port(dio0Port)
        , m_dio0Pin(dio0Pin)
        , m_nssPort(nssPort)
        , m_nssPin(nssPin)
        , m_resetPort(resetPort)
        , m_resetPin(resetPin)
        , m_initialMsgSize(initialMessageSize)
    {
    }

    void initialize();

    int setModeRx(uint8_t length, uint32_t timeout);
    int setModeTx(uint8_t* txBuf, uint8_t length,
        uint32_t timeout);

    int32_t getLastPacketRssi();

    void dio0RisingEdgeIsr();

    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    std::function<void(const std::uint8_t*, std::size_t, std::int32_t)> onPacket;
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)
private:
    static constexpr auto LORA_MESSAGE_EVENT = 1;

    enum class Status {
        SLEEP,
        STANDBY,
        TX,
        RX
    };

    SPI_HandleTypeDef* m_spi;
    GPIO_TypeDef* m_dio0Port;
    int m_dio0Pin;
    GPIO_TypeDef* m_nssPort;
    int m_nssPin;
    GPIO_TypeDef* m_resetPort;
    int m_resetPin;
    std::size_t m_initialMsgSize;

    TaskHandle_t m_loraDriverTaskHandle {};
    StaticTask_t m_loraDriverTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 256> m_loraDriverTaskStack {};

    std::uint32_t m_frequency {};
    std::uint8_t m_power {};
    std::uint8_t m_sf {};
    std::uint8_t m_bw {};
    std::uint8_t m_cr {};
    std::uint8_t m_crcSum {};
    std::uint8_t m_packetLength {};
    std::uint8_t m_syncWord {};
    std::uint8_t m_readBytes {};
    Status m_status {};

    std::array<std::uint8_t, MAX_PACKET> m_rxBuffer {};

    void loraDriverMain();

    void initializeHardware(LoraFrequency frequency, LoraPower power,
        LoraSpreadFactor sf, LoraBandwidth bw, LoraCodingRate cr,
        bool crcEnabled, uint8_t packetLength, uint8_t syncWord);
    uint8_t receivePacket();
    int transmitPacket(uint8_t* txBuf, uint8_t length, uint32_t timeout);
    uint8_t isPacketAvailable();
    uint8_t readBytes(uint8_t* rxBuf, uint8_t length);
    void setStandbyMode();
    void setSleepMode();

    uint8_t readSpi(uint8_t addr);
    void writeSpi(uint8_t addr, uint8_t cmd);
    void readSpiBurst(uint8_t addr, uint8_t* rxBuf, uint8_t length);
    void writeSpiBurst(uint8_t addr, uint8_t* txBuf, uint8_t length);

    uint8_t getLoraRssi();
    uint8_t getFskRssi();

    void configure();
    void entryLora();
    void clearLoraIrq();
    int entryLoraRx(uint8_t length, uint32_t timeout);
    int entryLoraTx(uint8_t length, uint32_t timeout);

    // RFM98 Internal registers Address
    /********************LoRa mode***************************/
    static constexpr auto LR_RegFifo = 0x00;
    // Common settings
    static constexpr auto LR_RegOpMode = 0x01;
    static constexpr auto LR_RegFrMsb = 0x06;
    static constexpr auto LR_RegFrMid = 0x07;
    static constexpr auto LR_RegFrLsb = 0x08;
    // Tx settings
    static constexpr auto LR_RegPaConfig = 0x09;
    static constexpr auto LR_RegPaRamp = 0x0A;
    static constexpr auto LR_RegOcp = 0x0B;
    // Rx settings
    static constexpr auto LR_RegLna = 0x0C;
    // LoRa registers
    static constexpr auto LR_RegFifoAddrPtr = 0x0D;
    static constexpr auto LR_RegFifoTxBaseAddr = 0x0E;
    static constexpr auto LR_RegFifoRxBaseAddr = 0x0F;
    static constexpr auto LR_RegFifoRxCurrentaddr = 0x10;
    static constexpr auto LR_RegIrqFlagsMask = 0x11;
    static constexpr auto LR_RegIrqFlags = 0x12;
    static constexpr auto LR_RegRxNbBytes = 0x13;
    static constexpr auto LR_RegRxHeaderCntValueMsb = 0x14;
    static constexpr auto LR_RegRxHeaderCntValueLsb = 0x15;
    static constexpr auto LR_RegRxPacketCntValueMsb = 0x16;
    static constexpr auto LR_RegRxPacketCntValueLsb = 0x17;
    static constexpr auto LR_RegModemStat = 0x18;
    static constexpr auto LR_RegPktSnrValue = 0x19;
    static constexpr auto LR_RegPktRssiValue = 0x1A;
    static constexpr auto LR_RegRssiValue = 0x1B;
    static constexpr auto LR_RegHopChannel = 0x1C;
    static constexpr auto LR_RegModemConfig1 = 0x1D;
    static constexpr auto LR_RegModemConfig2 = 0x1E;
    static constexpr auto LR_RegSymbTimeoutLsb = 0x1F;
    static constexpr auto LR_RegPreambleMsb = 0x20;
    static constexpr auto LR_RegPreambleLsb = 0x21;
    static constexpr auto LR_RegPayloadLength = 0x22;
    static constexpr auto LR_RegMaxPayloadLength = 0x23;
    static constexpr auto LR_RegHopPeriod = 0x24;
    static constexpr auto LR_RegFifoRxByteAddr = 0x25;
    static constexpr auto LR_RegModemConfig3 = 0x26;
    static constexpr auto LR_RegFeiMsb = 0x28;
    static constexpr auto LR_RegFeiMid = 0x29;
    static constexpr auto LR_RegFeiMLsb = 0x2a;
    static constexpr auto LR_RegRssiWideBand = 0x2c;
    static constexpr auto LR_RegDetectOptimize = 0x31;
    static constexpr auto LR_RegInvertIq = 0x33;
    static constexpr auto LR_RegDetectionThreshold = 0x37;
    static constexpr auto LR_RegSyncWord = 0x39;
    // I/O settings
    static constexpr auto REG_LR_DIOMAPPING1 = 0x40;
    static constexpr auto REG_LR_DIOMAPPING2 = 0x41;
    // Version
    static constexpr auto REG_LR_VERSION = 0x42;
    // Additional settings
    static constexpr auto REG_LR_PLLHOP = 0x44;
    static constexpr auto REG_LR_TCXO = 0x4B;
    static constexpr auto REG_LR_PADAC = 0x4D;
    static constexpr auto REG_LR_FORMERTEMP = 0x5B;
    static constexpr auto REG_LR_AGCREF = 0x61;
    static constexpr auto REG_LR_AGCTHRESH1 = 0x62;
    static constexpr auto REG_LR_AGCTHRESH2 = 0x63;
    static constexpr auto REG_LR_AGCTHRESH3 = 0x64;

    static constexpr auto FSK_RegRssiValue = 0x11;

    // Hardware functions
    void hardwareInit();
    void hardwareSetNss(int value);
    void hardwareReset();
    void hardwareSpiCommand(uint8_t cmd);
    uint8_t hardwareSpiReadByte();
    void hardwareDelayMs(uint32_t msec);
    int hardwareGetDio0();
};

};
