#pragma once

#include <leakguard/leak_logic.hpp>

namespace lg {

class LeakLogicManager {
public:
    static void leakLogicManagerEntryPoint(void* params);

    void initialize();

    [[nodiscard]] StaticString<64> getCriteriaString() const { return m_leakLogic.serialize(); }
    void loadFromString(const StaticString<64>& criteriaString) { m_leakLogic.loadFromString(criteriaString); }

private:
    static constexpr auto UPDATE_INTERVAL_MS = 1000;

    LeakLogic m_leakLogic;
    SensorState m_sensorState { .flowRate = 0, .probeStates = std::array<bool, 256>() };

    time_t m_lastUpdateTime = 0;

    TaskHandle_t m_leakLogicManagerTaskHandle {};
    StaticTask_t m_leakLogicManagerTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 256> m_leakLogicManagerTaskStack {};

    void leakLogicManagerMain();
    void updateSensorState();
    void updateLeakLogic();

    bool addCriterion(std::unique_ptr<LeakDetectionCriterion> criterion) { return m_leakLogic.addCriterion(std::move(criterion)); }
    StaticVector<std::unique_ptr<LeakDetectionCriterion>, LEAK_LOGIC_MAX_CRITERIA>::Iterator getCriteria() { return m_leakLogic.getCriteria(); }
    bool removeCriterion(const uint8_t index) { return m_leakLogic.removeCriterion(index); }
    void clearCriteria() { m_leakLogic.clearCriteria(); }

    void saveConfiguration() const;
    void loadConfiguration();
};

}