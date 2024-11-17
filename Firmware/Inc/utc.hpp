#pragma once
#include <array>
#include <cstdint>

#include <stm32f7xx_hal.h>

namespace lg {

class UtcTime {
public:
    enum class WeekDay {
        SUNDAY,
        MONDAY,
        TUESDAY,
        WEDNESDAY,
        THURSDAY,
        FRIDAY,
        SATURDAY,
    };

    UtcTime() = default;
    UtcTime(std::uint32_t timestamp);
    static UtcTime FromHAL(const RTC_DateTypeDef& date, const RTC_TimeTypeDef& time);

private:
    const static std::array<std::uint32_t, 1572> MONTH_LUT;

    int m_day { 1 };
    int m_month { 1 };
    int m_year { 1970 };
    int m_hour { 0 };
    int m_minute { 0 };
    int m_second { 0 };
    WeekDay m_weekday { WeekDay::THURSDAY };
};

};
