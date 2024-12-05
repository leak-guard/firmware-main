#include "lora.hpp"

namespace lg {

void LoraService::loraServiceEntryPoint(void* params)
{
    auto loraService = reinterpret_cast<LoraService*>(params);
    loraService->loraServiceMain();
}

void LoraService::initialize()
{
    SX1278_hw.dio0.port = m_dio0Port;
    SX1278_hw.dio0.pin = m_dio0Pin;
    SX1278_hw.nss.port = m_nssPort;
    SX1278_hw.nss.pin = m_nssPin;
    SX1278_hw.reset.port = m_resetPort;
    SX1278_hw.reset.pin = m_resetPin;
    SX1278_hw.spi = m_spi;

    SX1278.hw = &SX1278_hw;

    SX1278_init(&SX1278, SX1278_FREQ_433MHz, SX1278_POWER_20DBM, SX1278_LORA_SF_12,
        SX1278_LORA_BW_125KHZ, SX1278_LORA_CR_4_5, SX1278_LORA_CRC_DIS, 8, SX127X_SYNC_WORD);

    SX1278_LoRaEntryRx(&SX1278, 16, 2000);
}

void LoraService::loraServiceMain()
{
    while (true) {
        Message receivedMsg;
        ret = SX1278_LoRaRxPacket(&SX1278);

        if (ret > 0)
        {
            SX1278_read(&SX1278, (uint8_t*)&receivedMsg, sizeof(Message));

            for (uint8_t i = 0; i < 200; i++) {
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6); // buzzer out
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0); // imp led
                HAL_Delay(1);
            }
        }

        HAL_Delay(800);
    }
}

}