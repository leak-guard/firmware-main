#pragma once
#include <leakguard/staticvector.hpp>

#include <functional>

#include <FreeRTOS.h>
#include <task.h>

namespace lg {

class CronService {
public:
    static void cronServiceEntryPoint(void* params);

    CronService() = default;

    void initialize();

    void registerJob(std::function<void()> job);
    void notifyCronFromIsr();

private:
    void cronServiceMain();
    void setupAlarmInterrupt();

    StaticVector<std::function<void()>, 16> m_jobsToRun;

    TaskHandle_t m_cronServiceTaskHandle {};
    StaticTask_t m_cronServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 512> m_cronServiceTaskStack {};
};

};
