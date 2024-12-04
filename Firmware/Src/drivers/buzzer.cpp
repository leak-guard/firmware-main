#include <drivers/buzzer.hpp>

namespace lg {

void BuzzerDriver::initialize()
{
    m_timerFrequency = HAL_RCC_GetSysClockFreq();
    m_counterPeriod = __HAL_TIM_GetAutoreload(m_timer);
    HAL_TIM_Base_Start(m_timer);
}

void BuzzerDriver::buzzerOn()
{
    HAL_TIM_PWM_Start(m_timer, m_channel);
}

void BuzzerDriver::buzzerOff()
{
    HAL_TIM_PWM_Stop(m_timer, m_channel);
}

void BuzzerDriver::setFrequency(const uint32_t freq)
{
    __HAL_TIM_SET_PRESCALER(m_timer, prescalerForFreq(freq));
}

uint32_t BuzzerDriver::prescalerForFreq(const uint32_t freq)
{
    if (freq <= 0) return 0;
    return (m_timerFrequency / (m_counterPeriod * freq) - 1) >> 1;
}

}