#pragma once
#include "drivers/sx1278.hpp"
#include <array>
#include <functional>
#include <leakguard/staticvector.hpp>
#include <stm32f7xx_hal.h>
#include <FreeRTOS.h>
#include <task.h>

namespace lg {

class LoraService {
public:
    explicit LoraService(SPI_HandleTypeDef* spi)
        : m_spi(spi)
    {
        // Default configuration initialization
        m_config.frequency = 915000000;  // 915 MHz
        m_config.power = 17;             // 17 dBm
        m_config.sf = Sx1278Driver::SpreadingFactor::SF_7;
        m_config.bw = Sx1278Driver::Bandwidth::BW_125;
        m_config.cr = Sx1278Driver::CodingRate::CR_4_5;
        m_config.enableCrc = true;
        m_config.syncWord = 0x12;
    }

    struct Packet {
        StaticVector<uint8_t, 255> data;
        uint8_t rssi;

        Packet() { }
        Packet(const uint8_t* srcData, uint8_t length, uint8_t rssi)
            : rssi(rssi)
        {
            for (uint8_t i = 0; i < length; i++) {
                data.Append(srcData[i]);
            }
        }
    };

    enum class LoraMode {
        SLEEP,
        STANDBY,
        RX,
        TX
    };

    struct LoraConfig {
        uint64_t frequency;
        uint8_t power;
        Sx1278Driver::SpreadingFactor sf;
        Sx1278Driver::Bandwidth bw;
        Sx1278Driver::CodingRate cr;
        bool enableCrc;
        uint8_t syncWord;
    };

    using OnReceiveCallback = std::function<void(const Packet&)>;

    static void loraServiceEntryPoint(void* params);
    void initialize();

    bool transmit(const uint8_t* data, uint8_t length, uint32_t timeout = 1000);
    void startReceive(uint8_t length = 255);
    void stopReceive();
    void sleep();
    void standby();

    // Configuration methods
    void setFrequency(uint64_t freq) { m_config.frequency = freq; }
    void setPower(uint8_t power) { m_config.power = power; }
    void setSpreadingFactor(Sx1278Driver::SpreadingFactor sf) { m_config.sf = sf; }
    void setBandwidth(Sx1278Driver::Bandwidth bw) { m_config.bw = bw; }
    void setCodingRate(Sx1278Driver::CodingRate cr) { m_config.cr = cr; }
    void setEnableCrc(bool enable) { m_config.enableCrc = enable; }
    void setSyncWord(uint8_t syncWord) { m_config.syncWord = syncWord; }

    void setOnReceiveCallback(OnReceiveCallback callback) { m_onReceiveCallback = callback; }
    LoraMode getMode() const { return m_mode; }
    void loraTimerCallback();

private:
    static constexpr int LORA_CHECK_PERIOD_MS = 10;

    void setLoraTimer();
    void loraServiceMain();
    void handleReceivedData();
    void applyConfig();

    SPI_HandleTypeDef* m_spi;
    TIM_HandleTypeDef* m_loraTimer;
    TaskHandle_t m_loraServiceTaskHandle {};
    StaticTask_t m_loraServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 256> m_loraServiceTaskStack {};

    LoraMode m_mode { LoraMode::SLEEP };
    OnReceiveCallback m_onReceiveCallback;
    LoraConfig m_config;
    uint8_t m_expectedLength { 255 };
};

} // namespace lg