#pragma once

#include <leakguard/leak_logic.hpp>

namespace lg {

class LeakLogicManager {
public:
    static void leakLogicManagerEntryPoint(void* params);

    void initialize();

private:
    static constexpr auto UPDATE_INTERVAL_MS = 60000;

    LeakLogic leakLogic;
    SensorState sensorState { .flowRate=0, .probeStates=StaticVector<bool, 256>() };

    time_t lastUpdateTime = 0;

    void leakLogicManagerMain();
    void updateSensorState();
    void updateLeakLogic();

    bool addCriterion(std::unique_ptr<LeakDetectionCriterion> criterion) { return leakLogic.addCriterion(std::move(criterion)); }
    StaticVector<std::unique_ptr<LeakDetectionCriterion>, LEAK_LOGIC_MAX_CRITERIA>::Iterator getCriteria() { return leakLogic.getCriteria(); }
    bool removeCriterion(const uint8_t index) { return leakLogic.removeCriterion(index); }
    void clearCriteria() { leakLogic.clearCriteria(); }
};

}