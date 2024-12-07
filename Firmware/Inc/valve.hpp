#pragma once
#include "utc.hpp"

#include <cstdint>

#include <stm32f7xx_hal.h>

namespace lg {

class ValveService {
public:
    enum class BlockReason {
        USER_BLOCK,
        SCHEDULE_BLOCK,
        HEURISTICS_BLOCK,
        ALARM_BLOCK,
        STARTUP_BLOCK,
        PROBE_DIED_BLOCK,
    };

    ValveService(GPIO_TypeDef* valvePort, std::uint16_t valvePin)
        : m_valvePort(valvePort)
        , m_valvePin(valvePin)
    {
    }

    void initialize();
    void update();

    void blockDueTo(BlockReason reason);
    void unblock();

    [[nodiscard]] bool isValveBlocked() const { return m_blockReason != 0; }

private:
    enum class ScheduleBlockState {
        DISABLED,
        BLOCKED,
        UNBLOCKED,
    };

    GPIO_TypeDef* m_valvePort;
    std::uint16_t m_valvePin;
    std::uint32_t m_blockReason {};
    bool m_scheduleBypass {};

    void handleInterval();
    ScheduleBlockState checkIfBlockedBySchedule(const UtcTime& currentTime);

    void updatePinState();

    [[nodiscard]] bool isBlockedDueTo(BlockReason reason) const
    {
        return (m_blockReason & (1 << static_cast<int>(reason))) > 0;
    }

    void setBlockedDueTo(BlockReason reason)
    {
        m_blockReason |= (1 << static_cast<int>(reason));
    }

    void clearBlockedDueTo(BlockReason reason)
    {
        m_blockReason &= ~(1 << static_cast<int>(reason));
    }
};

};
