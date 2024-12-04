#pragma once
#include <FreeRTOS.h>
#include <array>
#include <stm32f7xx_hal.h>
#include <task.h>

namespace lg {

class BuzzerService {
public:
    explicit BuzzerService(TIM_HandleTypeDef* toneTimer) : m_toneTimer(toneTimer) {}

    static void buzzerServiceEntryPoint(void* params);

    void initialize();

    void playTone(uint16_t frequency, uint16_t duration);
    void toneTimerCallback();

private:
    void setToneTimer(uint16_t duration);

    void buzzerServiceMain();

    TIM_HandleTypeDef* m_toneTimer;

    TaskHandle_t m_buzzerServiceTaskHandle {};
    StaticTask_t m_buzzerServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 256> m_buzzerServiceTaskStack {};
};

}