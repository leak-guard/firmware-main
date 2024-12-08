#include "sx1278.hpp"
#include "device.hpp"

namespace lg {

void LoraService::loraServiceEntryPoint(void* params)
{
    auto loraService = reinterpret_cast<LoraService*>(params);
    loraService->loraServiceMain();
}

void LoraService::initialize()
{
    auto lora = Device::get().getLoraDriver();

    // Configure the driver before initialization
    lora->setLoraFrequency(m_config.frequency);
    lora->setLoraPower(m_config.power);
    lora->setLoraSpreadingFactor(m_config.sf);
    lora->setLoraBandwidth(m_config.bw);
    lora->setLoraCodingRate(m_config.cr);
    lora->setLoraCrcEnable(m_config.enableCrc);
    lora->setLoraSyncWord(m_config.syncWord);
    lora->setLoraPacketLength(255);  // Max packet length

    // Initialize with the configured settings
    lora->initialize();

    m_loraServiceTaskHandle = xTaskCreateStatic(
        &LoraService::loraServiceEntryPoint,
        "Lora Service",
        m_loraServiceTaskStack.size(),
        this,
        5,  // Priority
        m_loraServiceTaskStack.data(),
        &m_loraServiceTaskTcb
    );
}

void LoraService::loraServiceMain()
{
    while (true) {
        if (m_mode == LoraMode::RX) {
            auto lora = Device::get().getLoraDriver();
            if (lora->available()) {
                handleReceivedData();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(LORA_CHECK_PERIOD_MS));
    }
}

bool LoraService::transmit(const uint8_t* data, uint8_t length, uint32_t timeout)
{
    if (m_mode == LoraMode::RX) {
        stopReceive();
    }

    m_mode = LoraMode::TX;
    auto lora = Device::get().getLoraDriver();
    bool success = lora->transmit(data, length, timeout);
    standby();

    return success;
}

void LoraService::startReceive(uint8_t length)
{
    if (m_mode == LoraMode::RX) {
        return;
    }

    m_expectedLength = length;
    auto lora = Device::get().getLoraDriver();
    if (lora->receive(length)) {
        m_mode = LoraMode::RX;
    }
}

void LoraService::stopReceive()
{
    if (m_mode != LoraMode::RX) {
        return;
    }
    standby();
}

void LoraService::sleep()
{
    if (m_mode == LoraMode::RX) {
        stopReceive();
    }
    auto lora = Device::get().getLoraDriver();
    lora->sleep();
    m_mode = LoraMode::SLEEP;
}

void LoraService::standby()
{
    auto lora = Device::get().getLoraDriver();
    lora->standby();
    m_mode = LoraMode::STANDBY;
}

void LoraService::handleReceivedData()
{
    auto lora = Device::get().getLoraDriver();
    uint8_t buffer[255];
    uint8_t length = lora->read(buffer, m_expectedLength);
    uint8_t rssi = lora->getRSSI();
    
    if (m_onReceiveCallback && length > 0) {
        Packet packet(buffer, length, rssi);
        m_onReceiveCallback(packet);
    }
}

void LoraService::loraTimerCallback()
{
    // Handle any timer-based operations here
    // Currently unused but maintained for future extensions
}

void LoraService::setLoraTimer()
{
    if (m_loraTimer != nullptr) {
        HAL_TIM_Base_Start_IT(m_loraTimer);
    }
}

} // namespace lg

/*
extern "C" void TIM3_IRQHandler(void)
{
    lg::Device::get().getLoraService()->loraTimerCallback();
}*/
