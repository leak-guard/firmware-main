#include <cron.hpp>
#include <device.hpp>

#include <rtc.h>

#include <stm32f7xx_hal.h>

namespace lg {

void CronService::cronServiceEntryPoint(void* params)
{
    auto instance = reinterpret_cast<CronService*>(params);
    instance->cronServiceMain();
}

void CronService::initialize()
{
    m_cronServiceTaskHandle = xTaskCreateStatic(
        &CronService::cronServiceEntryPoint /* Task function */,
        "Cron Service" /* Task name */,
        m_cronServiceTaskStack.size() /* Stack size */,
        this /* parameters */,
        1 /* Prority */,
        m_cronServiceTaskStack.data() /* Task stack address */,
        &m_cronServiceTaskTcb /* Task control block */
    );

    setupAlarmInterrupt();
}

void CronService::registerJob(std::function<void()> job)
{
    m_jobsToRun.Append(job);
}

void CronService::notifyCronFromIsr()
{
    xTaskNotifyFromISR(m_cronServiceTaskHandle, 1, eSetBits, NULL);
}

void CronService::cronServiceMain()
{
    while (true) {
        xTaskNotifyWait(0, 1, NULL, portMAX_DELAY);

        for (auto& job : m_jobsToRun) {
            job();
        }
    }
}

void CronService::setupAlarmInterrupt()
{
    RTC_AlarmTypeDef alarm {};
    alarm.Alarm = RTC_ALARM_A;
    alarm.AlarmMask = RTC_ALARMMASK_ALL ^ RTC_ALARMMASK_SECONDS;
    alarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;

    HAL_RTC_SetAlarm_IT(&hrtc, &alarm, FORMAT_BCD);
    HAL_RTCEx_SetSmoothCalib(&hrtc,
        RTC_SMOOTHCALIB_PERIOD_32SEC, RTC_SMOOTHCALIB_PLUSPULSES_SET, 380);
    HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
}

extern "C" void RTC_Alarm_IRQHandler()
{
    Device::get().getCronService()->notifyCronFromIsr();
    HAL_RTC_AlarmIRQHandler(&hrtc);
}

};
