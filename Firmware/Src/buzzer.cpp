#include <buzzer.hpp>
#include <device.hpp>

namespace lg {

void BuzzerService::buzzerServiceEntryPoint(void* params)
{
    auto buzzerService = reinterpret_cast<BuzzerService*>(params);
    buzzerService->buzzerServiceMain();
}

void BuzzerService::initialize()
{
    m_buzzerServiceTaskHandle = xTaskCreateStatic(
        &BuzzerService::buzzerServiceEntryPoint /* Task function */,
        "Buzzer Service" /* Task name */,
        m_buzzerServiceTaskStack.size() /* Stack size */,
        this /* parameters */,
        6 /* Prority */,
        m_buzzerServiceTaskStack.data() /* Task stack address */,
        &m_buzzerServiceTaskTcb /* Task control block */
    );
}

void BuzzerService::playTone(const uint16_t frequency, const uint16_t duration)
{
    auto buzzer = Device::get().getBuzzerDriver();
    buzzer->setFrequency(frequency);
    setToneTimer(duration);

    buzzer->buzzerOn();

    HAL_TIM_Base_Start_IT(m_toneTimer);
}

void BuzzerService::toneTimerCallback()
{
    Device::get().getBuzzerDriver()->buzzerOff();
    HAL_TIM_Base_Stop_IT(m_toneTimer);
}

void BuzzerService::setToneTimer(const uint16_t duration)
{
    __HAL_TIM_SetAutoreload(m_toneTimer, duration);
    __HAL_TIM_CLEAR_FLAG(m_toneTimer, TIM_FLAG_UPDATE);
}

void BuzzerService::buzzerServiceMain()
{
    while (true) {
        playTone(440, 200);
        vTaskDelay(1000);
    }
}

}

extern "C" void TIM7_IRQHandler(void)
{
    lg::Device::get().getBuzzerService()->toneTimerCallback();
}