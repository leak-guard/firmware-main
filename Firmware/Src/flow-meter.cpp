#include <flow-meter.hpp>

#include <device.hpp>

#include <stm32f7xx_ll_tim.h>

namespace lg {

void FlowMeterService::flowMeterServiceEntryPoint(void* params)
{
    auto instance = reinterpret_cast<FlowMeterService*>(params);
    instance->flowMeterServiceMain();
}

void FlowMeterService::initialize()
{
    m_flowMeterServiceTaskHandle = xTaskCreateStatic(
        &FlowMeterService::flowMeterServiceEntryPoint /* Task function */,
        "Flow Meter Svc" /* Task name */,
        m_flowMeterServiceTaskStack.size() /* Stack size */,
        this /* parameters */,
        6 /* Prority */,
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

void FlowMeterService::flowMeterServiceMain()
{
    while (true) {
        updateFlow();
        vTaskDelay(MEASUREMENT_INTERVAL_MS);
    }
}

void FlowMeterService::updateFlow()
{
    static constexpr auto DEFAULT_IMPULSES_PER_LITER = 500.f;
    static constexpr auto MS_PER_MINUTE = 60000.f;

    std::uint16_t currentImpulses = LL_TIM_GetCounter(m_timer->Instance);
    std::uint32_t currentTicks = xTaskGetTickCount();
    float impulsesPerLiter = 0;

    {
        auto configService = Device::get().getConfigService();
        impulsesPerLiter = static_cast<float>(
            configService->getCurrentConfig().impulsesPerLiter);
    }

    if (impulsesPerLiter == 0) {
        impulsesPerLiter = DEFAULT_IMPULSES_PER_LITER;
    }

    std::uint16_t impulseDelta = currentImpulses - m_prevImpulses;
    m_prevImpulses = currentImpulses;

    DataPoint currentDataPoint = { currentTicks, impulseDelta };
    m_history.PushOne(currentDataPoint);

    m_totalImpulseDelta += impulseDelta;

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
}

void FlowMeterService::enableCounter()
{
    LL_TIM_SetCounter(m_timer->Instance, 0);
    HAL_TIM_Base_Start(m_timer);
}

extern "C" void HAL_GPIO_EXTI_Callback(std::uint16_t pin)
{
    Device::get().getFlowMeterService()->impulseButtonPressed();
}

};
