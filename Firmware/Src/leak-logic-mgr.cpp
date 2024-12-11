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
        saveConfiguration();
    } else {
        loadFromString(config.leakLogicConfig);
    }

    m_leakLogicManagerTaskHandle = xTaskCreateStatic(
        &LeakLogicManager::leakLogicManagerEntryPoint /* Task function */,
        "Leak Logic Mgr" /* Task name */,
        m_leakLogicManagerTaskStack.size() /* Stack size */,
        this /* Parameters */,
        6 /* Priority */,
        m_leakLogicManagerTaskStack.data() /* Task stack address */,
        &m_leakLogicManagerTaskTcb /* Task control block */
    );
}

void LeakLogicManager::forceUpdate()
{
    xTaskNotify(m_leakLogicManagerTaskHandle, 1, eSetBits);
}

void LeakLogicManager::leakLogicManagerMain()
{
    while (true) {
        updateSensorState();
        updateLeakLogic();

        xTaskNotifyWait(0, 1, NULL, UPDATE_INTERVAL_MS);
    }
}

void LeakLogicManager::updateSensorState()
{
    {
        auto flowMeterService = Device::get().getFlowMeterService();
        m_sensorState.flowRate = static_cast<float>(
                                     flowMeterService->getCurrentFlowInMlPerMinute())
            / 1000.0f;
    }

    for (auto& entry : m_sensorState.probeStates) {
        entry = false;
    }

    auto probeService = Device::get().getProbeService();
    for (auto& probe : probeService->getPairedProbesInfo()) {
        m_sensorState.probeStates.at(probe.masterAddress) = probe.isAlerted && !probe.isDead;
    }
}

static StaticString<8> ToHex(std::uint32_t in)
{
    static auto alphabet = "0123456789abcdef";
    StaticString<8> out;

    for (int i = 0; i < 8; ++i) {
        out += alphabet[in & 0xF];
        in >>= 4;
    }

    return out;
}

void LeakLogicManager::updateLeakLogic()
{
    auto currentTime = Device::get().getMonotonicTimestamp();
    m_leakLogic.update(m_sensorState, currentTime - m_lastUpdateTime);
    m_lastUpdateTime = currentTime;

    auto action = m_leakLogic.getAction();

    if (action.getActionType() != ActionType::NO_ACTION) {
        auto valveService = Device::get().getValveService();
        auto reason = action.getActionReason() == ActionReason::LEAK_DETECTED_BY_PROBE
            ? ValveService::BlockReason::ALARM_BLOCK
            : ValveService::BlockReason::HEURISTICS_BLOCK;

        if (!valveService->isAlarmed()) {

            valveService->blockDueTo(reason);

            if (action.getActionReason() == ActionReason::LEAK_DETECTED_BY_PROBE) {
                auto networkMgr = Device::get().getNetworkManager();
                StaticString<128> jsonData;
                jsonData += R"({"alert_type":"leak","alert_data":{"reason":"probe_detected_leak","probe_id":)";
                jsonData += StaticString<8>::Of(action.getProbeId());
                jsonData += R"(}})";
                networkMgr->mqttPublishLeak(jsonData.ToCStr());
            } else {
                auto networkMgr = Device::get().getNetworkManager();
                networkMgr->mqttPublishLeak(
                    R"({"alert_type": "leak","alert_data":{"reason":"flow_rate_exceeded"}})");
            }
        }
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
