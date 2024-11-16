#include "u8g2.h"
#include <ui.hpp>

#include <device.hpp>
#include <leakguard/staticstring.hpp>

#include <rtc.h>

namespace lg {

void UiService::uiServiceEntryPoint(void* params)
{
    auto instance = reinterpret_cast<UiService*>(params);
    instance->uiServiceMain();
}

void UiService::initialize()
{
    m_uiServiceTaskHandle = xTaskCreateStatic(
        &UiService::uiServiceEntryPoint /* Task function */,
        "UI Service" /* Task name */,
        m_uiServiceTaskStack.size() /* Stack size */,
        this /* parameters */,
        2 /* Prority */,
        m_uiServiceTaskStack.data() /* Task stack address */,
        &m_uiServiceTaskTcb /* Task control block */
    );
}

void UiService::uiServiceMain()
{
    StaticString<30> timeText;

    while (true) {
        auto oledDriver = Device::get().getOledDriver();
        auto u8g2 = oledDriver->getU8g2Handle();

        timeText.Clear();

        RTC_DateTypeDef rtcDate {};
        RTC_TimeTypeDef rtcTime {};

        HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN);

        if (rtcDate.Date < 10)
            timeText += '0';
        timeText += StaticString<2>::Of(rtcDate.Date);
        timeText += '-';
        if (rtcDate.Month < 10)
            timeText += '0';
        timeText += StaticString<2>::Of(rtcDate.Month);
        timeText += '-';
        if (rtcDate.Year < 10)
            timeText += '0';
        timeText += StaticString<2>::Of(rtcDate.Year);
        timeText += ' ';
        if (rtcTime.Hours < 10)
            timeText += '0';
        timeText += StaticString<2>::Of(rtcTime.Hours);
        timeText += ':';
        if (rtcTime.Minutes < 10)
            timeText += '0';
        timeText += StaticString<2>::Of(rtcTime.Minutes);
        timeText += ':';
        if (rtcTime.Seconds < 10)
            timeText += '0';
        timeText += StaticString<2>::Of(rtcTime.Seconds);

        u8g2_ClearBuffer(u8g2);
        u8g2_SetFont(u8g2, u8g2_font_prospero_bold_nbp_tr);
        u8g2_DrawStr(u8g2, 0, 12, timeText.ToCStr());

        oledDriver->sendBufferDma();
        vTaskDelay(10);
    }
}

};
