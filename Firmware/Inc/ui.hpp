#pragma once
#include <leakguard/staticstring.hpp>

#include <array>
#include <cstdint>

#include <FreeRTOS.h>
#include <buzzer.hpp>
#include <task.h>

// Forward declarations
struct u8g2_struct;

namespace lg {

class UiService {
public:
    static void uiServiceEntryPoint(void* params);

    UiService() = default;

    void initialize();

    void unlockButtonPressed();

private:
    void uiServiceMain();
    void refreshLeds();
    void refreshDisplay();

    void drawSplashScreen(u8g2_struct* u8g2);
    void drawStatusBar(u8g2_struct* u8g2);
    void drawBox(u8g2_struct* u8g2);
    void drawAccessPointCredentials(u8g2_struct* u8g2);
    void drawFlow(u8g2_struct* u8g2);
    void drawError(u8g2_struct* u8g2);
    void drawPairing(u8g2_struct* u8g2);

    TaskHandle_t m_uiServiceTaskHandle {};
    StaticTask_t m_uiServiceTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 1024> m_uiServiceTaskStack {};

    bool m_splashScreenHidden { false };
    StaticString<32> m_apSsid;
    StaticString<64> m_apPassword;

    std::uint32_t m_waterAnimAccumulator {};
    std::uint32_t m_waterAnimFrame {};
    std::uint32_t m_frameCounter {};

    volatile bool m_unlockButtonPressed { false };

    BuzzerService::ToneSequence m_buttonPressSequence {};
};

};
