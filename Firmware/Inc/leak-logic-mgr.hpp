#pragma once

#include <leakguard/leak_logic.hpp>

namespace lg {

class LeakLogicManager {
public:
    static void leakLogicManagerEntryPoint(void* params);

    void initialize();

private:
    LeakLogic leakLogic;
    SensorState sensorState;

    void update();
};

}