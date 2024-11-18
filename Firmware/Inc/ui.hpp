#pragma once
#include <leakguard/staticstring.hpp>

#include <array>

#include <FreeRTOS.h>
#include <task.h>

// Forward declarations
struct u8g2_struct;

namespace lg {

class UiService {
public:
    static void uiServiceEntryPoint(void* params);

    UiService() = default;

    void initialize();

private:
    void uiServiceMain();
    void refreshDisplay();

    void drawSplashScreen(u8g2_struct* u8g2);
    void drawStatusBar(u8g2_struct* u8g2);
    void drawAccessPointCredentials(u8g2_struct* u8g2);

    TaskHandle_t m_uiServiceTaskHandle {};
    StaticTask_t m_uiServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 1024> m_uiServiceTaskStack {};

    bool m_splashScreenHidden { false };
    StaticString<32> m_apSsid;
    StaticString<64> m_apPassword;
};

};
