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

        Tone() {}
        Tone(const uint16_t frequency, const uint16_t duration, const uint8_t dutyCycle)
            : frequency(frequency), duration(duration), dutyCycle(dutyCycle) {}
        Tone(const uint16_t frequency, const uint16_t duration) : Tone(frequency, duration, 50) {}
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

    void playTone(uint16_t frequency, uint16_t duration, uint8_t dutyCycle);
    void playTone(const Tone tone) { playTone(tone.frequency, tone.duration, tone.dutyCycle); }

    void playSequence(const ToneSequence& sequence, SequenceMode sequenceMode);

    void playSample(const std::function<bool(uint32_t)>& sampleProvider, uint16_t sampleRate);

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

    std::function<bool(uint32_t)> m_sampleProvider;
};

}