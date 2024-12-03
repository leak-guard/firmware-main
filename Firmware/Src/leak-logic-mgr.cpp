#include <device.hpp>
#include <leak-logic-mgr.hpp>

namespace lg {

void LeakLogicManager::leakLogicManagerEntryPoint(void* params)
{
    auto instance = reinterpret_cast<LeakLogicManager*>(params);
    instance->leakLogicManagerMain();
}

void LeakLogicManager::initialize()
{
    lastUpdateTime = Device::get().getLocalTime().toTimestamp();
}

void LeakLogicManager::leakLogicManagerMain()
{
    while (true) {
        updateSensorState();
        updateLeakLogic();

        vTaskDelay(UPDATE_INTERVAL_MS);
    }
}

void LeakLogicManager::updateSensorState()
{
    auto flowMeterService = Device::get().getFlowMeterService();
    sensorState.flowRate = flowMeterService->getCurrentFlowInMlPerMinute() / 1000.0f;

    // TODO: Hook up probe service
}

void LeakLogicManager::updateLeakLogic()
{
    auto currentTime = Device::get().getLocalTime().toTimestamp();
    leakLogic.update(sensorState, currentTime - lastUpdateTime);
    lastUpdateTime = currentTime;

    auto action = leakLogic.getAction();
    if (action.getActionType() != ActionType::NO_ACTION) {
        // TODO: Alert
    }
}

}