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

void BuzzerService::playTone(const uint16_t frequency, const uint16_t duration, const uint8_t dutyCycle = 50)
{
    if (m_soundMode != TONE)
        setToneSoundMode();

    auto buzzer = Device::get().getBuzzerDriver();
    buzzer->setFrequency(frequency);
    buzzer->setDutyCycle(dutyCycle);
    setToneTimer(duration);

    if (frequency != 0)
        buzzer->buzzerOn();

    HAL_TIM_Base_Start_IT(m_toneTimer);
}

void BuzzerService::playSequence(const ToneSequence& sequence, const SequenceMode sequenceMode = ONESHOT)
{
    m_sequenceMode = sequenceMode;
    m_toneSequence = sequence;
    m_toneSequenceIndex = 0;

    playTone(m_toneSequence[m_toneSequenceIndex]);
}

void BuzzerService::playSample(const std::function<bool(uint32_t)>& sampleProvider, uint16_t sampleRate)
{
    setSampleSoundMode(sampleRate);
    m_sampleTime = 0;

    m_sampleProvider = sampleProvider;

    Device::get().getBuzzerDriver()->setDutyCycle(0);
    Device::get().getBuzzerDriver()->buzzerOn();

    HAL_TIM_Base_Start_IT(m_toneTimer);
}

void BuzzerService::toneTimerCallback()
{
    if (m_soundMode == TONE) {
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
    else if (m_soundMode == SAMPLE) {
        HAL_TIM_Base_Stop_IT(m_toneTimer);
        __HAL_TIM_CLEAR_FLAG(m_toneTimer, TIM_FLAG_UPDATE);

        bool sample = m_sampleProvider(m_sampleTime);
        Device::get().getBuzzerDriver()->setDutyCycle(sample ? 0 : 100);
        m_sampleTime++;

        HAL_TIM_Base_Start_IT(m_toneTimer);
    }
}

void BuzzerService::setToneTimer(const uint16_t duration)
{
    __HAL_TIM_SetAutoreload(m_toneTimer, duration);
    __HAL_TIM_CLEAR_FLAG(m_toneTimer, TIM_FLAG_UPDATE);
}

void BuzzerService::setToneSoundMode()
{
    m_soundMode = TONE;
    __HAL_TIM_SET_PRESCALER(m_toneTimer, TONE_TIM_PRESCALER);
}

void BuzzerService::setSampleSoundMode(const uint16_t sampleRate)
{
    m_soundMode = SAMPLE;

    auto prescaler = (HAL_RCC_GetSysClockFreq() / (SAMPLE_TIM_PERIOD * sampleRate) >> 1) - 1;
    __HAL_TIM_SET_PRESCALER(m_toneTimer, prescaler);
    __HAL_TIM_SetAutoreload(m_toneTimer, SAMPLE_TIM_PERIOD);
    __HAL_TIM_SET_COUNTER(m_toneTimer, 0);
    __HAL_TIM_CLEAR_FLAG(m_toneTimer, TIM_FLAG_UPDATE);
}

void BuzzerService::buzzerServiceMain()
{
    while (true) {
        vTaskDelay(1000);
    }
}

}

extern "C" void TIM7_IRQHandler(void)
{
    lg::Device::get().getBuzzerService()->toneTimerCallback();
}