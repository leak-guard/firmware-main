#pragma once
#include <stm32f7xx_hal.h>

namespace lg {

class BuzzerDriver {
public:
    explicit BuzzerDriver(TIM_HandleTypeDef* timer, uint32_t channel) : m_timer(timer), m_channel(channel) {}

    void initialize();
    void buzzerOn();
    void buzzerOff();
    void setFrequency(uint32_t freq);

private:
    uint32_t prescalerForFreq(uint32_t freq);

    TIM_HandleTypeDef* m_timer;
    uint32_t m_channel;

    uint32_t m_timerFrequency = 0;
    uint32_t m_counterPeriod = 0;
};

}