#include <flow-meter.hpp>

#include <device.hpp>
#include <utc.hpp>

#include <stm32f7xx_ll_tim.h>

namespace lg {

void FlowMeterService::flowMeterServiceEntryPoint(void* params)
{
    auto instance = reinterpret_cast<FlowMeterService*>(params);
    instance->flowMeterServiceMain();
}

void FlowMeterService::initialize()
{
    Device::get().getCronService()->registerJob([] {
        Device::get().getFlowMeterService()->handleInterval();
    });

    m_flowMeterServiceTaskHandle = xTaskCreateStatic(
        &FlowMeterService::flowMeterServiceEntryPoint /* Task function */,
        "Flow Meter Svc" /* Task name */,
        m_flowMeterServiceTaskStack.size() /* Stack size */,
        this /* Parameters */,
        6 /* Priority */,
        m_flowMeterServiceTaskStack.data() /* Task stack address */,
        &m_flowMeterServiceTaskTcb /* Task control block */
    );

    enableCounter();
}

void FlowMeterService::impulseButtonPressed()
{
    static constexpr auto INCREMENT = 10;

    std::uint16_t currentCounter = LL_TIM_GetCounter(m_timer->Instance);
    currentCounter += INCREMENT;
    LL_TIM_SetCounter(m_timer->Instance, currentCounter);
}

void FlowMeterService::setTotalVolumeInMl(std::uint32_t value)
{
    m_totalMilliliters = value;
}

void FlowMeterService::setInitialTodayVolumeInMl(std::uint32_t value)
{
    m_midnightMilliliters = m_totalMilliliters - value;
    m_midnightTime = Device::get().getLocalTime();
}

void FlowMeterService::flowMeterServiceMain()
{
    while (true) {
        updateFlow();
        vTaskDelay(MEASUREMENT_INTERVAL_MS);
    }
}

void FlowMeterService::updateFlow()
{
    static constexpr auto DEFAULT_IMPULSES_PER_LITER = 500;
    static constexpr auto MS_PER_MINUTE = 60000.f;

    std::uint16_t currentImpulses = LL_TIM_GetCounter(m_timer->Instance);
    std::uint32_t currentTicks = xTaskGetTickCount();
    std::uint32_t impulsesPerLiterInt = 0;
    float impulsesPerLiter = 0;

    {
        auto configService = Device::get().getConfigService();
        impulsesPerLiterInt = configService->getCurrentConfig().impulsesPerLiter;
        impulsesPerLiter = static_cast<float>(impulsesPerLiterInt);
    }

    if (impulsesPerLiter == 0) {
        impulsesPerLiter = DEFAULT_IMPULSES_PER_LITER;
        impulsesPerLiterInt = DEFAULT_IMPULSES_PER_LITER;
    }

    std::uint16_t impulseDelta = currentImpulses - m_prevImpulses;
    m_prevImpulses = currentImpulses;

    if (impulseDelta) {
        HAL_GPIO_WritePin(m_impPort, m_impPin, GPIO_PIN_SET);
    }

    DataPoint currentDataPoint = { currentTicks, impulseDelta };
    m_history.PushOne(currentDataPoint);

    m_totalImpulseDelta += impulseDelta;

    std::uint32_t impulsesIncrement = m_milliliterImpulseRemainder + impulseDelta * 1000;
    std::uint32_t wholeMilliliters = impulsesIncrement / impulsesPerLiterInt;
    std::uint32_t currentMilliliters = m_totalMilliliters;
    currentMilliliters += wholeMilliliters;

    m_milliliterImpulseRemainder
        = impulsesIncrement - wholeMilliliters * impulsesPerLiterInt;

    m_totalMilliliters = currentMilliliters;

    if (m_history.GetCurrentSize() == m_history.GetCapacity()) {
        DataPoint lastDataPoint {};
        m_history.PeekAndPop(lastDataPoint);

        std::uint32_t timeDelta = currentTicks - lastDataPoint.ticks;

        float totalMilliliters
            = static_cast<float>(m_totalImpulseDelta) * 1000.f / impulsesPerLiter;
        float millilitersPerMillisecond
            = totalMilliliters / static_cast<float>(timeDelta);
        float millilitersPerMinute
            = millilitersPerMillisecond * MS_PER_MINUTE;

        m_currentFlowMlPerMinute = static_cast<std::uint32_t>(millilitersPerMinute);
        m_totalImpulseDelta -= lastDataPoint.impulseDelta;
    } else {
        m_currentFlowMlPerMinute = 0;
    }

    vTaskDelay(1);
    HAL_GPIO_WritePin(m_impPort, m_impPin, GPIO_PIN_RESET);
}

void FlowMeterService::enableCounter()
{
    LL_TIM_SetCounter(m_timer->Instance, 0);
    HAL_TIM_Base_Start(m_timer);
}

void FlowMeterService::handleInterval()
{
    // This runs every minute

    auto localTime = Device::get().getLocalTime();
    if (localTime.getDay() != m_midnightTime.getDay()
        || localTime.getMonth() != m_midnightTime.getMonth()
        || localTime.getYear() != m_midnightTime.getYear()) {

        // Reset today's usage
        m_midnightMilliliters = m_totalMilliliters;
        m_midnightTime = localTime;
    }
}

};
