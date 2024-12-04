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

    if (frequency != 0)
        buzzer->buzzerOn();

    HAL_TIM_Base_Start_IT(m_toneTimer);
}

void BuzzerService::playSequence(const ToneSequence& sequence, SequenceMode sequenceMode = ONESHOT)
{
    m_sequenceMode = sequenceMode;
    m_toneSequence = sequence;
    m_toneSequenceIndex = 0;

    playTone(m_toneSequence[m_toneSequenceIndex]);
}

void BuzzerService::toneTimerCallback()
{
    Device::get().getBuzzerDriver()->buzzerOff();
    HAL_TIM_Base_Stop_IT(m_toneTimer);

    if (m_sequenceMode) {
        m_toneSequenceIndex++;
        if (m_toneSequenceIndex >= m_toneSequence.GetSize()) {
            if (m_sequenceMode == ONESHOT) {
                m_sequenceMode = OFF;
                return;
            }
            if (m_sequenceMode == LOOP) {
                m_toneSequenceIndex = 0;
            }
        }

        const auto tone = m_toneSequence[m_toneSequenceIndex];
        playTone(tone);
    }
}

void BuzzerService::setToneTimer(const uint16_t duration)
{
    __HAL_TIM_SetAutoreload(m_toneTimer, duration);
    __HAL_TIM_CLEAR_FLAG(m_toneTimer, TIM_FLAG_UPDATE);
}

void BuzzerService::buzzerServiceMain()
{
    while (true) {
        vTaskDelay(10000);
    }
}

}

extern "C" void TIM7_IRQHandler(void)
{
    lg::Device::get().getBuzzerService()->toneTimerCallback();
}