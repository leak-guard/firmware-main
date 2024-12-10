#include <ui.hpp>

#include <device.hpp>
#include <leakguard/staticstring.hpp>

// Bitmaps
#define static static const
#include <bitmaps/error.xbm>
#include <bitmaps/hotspot.xbm>
#include <bitmaps/l_per_min.xbm>
#include <bitmaps/splashscreen.xbm>
#include <bitmaps/wateranim_0.xbm>
#include <bitmaps/wateranim_1.xbm>
#include <bitmaps/wateranim_2.xbm>
#include <bitmaps/wateranim_3.xbm>
#include <bitmaps/wifi_conn.xbm>
#include <bitmaps/wifi_pass.xbm>
#include <bitmaps/wifi_sig0.xbm>
#include <bitmaps/wifi_sig1.xbm>
#include <bitmaps/wifi_sig2.xbm>
#include <bitmaps/wifi_sig3.xbm>
#include <bitmaps/wifi_sig4.xbm>
#include <bitmaps/wifi_user.xbm>
#undef static

#include <rtc.h>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

namespace lg {

void UiService::uiServiceEntryPoint(void* params)
{
    auto instance = reinterpret_cast<UiService*>(params);
    instance->uiServiceMain();
}

void UiService::initialize()
{
    m_buttonPressSequence.Append({ 480, 50 });
    m_buttonPressSequence.Append({ 2000, 50 });

    m_uiServiceTaskHandle = xTaskCreateStatic(
        &UiService::uiServiceEntryPoint /* Task function */,
        "UI Service" /* Task name */,
        m_uiServiceTaskStack.size() /* Stack size */,
        this /* Parameters */,
        2 /* Priority */,
        m_uiServiceTaskStack.data() /* Task stack address */,
        &m_uiServiceTaskTcb /* Task control block */
    );
}

void UiService::unlockButtonPressed()
{
    m_unlockButtonPressed = true;
}

void UiService::uiServiceMain()
{
    static constexpr auto DISPLAY_REFRESH_INTERVAL_MS = 50;

    std::uint32_t lastDisplayRefresh = 0;

    while (true) {
        std::uint32_t currentTicks = xTaskGetTickCount();

        refreshLeds();

        if (currentTicks - lastDisplayRefresh >= DISPLAY_REFRESH_INTERVAL_MS) {
            refreshDisplay();
            lastDisplayRefresh = currentTicks;
        }

        if (m_unlockButtonPressed) {
            m_unlockButtonPressed = false;

            {
                auto buzzerService = Device::get().getBuzzerService();
                buzzerService->playSequence(m_buttonPressSequence);
            }

            {
                auto probeService = Device::get().getProbeService();
                probeService->stopAlarm();
            }

            {
                auto valveService = Device::get().getValveService();
                if (valveService->isValveBlocked()) {
                    valveService->unblock();
                } else {
                    valveService->blockDueTo(ValveService::BlockReason::USER_BLOCK);
                }
            }
        }

        vTaskDelay(10);
    }
}

void UiService::refreshLeds()
{
    bool blinkingLit = (xTaskGetTickCount() & 256) > 0;
    auto wifiStatus = Device::get().getSignalStrength();
    bool deviceError = Device::get().hasError();

    bool wifiLit = Device::get().hasWifiStationConnection()
        || (blinkingLit && wifiStatus == Device::SignalStrength::CONNECTING);

    bool valveClosed = Device::get().getValveService()->isValveBlocked();

    ::HAL_GPIO_WritePin(
        LED_WIFI_GPIO_Port, LED_WIFI_Pin, wifiLit ? GPIO_PIN_SET : GPIO_PIN_RESET);
    ::HAL_GPIO_WritePin(
        LED_ERROR_GPIO_Port, LED_ERROR_Pin, deviceError ? GPIO_PIN_SET : GPIO_PIN_RESET);
    ::HAL_GPIO_WritePin(
        LED_OK_GPIO_Port, LED_OK_Pin, (!deviceError) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    ::HAL_GPIO_WritePin(
        LED_VALVE_GPIO_Port, LED_VALVE_Pin, (valveClosed) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void UiService::refreshDisplay()
{
    static constexpr auto SPLASH_SCREEN_TIME_MS = 3000;

    auto oledDriver = Device::get().getOledDriver();
    auto u8g2 = oledDriver->getU8g2Handle();

    if (xTaskGetTickCount() > SPLASH_SCREEN_TIME_MS) {
        m_splashScreenHidden = true;
    }

    if (!m_splashScreenHidden) {
        drawSplashScreen(u8g2);
    } else {
        drawStatusBar(u8g2);

        if (Device::get().hasError()) {
            drawError(u8g2);
        } else if (Device::get().getProbeService()->isInPairingMode()) {
            drawPairing(u8g2);
        } else if (Device::get().getSignalStrength() == Device::SignalStrength::HOTSPOT) {
            drawAccessPointCredentials(u8g2);
        } else {
            drawFlow(u8g2);
        }
    }

    oledDriver->sendBufferDma();
}

void UiService::drawSplashScreen(u8g2_struct* u8g2)
{
    ::u8g2_DrawXBM(u8g2, 0, 0, splashscreen_width, splashscreen_height, splashscreen_bits);
}

void UiService::drawStatusBar(u8g2_struct* u8g2)
{
    std::uint32_t subseconds = 0, secondFraction = 0;
    auto localTime = Device::get().getLocalTime(&subseconds, &secondFraction);

    ::u8g2_ClearBuffer(u8g2);
    ::u8g2_SetFont(u8g2, u8g2_font_crox1cb_mr);
    ::u8g2_DrawGlyph(u8g2, 0, 14, '0' + localTime.getDay() / 10);
    ::u8g2_DrawGlyph(u8g2, 8, 14, '0' + localTime.getDay() % 10);
    ::u8g2_DrawBox(u8g2, 18, 12, 2, 2);
    ::u8g2_DrawGlyph(u8g2, 20, 14, '0' + localTime.getMonth() / 10);
    ::u8g2_DrawGlyph(u8g2, 28, 14, '0' + localTime.getMonth() % 10);

    ::u8g2_DrawGlyph(u8g2, 40, 14, '0' + localTime.getHour() / 10);
    ::u8g2_DrawGlyph(u8g2, 48, 14, '0' + localTime.getHour() % 10);
    if (subseconds > secondFraction / 2) {
        ::u8g2_DrawBox(u8g2, 58, 6, 2, 2);
        ::u8g2_DrawBox(u8g2, 58, 12, 2, 2);
    }
    ::u8g2_DrawGlyph(u8g2, 60, 14, '0' + localTime.getMinute() / 10);
    ::u8g2_DrawGlyph(u8g2, 68, 14, '0' + localTime.getMinute() % 10);
    if (subseconds > secondFraction / 2) {
        ::u8g2_DrawBox(u8g2, 78, 6, 2, 2);
        ::u8g2_DrawBox(u8g2, 78, 12, 2, 2);
    }
    ::u8g2_DrawGlyph(u8g2, 80, 14, '0' + localTime.getSecond() / 10);
    ::u8g2_DrawGlyph(u8g2, 88, 14, '0' + localTime.getSecond() % 10);

    // Draw signal strength
    auto strength = Device::get().getSignalStrength();
    switch (strength) {
    case Device::SignalStrength::HOTSPOT:
        ::u8g2_DrawXBM(u8g2, OledDriver::WIDTH - 16, 1, 16, 16, hotspot_bits);
        break;
    case Device::SignalStrength::CONNECTING:
        ::u8g2_DrawXBM(u8g2, OledDriver::WIDTH - 17, 0, 17, 17, wifi_conn_bits);
        break;
    case Device::SignalStrength::STRENGTH_0:
        ::u8g2_DrawXBM(u8g2, OledDriver::WIDTH - 17, 0, 17, 17, wifi_sig0_bits);
        break;
    case Device::SignalStrength::STRENGTH_1:
        ::u8g2_DrawXBM(u8g2, OledDriver::WIDTH - 17, 0, 17, 17, wifi_sig1_bits);
        break;
    case Device::SignalStrength::STRENGTH_2:
        ::u8g2_DrawXBM(u8g2, OledDriver::WIDTH - 17, 0, 17, 17, wifi_sig2_bits);
        break;
    case Device::SignalStrength::STRENGTH_3:
        ::u8g2_DrawXBM(u8g2, OledDriver::WIDTH - 17, 0, 17, 17, wifi_sig3_bits);
        break;
    case Device::SignalStrength::STRENGTH_4:
        ::u8g2_DrawXBM(u8g2, OledDriver::WIDTH - 17, 0, 17, 17, wifi_sig4_bits);
        break;
    default:
        break;
    }
}

void UiService::drawBox(u8g2_struct* u8g2)
{
    ::u8g2_DrawFrame(u8g2, 8, 22, OledDriver::WIDTH - 16, 38);
    ::u8g2_DrawLine(u8g2, 9, 60, OledDriver::WIDTH - 8, 60);
    ::u8g2_DrawLine(u8g2, OledDriver::WIDTH - 8, 23, OledDriver::WIDTH - 8, 59);
}

void UiService::drawAccessPointCredentials(u8g2_struct* u8g2)
{
    if (m_apSsid.IsEmpty() || m_apPassword.IsEmpty()) {
        auto networkManager = Device::get().getNetworkManager();

        m_apSsid = networkManager->getAccessPointSsid();
        m_apPassword = networkManager->getAccessPointPassword();
    }

    drawBox(u8g2);

    ::u8g2_DrawXBM(u8g2, 12, 27, 12, 12, wifi_user_bits);
    ::u8g2_DrawXBM(u8g2, 12, 43, 12, 12, wifi_pass_bits);

    ::u8g2_SetFont(u8g2, u8g2_font_helvR08_tr);
    ::u8g2_DrawStr(u8g2, 27, 37, m_apSsid.ToCStr());
    ::u8g2_DrawStr(u8g2, 27, 53, m_apPassword.ToCStr());
}

void UiService::drawFlow(u8g2_struct* u8g2)
{
    static constexpr auto ML_FOR_FRAME = 10 * 1200; // 10 ml and 1200 calls in a minute (Every 50ms)
    static const std::array<const unsigned char*, 4> ANIM_FRAMES = {
        wateranim_0_bits, wateranim_1_bits, wateranim_2_bits, wateranim_3_bits
    };

    StaticString<8> flowText;
    std::uint32_t flowMl = Device::get().getFlowMeterService()->getCurrentFlowInMlPerMinute();

    m_waterAnimAccumulator += flowMl;
    while (m_waterAnimAccumulator > ML_FOR_FRAME) {
        m_waterAnimAccumulator -= ML_FOR_FRAME;
        ++m_waterAnimFrame;

        if (m_waterAnimFrame > 3) {
            m_waterAnimFrame = 0;
        }
    }

    if (flowMl > 99999) {
        flowMl = 99999;
    }

    std::uint32_t integral = flowMl / 1000;
    std::uint32_t fraction = (flowMl - integral * 1000) / 10;

    flowText = StaticString<8>::Of(integral);
    flowText += '.';

    auto fractionText = StaticString<2>::Of(fraction);
    if (fractionText.GetSize() == 1) {
        flowText += '0';
    }
    flowText += fractionText;

    ::u8g2_DrawXBM(u8g2, 2, 24, wateranim_0_width, wateranim_0_height, ANIM_FRAMES.at(m_waterAnimFrame));
    ::u8g2_SetFont(u8g2, u8g2_font_profont29_mn);
    auto textWidth = ::u8g2_GetStrWidth(u8g2, flowText.ToCStr());
    ::u8g2_DrawStr(u8g2, 106 - textWidth, 43, flowText.ToCStr());
    ::u8g2_DrawXBM(u8g2, 107, 24, l_per_min_width, l_per_min_height, l_per_min_bits);

    {
        auto networkMgr = Device::get().getNetworkManager();
        const auto& ip = networkMgr->getIpAddress();

        ::u8g2_SetFont(u8g2, u8g2_font_crox1cb_mr);
        ::u8g2_DrawStr(u8g2, 0, 60, "IP:");
        ::u8g2_SetFont(u8g2, u8g2_font_helvR08_tr);
        ::u8g2_DrawStr(u8g2, 30, 60, ip.ToCStr());
    }
}

void UiService::drawError(u8g2_struct* u8g2)
{
    using ArrayType = std::array<const char*, static_cast<int>(Device::ErrorCode::ERROR_TYPE_COUNT)>;
    static const ArrayType ERRORS = {
        "No error.",
        "Unknown error!",
        "WiFi module error!",
        "OLED error!",
        "EEPROM error!",
        "Flash memory error!",
        "LoRa module error!",
    };

    drawBox(u8g2);

    ::u8g2_DrawXBM(u8g2, 54, 25, error_width, error_height, error_bits);
    ::u8g2_SetFont(u8g2, u8g2_font_helvR08_tr);

    const char* textToDraw = ERRORS[0];
    auto error = Device::get().getError();
    if (error < Device::ErrorCode::ERROR_TYPE_COUNT) {
        textToDraw = ERRORS.at(static_cast<int>(error));
    }

    auto width = ::u8g2_GetStrWidth(u8g2, textToDraw);
    ::u8g2_DrawStr(u8g2,
        (OledDriver::WIDTH - width) / 2, 55, textToDraw);
}

void UiService::drawPairing(u8g2_struct* u8g2)
{
    static const char* TEXT = "Press PING on probe.";

    drawBox(u8g2);
    ::u8g2_SetFont(u8g2, u8g2_font_helvR08_tr);

    auto width = ::u8g2_GetStrWidth(u8g2, TEXT);
    ::u8g2_DrawStr(u8g2,
        (OledDriver::WIDTH - width) / 2, 45, TEXT);
}

};

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
