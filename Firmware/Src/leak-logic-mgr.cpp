#include <device.hpp>
#include <gpio.h>
#include <leak-logic-mgr.hpp>

namespace lg {

void LeakLogicManager::leakLogicManagerEntryPoint(void* params)
{
    auto instance = reinterpret_cast<LeakLogicManager*>(params);
    instance->leakLogicManagerMain();
}

void LeakLogicManager::initialize()
{
    m_lastUpdateTime = Device::get().getLocalTime().toTimestamp();

    auto configService = Device::get().getConfigService();
    auto& config = configService->getCurrentConfig();

    if (config.leakLogicConfig.IsEmpty()) {
        // TODO: Remove this - for testing only
        addCriterion(std::make_unique<TimeBasedFlowRateCriterion>(1, 5));
        saveConfiguration();
    } else {
        loadFromString(config.leakLogicConfig);
    }

    m_leakLogicManagerTaskHandle = xTaskCreateStatic(
        &LeakLogicManager::leakLogicManagerEntryPoint /* Task function */,
        "Leak Logic Mgr" /* Task name */,
        m_leakLogicManagerTaskStack.size() /* Stack size */,
        this /* parameters */,
        6 /* Prority */,
        m_leakLogicManagerTaskStack.data() /* Task stack address */,
        &m_leakLogicManagerTaskTcb /* Task control block */
    );
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
    m_sensorState.flowRate = flowMeterService->getCurrentFlowInMlPerMinute() / 1000.0f;

    // TODO: Hook up probe service
}

void LeakLogicManager::updateLeakLogic()
{
    auto currentTime = Device::get().getMonotonicTimestamp();
    m_leakLogic.update(m_sensorState, currentTime - m_lastUpdateTime);
    m_lastUpdateTime = currentTime;

    auto action = m_leakLogic.getAction();
    if (action.getActionType() != ActionType::NO_ACTION) {
        // TODO: Talk to valve service
        HAL_GPIO_TogglePin(LED_VALVE_GPIO_Port, LED_VALVE_Pin);
    }
}

void LeakLogicManager::saveConfiguration() const
{
    const auto serializedConfig = getCriteriaString();
    auto configService = Device::get().getConfigService();
    auto& config = configService->getCurrentConfig();

    config.leakLogicConfig = serializedConfig;

    configService->commit();
}

void LeakLogicManager::loadConfiguration()
{
    const auto serializedConfig = Device::get().getConfigService()->getCurrentConfig().leakLogicConfig;
    loadFromString(serializedConfig);
}

}