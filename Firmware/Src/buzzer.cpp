#include <buzzer.hpp>
#include <device.hpp>

namespace lg {

BuzzerService::ToneSequence chord1;
BuzzerService::ToneSequence chord2;
BuzzerService::ToneSequence chord3;
BuzzerService::ToneSequence chord4;

void BuzzerService::buzzerServiceEntryPoint(void* params)
{
    auto buzzerService = reinterpret_cast<BuzzerService*>(params);
    buzzerService->buzzerServiceMain();
}

void BuzzerService::initialize()
{
    chord1.Append({ C_4, 50 });
    chord1.Append({ Eb4, 50 });
    chord1.Append({ G_4, 50 });
    chord1.Append({ Bb4, 50 });

    chord2.Append({ C_4, 50 });
    chord2.Append({ Eb4, 50 });
    chord2.Append({ Gb4, 50 });
    chord2.Append({ Bb4, 50 });

    chord3.Append({ C_4, 50 });
    chord3.Append({ Eb4, 50 });
    chord3.Append({ F_4, 50 });
    chord3.Append({ Bb4, 50 });

    chord4.Append({ C_4, 50 });
    chord4.Append({ Eb4, 50 });
    chord4.Append({ F_4, 50 });
    chord4.Append({ A_4, 50 });

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
    static uint8_t duty = 10;
    static bool dutyDir = false;

    Device::get().getBuzzerDriver()->buzzerOff();
    HAL_TIM_Base_Stop_IT(m_toneTimer);

    if (dutyDir) duty++;
    else duty--;
    if (duty > 90) dutyDir = false;
    else if (duty < 10) dutyDir = true;
    Device::get().getBuzzerDriver()->setDutyCycle(duty);

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
        playSequence(chord1, LOOP);
        vTaskDelay(3000);
        playSequence(chord2, LOOP);
        vTaskDelay(3000);
        playSequence(chord3, LOOP);
        vTaskDelay(3000);
        playSequence(chord4, LOOP);
        vTaskDelay(3000);
    }
}

}

extern "C" void TIM7_IRQHandler(void)
{
    lg::Device::get().getBuzzerService()->toneTimerCallback();
}