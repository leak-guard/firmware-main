#pragma once
#include <leakguard/circularbuffer.hpp>

#include <cstdint>

#include <FreeRTOS.h>
#include <stm32f7xx_hal.h>
#include <task.h>

namespace lg {

class FlowMeterService {
public:
    static void flowMeterServiceEntryPoint(void* params);

    FlowMeterService(TIM_HandleTypeDef* counterTimer)
        : m_timer(counterTimer)
    {
    }

    void initialize();
    void impulseButtonPressed();

    std::uint32_t getCurrentFlowInMlPerMinute() const { return m_currentFlowMlPerMinute; }

private:
    static constexpr auto HISTORY_LENGTH = 10;
    static constexpr auto MEASUREMENT_INTERVAL_MS = 100;

    struct DataPoint {
        std::uint32_t ticks;
        std::uint32_t impulseDelta;
    };

    void flowMeterServiceMain();
    void updateFlow();
    void enableCounter();

    TIM_HandleTypeDef* m_timer;

    TaskHandle_t m_flowMeterServiceTaskHandle {};
    StaticTask_t m_flowMeterServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 256> m_flowMeterServiceTaskStack {};

    CircularBuffer<DataPoint, HISTORY_LENGTH> m_history;
    std::uint32_t m_prevTicks {};
    std::uint16_t m_prevImpulses {};

    std::uint32_t m_totalImpulseDelta {};

    volatile std::uint32_t m_currentFlowMlPerMinute {};
};

};
