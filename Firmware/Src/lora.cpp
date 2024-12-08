#include <lora.hpp>

#include <device.hpp>

namespace lg {

void LoraService::loraServiceEntryPoint(void* params)
{
    auto loraService = reinterpret_cast<LoraService*>(params);
    loraService->loraServiceMain();
}

void LoraService::initialize()
{
    auto loraDriver = Device::get().getLoraDriver();
    loraDriver->SX1278_init(
        LoraDriver::LoraFrequency::FREQ_433MHz,
        LoraDriver::LoraPower::POWER_20DBM,
        LoraDriver::LoraSpreadFactor::SF_12,
        LoraDriver::LoraBandwidth::BW_125KHZ,
        LoraDriver::LoraCodingRate::CR_4_5,
        false,
        8,
        LoraDriver::SYNC_WORD);

    // loraDriver->SX1278_LoRaEntryRx(sizeof(ProbeMessage), 2000);
    loraDriver->SX1278_LoRaEntryRx(16, 2000);
    loraServiceMain();
}

void LoraService::loraServiceMain()
{
    auto loraDriver = Device::get().getLoraDriver();
    ProbeMessage receivedMsg {};

    while (true) {
        auto ret = loraDriver->SX1278_LoRaRxPacket();

        if (ret > 0) {
            loraDriver->SX1278_read((uint8_t*)&receivedMsg, sizeof(ProbeMessage));

            for (uint8_t i = 0; i < 200; i++) {
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6); // buzzer out
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0); // imp led
                HAL_Delay(1);
            }
        }

        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0); // imp led
        HAL_Delay(800);
    }
}

}