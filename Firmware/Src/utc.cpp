#include <utc.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>

namespace lg {

UtcTime::UtcTime(std::uint32_t timestamp)
{
    auto lutIter = std::lower_bound(
        MONTH_LUT.rbegin(), MONTH_LUT.rend(), timestamp, std::greater<std::uint32_t> {});
    auto index = std::distance(MONTH_LUT.begin(), lutIter.base()) - 1;

    int years = index / MONTH_PER_YEAR;
    int months = index - years * MONTH_PER_YEAR;
    std::uint32_t remainder = timestamp - *lutIter;

    std::uint32_t days = remainder / (SEC_PER_MIN * MIN_PER_HOUR * HOUR_PER_DAY);
    remainder -= days * (SEC_PER_MIN * MIN_PER_HOUR * HOUR_PER_DAY);

    m_year = EPOCH_YEAR + years;
    m_month = months + 1;
    m_day = static_cast<int>(days + 1);
    m_hour = static_cast<int>(remainder / (SEC_PER_MIN * MIN_PER_HOUR));
    remainder -= m_hour * (SEC_PER_MIN * MIN_PER_HOUR);
    m_minute = static_cast<int>(remainder / SEC_PER_MIN);
    remainder -= m_minute * SEC_PER_MIN;
    m_second = static_cast<int>(remainder);

    calculateWeekday(timestamp);
}

UtcTime UtcTime::fromHAL(const RTC_DateTypeDef& date, const RTC_TimeTypeDef& time)
{
    UtcTime instance;
    instance.m_day = date.Date;
    instance.m_month = date.Month;
    instance.m_year = 2000 + date.Year;
    instance.m_hour = time.Hours;
    instance.m_minute = time.Minutes;
    instance.m_second = time.Seconds;

    instance.calculateWeekday();
    return instance;
}

template <std::size_t N>
static inline int indexOf(const char* in,
    const std::array<const char*, N>& prefixes, std::size_t characters)
{
    for (std::size_t i = 0; i < prefixes.size(); ++i) {
        if (strncmp(prefixes.at(i), in, characters) == 0) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

UtcTime UtcTime::fromAscTime(const char* ascTime)
{
    // Example format: Fri Jun 16 02:03:55 1995[\n]

    static const std::array<const char*, 7> WEEKDAYS = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };

    static const std::array<const char*, 12> MONTHS = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    UtcTime instance {};
    auto len = ::strlen(ascTime);

    if (len != 24 && len != 25) {
        return instance;
    }

    instance.m_weekday = static_cast<WeekDay>(indexOf(ascTime, WEEKDAYS, 3));
    instance.m_month = indexOf(ascTime + 4, MONTHS, 3) + 1;
    instance.m_day = atoi(ascTime + 8);
    instance.m_hour = atoi(ascTime + 11);
    instance.m_minute = atoi(ascTime + 14);
    instance.m_second = atoi(ascTime + 17);
    instance.m_year = atoi(ascTime + 20);

    if (static_cast<int>(instance.m_weekday) == -1) {
        instance.calculateWeekday();
    }

    return instance;
}

std::uint32_t UtcTime::toTimestamp() const
{
    int lutIndex = (m_year - 2000) * MONTH_PER_YEAR + (m_month - 1);

    std::uint32_t timestamp = MONTH_LUT.at(lutIndex);
    timestamp += (m_day - 1) * SEC_PER_MIN * MIN_PER_HOUR * HOUR_PER_DAY;
    timestamp += m_hour * SEC_PER_MIN * MIN_PER_HOUR;
    timestamp += m_minute * SEC_PER_MIN;
    timestamp += m_second;

    return timestamp;
}

void UtcTime::toHAL(RTC_DateTypeDef& date, RTC_TimeTypeDef& time) const
{
    date.Date = m_day;
    date.Month = m_month;
    date.Year = m_year - 2000;
    date.WeekDay = static_cast<std::uint8_t>(m_weekday);

    if (date.WeekDay == 0) {
        date.WeekDay = RTC_WEEKDAY_SUNDAY;
    }

    time.Hours = m_hour;
    time.Minutes = m_minute;
    time.Seconds = m_second;
}

void UtcTime::calculateWeekday()
{
    calculateWeekday(toTimestamp());
}

void UtcTime::calculateWeekday(std::uint32_t timestamp)
{
    std::uint32_t daysSinceEpoch = timestamp / (SEC_PER_MIN * MIN_PER_HOUR * HOUR_PER_DAY);
    m_weekday = static_cast<WeekDay>((static_cast<std::uint32_t>(EPOCH_WEEKDAY) + daysSinceEpoch) % DAY_PER_WEEK);
}

};
