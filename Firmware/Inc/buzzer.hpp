#pragma once
#include <FreeRTOS.h>
#include <array>
#include <functional>
#include <leakguard/staticstring.hpp>
#include <leakguard/staticvector.hpp>
#include <stm32f7xx_hal.h>
#include <task.h>

namespace lg {

class BuzzerService {
public:
    explicit BuzzerService(TIM_HandleTypeDef* toneTimer)
        : m_toneTimer(toneTimer)
    {
    }

    struct Tone {
        uint16_t frequency;
        uint16_t duration;
        uint8_t dutyCycle;

        Tone() { }
        Tone(const uint16_t frequency, const uint16_t duration, const uint8_t dutyCycle)
            : frequency(frequency)
            , duration(duration)
            , dutyCycle(dutyCycle)
        {
        }
        Tone(const uint16_t frequency, const uint16_t duration)
            : Tone(frequency, duration, 50)
        {
        }
    };

    enum Note : uint16_t {
        C_4 = 261,
        Db4 = 277,
        D_4 = 293,
        Eb4 = 311,
        E_4 = 329,
        F_4 = 349,
        Gb4 = 369,
        G_4 = 392,
        Ab4 = 415,
        A_4 = 440,
        Bb4 = 466,
        B_4 = 493,

        C_5 = C_4 * 2,
        Db5 = Db4 * 2,
        D_5 = D_4 * 2,
        Eb5 = Eb4 * 2,
        E_5 = E_4 * 2,
        F_5 = F_4 * 2,
        Gb5 = Gb4 * 2,
        G_5 = G_4 * 2,
        Ab5 = Ab4 * 2,
        A_5 = A_4 * 2,
        Bb5 = Bb4 * 2,
        B_5 = B_4 * 2,

        C_6 = C_5 * 2,
        Db6 = Db5 * 2,
        D_6 = D_5 * 2,
        Eb6 = Eb5 * 2,
        E_6 = E_5 * 2,
        F_6 = F_5 * 2,
        Gb6 = Gb5 * 2,
        G_6 = G_5 * 2,
        Ab6 = Ab5 * 2,
        A_6 = A_5 * 2,
        Bb6 = Bb5 * 2,
        B_6 = B_5 * 2,

        C_7 = C_6 * 2,
        Db7 = Db6 * 2,
        D_7 = D_6 * 2,
        Eb7 = Eb6 * 2,
        E_7 = E_6 * 2,
        F_7 = F_6 * 2,
        Gb7 = Gb6 * 2,
        G_7 = G_6 * 2,
        Ab7 = Ab6 * 2,
        A_7 = A_6 * 2,
        Bb7 = Bb6 * 2,
        B_7 = B_6 * 2,

        C_3 = C_4 / 2,
        Db3 = Db4 / 2,
        D_3 = D_4 / 2,
        Eb3 = Eb4 / 2,
        E_3 = E_4 / 2,
        F_3 = F_4 / 2,
        Gb3 = Gb4 / 2,
        G_3 = G_4 / 2,
        Ab3 = Ab4 / 2,
        A_3 = A_4 / 2,
        Bb3 = Bb4 / 2,
        B_3 = B_4 / 2,

        SIL = 0,
    };

    enum SequenceMode {
        OFF,
        ONESHOT,
        LOOP
    };

    enum SoundMode {
        TONE,
        SAMPLE
    };

    using ToneSequence = StaticVector<Tone, 256>;

    static void buzzerServiceEntryPoint(void* params);
    void initialize();

    void playTone(uint16_t frequency, uint16_t duration, uint8_t dutyCycle = 50, uint8_t priority = 128);
    void playTone(const Tone tone, const uint8_t priority = 128) { playTone(tone.frequency, tone.duration, tone.dutyCycle, priority); }

    void playSequence(const ToneSequence& sequence, SequenceMode sequenceMode = ONESHOT, uint8_t priority = 128);

    void stopSequence(uint8_t priority);

    void playSample(const std::function<bool(uint32_t)>& sampleProvider, uint16_t sampleRate, uint8_t priority = 128);

    void toneTimerCallback();

private:
    static constexpr int TONE_TIM_PRESCALER = 215;
    static constexpr int SAMPLE_TIM_PERIOD = 4096;

    void setToneTimer(uint16_t duration);
    void setToneSoundMode();
    void setSampleSoundMode(uint16_t sampleRate);

    void buzzerServiceMain();

    TIM_HandleTypeDef* m_toneTimer;

    TaskHandle_t m_buzzerServiceTaskHandle {};
    StaticTask_t m_buzzerServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 256> m_buzzerServiceTaskStack {};

    ToneSequence m_toneSequence;
    uint32_t m_toneSequenceIndex = 0;
    SequenceMode m_sequenceMode = OFF;

    SoundMode m_soundMode = TONE;
    uint32_t m_sampleTime = 0;

    uint8_t m_currentPriority = 0xFF;

    std::function<bool(uint32_t)> m_sampleProvider;
};

}