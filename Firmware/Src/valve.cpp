#include <valve.hpp>

#include <device.hpp>

namespace lg {

void ValveService::initialize()
{
    Device::get().getCronService()->registerJob(
        [this] { Device::get().getValveService()->handleInterval(); });

    setBlockedDueTo(BlockReason::STARTUP_BLOCK);
    updatePinState();
}

void ValveService::update()
{
    if (isBlockedDueTo(BlockReason::STARTUP_BLOCK)) {
        clearBlockedDueTo(BlockReason::STARTUP_BLOCK);
    }

    auto localTime = Device::get().getLocalTime();
    auto scheduleBlock = checkIfBlockedBySchedule(localTime);

    if (scheduleBlock != ScheduleBlockState::DISABLED) {
        bool blockState = scheduleBlock == ScheduleBlockState::BLOCKED;

        if (isBlockedDueTo(BlockReason::SCHEDULE_BLOCK) && !blockState) {
            clearBlockedDueTo(BlockReason::SCHEDULE_BLOCK);
        } else if (!isBlockedDueTo(BlockReason::SCHEDULE_BLOCK) && blockState) {
            setBlockedDueTo(BlockReason::SCHEDULE_BLOCK);
        }
    }

    updatePinState();
}

void ValveService::blockDueTo(BlockReason reason)
{
    if (reason == BlockReason::SCHEDULE_BLOCK || reason == BlockReason::STARTUP_BLOCK) {
        // Only ValveService can manage blocks due to schedule and startup
        return;
    }

    setBlockedDueTo(reason);
    update();
}

void ValveService::unblock()
{
    if (isBlockedDueTo(BlockReason::USER_BLOCK)) {
        clearBlockedDueTo(BlockReason::USER_BLOCK);
    }

    if (isBlockedDueTo(BlockReason::SCHEDULE_BLOCK)) {
        m_scheduleBypass = true;
        clearBlockedDueTo(BlockReason::SCHEDULE_BLOCK);
    }

    if (isBlockedDueTo(BlockReason::HEURISTICS_BLOCK)) {
        clearBlockedDueTo(BlockReason::HEURISTICS_BLOCK);
    }

    if (isBlockedDueTo(BlockReason::ALARM_BLOCK)) {
        clearBlockedDueTo(BlockReason::ALARM_BLOCK);
    }

    if (isBlockedDueTo(BlockReason::PROBE_DIED_BLOCK)) {
        clearBlockedDueTo(BlockReason::PROBE_DIED_BLOCK);
    }

    update();
}

void ValveService::handleInterval()
{
    update();
}

auto ValveService::checkIfBlockedBySchedule(const UtcTime& currentTime) -> ScheduleBlockState
{
    auto config = Device::get().getConfigService();
    auto& currentConfig = config->getCurrentConfig();

    int weekdayId = static_cast<int>(currentTime.getWeekDay());
    if (weekdayId >= UtcTime::DAY_PER_WEEK) {
        return ScheduleBlockState::DISABLED;
    }

    std::uint32_t configWord = currentConfig.weeklySchedule.at(weekdayId);

    if (!(configWord & ConfigService::BLOCKADE_ENABLED_FLAG)) {
        m_scheduleBypass = false;
        return ScheduleBlockState::DISABLED;
    }

    bool blocked = (configWord & (1 << currentTime.getHour())) != 0;

    if (!blocked) {
        m_scheduleBypass = false;
    }

    if (m_scheduleBypass) {
        return ScheduleBlockState::UNBLOCKED;
    }

    return blocked ? ScheduleBlockState::BLOCKED : ScheduleBlockState::UNBLOCKED;
}

void ValveService::updatePinState()
{
    bool isNc = false;
    GPIO_PinState targetState = GPIO_PIN_RESET;

    {
        auto config = Device::get().getConfigService();
        isNc = config->getCurrentConfig().valveTypeNC;
    }

    if (isValveBlocked()) {
        targetState = isNc ? GPIO_PIN_RESET : GPIO_PIN_SET;
    } else {
        targetState = isNc ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }

    HAL_GPIO_WritePin(m_valvePort, m_valvePin, targetState);
}

};
