#include <device.hpp>
#include <optional>

#include <gpio.h>
#include <i2c.h>
#include <quadspi.h>
#include <rtc.h>
#include <spi.h>
#include <tim.h>
#include <usart.h>

#include <stm32f7xx_ll_i2c.h>

extern "C" {
#include <utz.h>
};

namespace lg {

std::optional<Device> Device::m_instance;

Device::Device()
    : m_currentZone(std::make_unique<uzone_t>())
    , m_eepromDriver(&hi2c2, EEPROM_WP_GPIO_Port, EEPROM_WP_Pin)
    , m_espDriver(&huart1)
    , m_flashDriver(&hqspi)
    , m_oledDriver(&hi2c1)
    , m_buzzerDriver(&htim3, TIM_CHANNEL_1)
    , m_loraDriver(&hspi1, LORA_DIO0_GPIO_Port, LORA_DIO0_Pin, LORA_NSS_GPIO_Port, LORA_NSS_Pin, LORA_RESET_GPIO_Port, LORA_RESET_Pin)
    , m_valveService(VALVE_OUT_GPIO_Port, VALVE_OUT_Pin)
    , m_flowMeterService(&htim1, LED_IMP_GPIO_Port, LED_IMP_Pin)
    , m_buzzerService(&htim7)
{
}

Device& Device::get()
{
    if (!m_instance.has_value()) {
        m_instance.emplace();
    }

    return m_instance.value();
}

void Device::initializeDrivers()
{
    m_eepromDriver->initialize();
    m_espDriver.initialize();
    m_flashDriver->initialize();
    m_oledDriver->initialize();
    m_buzzerDriver->initialize();

    m_cronService->initialize();
    m_configService->initialize();
    m_networkManager->initialize();
    m_server->initialize();
    m_valveService->initialize();
    m_uiService->initialize();
    m_flowMeterService->initialize();
    m_loraService->initialize();
    m_leakLogicManager->initialize();
    m_probeService->initialize();
    m_buzzerService->initialize();

    if (!setLocalTimezone(m_configService->getCurrentConfig().timezoneId)) {
        setLocalTimezone("London");
    }
}

void Device::setError(ErrorCode code)
{
    m_error = code;
}

void Device::updateRtcTime(const UtcTime& newTime)
{
    m_timeIsValid = true;

    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;

    newTime.toHAL(date, time);

    if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK) {
        HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
    }

    getValveService()->update();
}

bool Device::setLocalTimezone(const char* timezoneName)
{
    uzone_t tempZone {};

    // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
    ::get_zone_by_name(const_cast<char*>(timezoneName), &tempZone);
    // NOLINTEND(cppcoreguidelines-pro-type-const-cast)

    if (!tempZone.name) {
        return false;
    }

    portENTER_CRITICAL();
    ::memcpy(m_currentZone.get(), &tempZone, sizeof(tempZone));
    portEXIT_CRITICAL();

    return true;
}

bool Device::setLocalTimezone(std::uint32_t timezoneId)
{
    static constexpr auto ZONE_COUNT = 46;
    static const char* ZONE_NAME = "Local Time";

    if (timezoneId >= ZONE_COUNT) {
        return false;
    }

    uzone_t tempZone {};
    ::unpack_zone(&zone_defns[timezoneId], ZONE_NAME, &tempZone);

    portENTER_CRITICAL();
    ::memcpy(m_currentZone.get(), &tempZone, sizeof(tempZone));
    portEXIT_CRITICAL();

    return true;
}

UtcTime Device::getLocalTime(
    std::uint32_t* subseconds, std::uint32_t* secondFraction)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;

    auto result1 = HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    auto result2 = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    if (result1 != HAL_OK || result2 != HAL_OK) {
        setError(ErrorCode::UNKNOWN_ERROR);
        return UtcTime {};
    }

    if (subseconds) {
        *subseconds = time.SubSeconds;
    }

    if (secondFraction) {
        *secondFraction = time.SecondFraction;
    }

    uzone_t tempZone;

    portENTER_CRITICAL();
    ::memcpy(&tempZone, m_currentZone.get(), sizeof(tempZone));
    portEXIT_CRITICAL();

    std::uint32_t timestamp = UtcTime::fromHAL(date, time).toTimestamp();

    udatetime_t tzTime {};
    tzTime.date.year = date.Year;
    tzTime.date.month = date.Month;
    tzTime.date.dayofmonth = date.Date;
    tzTime.time.hour = time.Hours;
    tzTime.time.minute = time.Minutes;
    tzTime.time.second = time.Seconds;

    uoffset_t offset;
    ::get_current_offset(&tempZone, &tzTime, &offset);

    std::int32_t timestampOffset = offset.hours * 60 * 60 + offset.minutes * 60;

    return UtcTime { timestamp + timestampOffset };
}

std::uint32_t Device::getMonotonicTimestamp() const
{
    std::uint32_t ticks = xTaskGetTickCount();
    std::uint32_t delta = ticks - m_monotonicLastTicks + m_monotonicRemainder;
    std::uint32_t wholeSeconds = delta / 1000;

    m_monotonicTime += wholeSeconds;
    m_monotonicRemainder = delta - wholeSeconds * 1000;
    m_monotonicLastTicks = ticks;

    return m_monotonicTime;
}

};

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while,cppcoreguidelines-pro-type-cstyle-cast)

extern "C" void I2C2_EV_IRQHandler(void)
{
    bool complete = LL_I2C_IsActiveFlag_STOP(hi2c2.Instance);
    bool tx = HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_BUSY_TX;

    HAL_I2C_EV_IRQHandler(&hi2c2);

    if (complete) {
        if (hi2c2.State == HAL_I2C_STATE_READY) {
            auto higherPriorityWoken
                = lg::Device::get().getEepromDriver()->notifyDmaFinishedFromIsr(tx);
            portYIELD_FROM_ISR(higherPriorityWoken);
        }
    }
}

// NOLINTEND(cppcoreguidelines-avoid-do-while,cppcoreguidelines-pro-type-cstyle-cast)
