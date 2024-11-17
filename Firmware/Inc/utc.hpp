#pragma once
#include <array>
#include <cstdint>

#include <stm32f7xx_hal.h>

namespace lg {

class UtcTime {
public:
    static constexpr auto SEC_PER_MIN = 60;
    static constexpr auto MIN_PER_HOUR = 60;
    static constexpr auto HOUR_PER_DAY = 24;
    static constexpr auto DAY_PER_WEEK = 7;
    static constexpr auto MONTH_PER_YEAR = 12;

    enum class WeekDay {
        SUNDAY,
        MONDAY,
        TUESDAY,
        WEDNESDAY,
        THURSDAY,
        FRIDAY,
        SATURDAY,
    };

    static constexpr auto EPOCH_WEEKDAY = WeekDay::THURSDAY;
    static constexpr auto EPOCH_YEAR = 1970;

    UtcTime() = default;
    UtcTime(std::uint32_t timestamp);
    static UtcTime FromHAL(const RTC_DateTypeDef& date, const RTC_TimeTypeDef& time);

    [[nodiscard]] std::uint32_t toTimestamp() const;

    void setDay(int newDay)
    {
        m_day = newDay;
        calculateWeekday();
    }

    [[nodiscard]] int getDay() const { return m_day; }

    void setMonth(int newMonth)
    {
        m_month = newMonth;
        calculateWeekday();
    }

    [[nodiscard]] int getMonth() const { return m_month; }

    void setYear(int newYear)
    {
        m_year = newYear;
        calculateWeekday();
    }

    [[nodiscard]] int getYear() const { return m_year; }

    void setHour(int newHour) { m_hour = newHour; }
    [[nodiscard]] int getHour() const { return m_hour; }
    void setMinute(int newMinute) { m_minute = newMinute; }
    [[nodiscard]] int getMinute() const { return m_minute; }
    void setSecond(int newSecond) { m_second = newSecond; }
    [[nodiscard]] int getSecond() const { return m_second; }

    [[nodiscard]] WeekDay getWeekDay() const { return m_weekday; }

private:
    void calculateWeekday();
    void calculateWeekday(std::uint32_t timestamp);

    const static std::array<std::uint32_t, 1572> MONTH_LUT;

    int m_day { 1 };
    int m_month { 1 };
    int m_year { 1970 };
    int m_hour { 0 };
    int m_minute { 0 };
    int m_second { 0 };
    WeekDay m_weekday { EPOCH_WEEKDAY };
};

};
