#include <utc.hpp>

#include <algorithm>
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

UtcTime UtcTime::FromHAL(const RTC_DateTypeDef& date, const RTC_TimeTypeDef& time)
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
