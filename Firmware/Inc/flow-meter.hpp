#pragma once
#include <leakguard/circularbuffer.hpp>

#include <utc.hpp>

#include <cstdint>

#include <FreeRTOS.h>
#include <stm32f7xx_hal.h>
#include <task.h>

namespace lg {

class FlowMeterService {
public:
    static void flowMeterServiceEntryPoint(void* params);

    FlowMeterService(TIM_HandleTypeDef* counterTimer, GPIO_TypeDef* impPort, std::uint16_t impPin)
        : m_timer(counterTimer)
        , m_impPort(impPort)
        , m_impPin(impPin)
    {
    }

    void initialize();
    void impulseButtonPressed();

    [[nodiscard]] std::uint32_t getCurrentFlowInMlPerMinute() const { return m_currentFlowMlPerMinute; }
    [[nodiscard]] std::uint32_t getTotalVolumeInMl() const { return m_totalMilliliters; }
    [[nodiscard]] std::uint32_t getTodayFlowInMl() const { return m_totalMilliliters - m_midnightMilliliters; }

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
    void handleInterval();

    TIM_HandleTypeDef* m_timer;
    GPIO_TypeDef* m_impPort;
    std::uint16_t m_impPin;

    TaskHandle_t m_flowMeterServiceTaskHandle {};
    StaticTask_t m_flowMeterServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 256> m_flowMeterServiceTaskStack {};

    CircularBuffer<DataPoint, HISTORY_LENGTH> m_history;
    std::uint32_t m_prevTicks {};
    std::uint16_t m_prevImpulses {};

    std::uint32_t m_totalImpulseDelta {};
    std::uint32_t m_milliliterImpulseRemainder {};

    volatile std::uint32_t m_currentFlowMlPerMinute {};
    volatile std::uint32_t m_totalMilliliters {};
    volatile std::uint32_t m_midnightMilliliters {};
    UtcTime m_midnightTime {};
};

};
