#pragma once
#include <array>

#include <FreeRTOS.h>
#include <task.h>

namespace lg {

class UiService {
public:
    static void uiServiceEntryPoint(void* params);

    UiService() = default;

    void initialize();

private:
    void uiServiceMain();

    TaskHandle_t m_uiServiceTaskHandle {};
    StaticTask_t m_uiServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 1024> m_uiServiceTaskStack {};
};

};
