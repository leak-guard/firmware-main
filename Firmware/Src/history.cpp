#include <history.hpp>

#include <device.hpp>

#include <crc.h>

namespace lg {

static inline bool areTheSameDay(const UtcTime& date1, const UtcTime& date2)
{
    return date1.getDay() == date2.getDay()
        && date1.getMonth() == date2.getMonth()
        && date1.getYear() == date2.getYear();
}

void HistoryService::initialize()
{
    Device::get().getCronService()->registerJob([this] {
        Device::get().getHistoryService()->handleInterval();
    });

    loadNewestHistoryFromEeprom();
    findFlashWriteIndex();

    std::uint32_t totalVolumeMl = 0;
    for (auto& entry : m_newestHistory) {
        if (isEntryValid(entry) && entry.totalMl > totalVolumeMl) {
            totalVolumeMl = entry.totalMl;
        }
    }

    Device::get().getFlowMeterService()->setTotalVolumeInMl(totalVolumeMl);
    m_lastTotalVolume = totalVolumeMl;
}

void HistoryService::timeUpdated()
{
    if (m_initialTimeDone) {
        return;
    }

    auto currentTime = Device::get().getLocalTime();
    std::uint32_t todayTotalMl = 0;

    for (auto& entry : m_newestHistory) {
        auto entryLocalTime = Device::get().getLocalTimeForUtcTimestamp(entry.timestamp);
        if (isEntryValid(entry) && areTheSameDay(currentTime, entryLocalTime)) {
            todayTotalMl += entry.volumeMl;
        }
    }

    Device::get().getFlowMeterService()->setInitialTodayVolumeInMl(todayTotalMl);

    m_initialTimeDone = true;
}

void HistoryService::forEachNewestHistoryEntry(
    std::function<void(std::size_t, const EepromHistoryEntry&)> functor)
{
    auto localTime = Device::get().getLocalTime();

    auto readIndex = m_newestHistoryWriteIndex;
    std::size_t functorCallCount = 0;

    for (std::size_t processed = 0; processed < m_newestHistory.size(); ++processed) {
        auto& entry = m_newestHistory.at(readIndex);

        if (isEntryValid(entry)) {
            auto entryTime = Device::get().getLocalTimeForUtcTimestamp(entry.timestamp);

            if (areTheSameDay(entryTime, localTime)) {
                functor(functorCallCount++, entry);
            }
        }

        ++readIndex;
        if (readIndex >= m_newestHistory.size()) {
            readIndex = 0;
        }
    }
}

void HistoryService::forEachFlashHistoryEntry(
    std::uint32_t fromTimestamp, std::uint32_t toTimestamp,
    std::function<void(std::size_t, const FlashHistoryEntry&)> functor)
{
    std::size_t functorCallCount = 0;
    auto flash = Device::get().getFlashDriver();

    for (size_t i = 0; i < m_flashWriteIndex; ++i) {
        auto entry = reinterpret_cast<FlashHistoryEntry*>(flash->getPageAddress(i));

        if (entry->toTimestamp < fromTimestamp || entry->fromTimestamp > toTimestamp) {
            continue;
        }

        if (!isFlashEntryOk(*entry)) {
            continue;
        }

        functor(functorCallCount++, *entry);
    }
}

void HistoryService::handleInterval()
{
    if (m_disabled) {
        return;
    }

    if (!Device::get().isTimeValid()) {
        return;
    }

    if (!m_initialDumpDone) {
        performInitialDumpToFlash();
        m_initialDumpDone = true;
    }

    bool nextDay = writeNewestHistoryDataPoint();

    if (nextDay) {
        auto timestamp = Device::get().getLocalTime().toTimestamp() - 43200;
        sumUpDayAndWriteToFlash(timestamp);
    }
}

void HistoryService::loadNewestHistoryFromEeprom()
{
    auto eepromDriver = Device::get().getEepromDriver();
    if (!eepromDriver->readObject(getEepromAddress(0), m_newestHistory)) {
        Device::get().setError(Device::ErrorCode::EEPROM_ERROR);
        m_disabled = true;
        return;
    }

    m_newestHistoryWriteIndex = findNewestHistoryWriteIndex();
}

std::uint32_t HistoryService::findNewestHistoryWriteIndex() const
{
    std::uint32_t lastTimestamp = 0;

    for (std::size_t i = 0; i < m_newestHistory.size(); ++i) {
        auto& entry = m_newestHistory.at(i);

        if (!isEntryValid(entry)) {
            // Set write index to first invalid cell
            return i;
        }

        if (entry.timestamp <= lastTimestamp) {
            // Set write index to first timestamp incontinuity
            return i;
        }

        lastTimestamp = entry.timestamp;
    }

    // If timestamps are monotonic and there are no empty cells,
    // the buffer just wrapped around
    return 0;
}

std::uint16_t HistoryService::getEepromAddress(std::size_t index) const
{
    return NEWEST_HISTORY_ADDR + index * sizeof(EepromHistoryEntry);
}

bool HistoryService::writeNewestHistoryDataPoint()
{
    auto utcTime = Device::get().getUtcTime();
    auto currentTotalVolume
        = Device::get().getFlowMeterService()->getTotalVolumeInMl();
    auto timestamp = utcTime.toTimestamp();
    auto localTime = Device::get().getLocalTimeForUtcTimestamp(timestamp);

    if (timestamp <= m_newestHistoryLastTimestamp) {
        return false;
    }

    std::uint32_t volumeDelta = currentTotalVolume - m_lastTotalVolume;
    m_lastTotalVolume = currentTotalVolume;

    auto& entry = m_newestHistory.at(m_newestHistoryWriteIndex);
    entry.timestamp = timestamp;
    entry.totalMl = currentTotalVolume;
    entry.volumeMl = volumeDelta;
    entry.checksum = calculateEntryChecksum(entry);

    {
        auto eepromDriver = Device::get().getEepromDriver();
        eepromDriver->enableWrites();
        if (!eepromDriver->writeSmallObject(
                getEepromAddress(m_newestHistoryWriteIndex), entry)) {

            Device::get().setError(Device::ErrorCode::EEPROM_ERROR);
            m_disabled = true;
        }
        eepromDriver->disableWrites();
    }

    ++m_newestHistoryWriteIndex;
    if (m_newestHistoryWriteIndex >= m_newestHistory.size()) {
        m_newestHistoryWriteIndex = 0;
    }

    auto prevDayTime = Device::get().getLocalTimeForUtcTimestamp(
        m_newestHistoryLastTimestamp);
    m_newestHistoryLastTimestamp = timestamp;
    return !areTheSameDay(localTime, prevDayTime);
}

std::uint32_t HistoryService::calculateEntryChecksum(const EepromHistoryEntry& entry)
{
    return entry.timestamp ^ entry.totalMl ^ entry.volumeMl ^ 0x89ABCDEF;
}

bool HistoryService::isEntryValid(const EepromHistoryEntry& entry)
{
    return entry.timestamp != INVALID_VALUE
        && entry.totalMl != INVALID_VALUE
        && entry.volumeMl != INVALID_VALUE
        && entry.checksum == calculateEntryChecksum(entry);
}

void HistoryService::performInitialDumpToFlash()
{
    auto utcTime = Device::get().getUtcTime();
    auto currentTimestamp = utcTime.toTimestamp();
    auto localTime = Device::get().getLocalTimeForUtcTimestamp(currentTimestamp);
    std::uint32_t latestTimestamp = 0;

    for (auto& entry : m_newestHistory) {
        if (!isEntryValid(entry)) {
            continue;
        }

        if (entry.timestamp > latestTimestamp) {
            latestTimestamp = entry.timestamp;
        }
    }

    if (currentTimestamp < latestTimestamp) {
        return;
    }

    // If the latest timestamp is before today, we dump data, otherwise not

    auto latestTime = Device::get().getLocalTimeForUtcTimestamp(latestTimestamp);
    if (!areTheSameDay(latestTime, localTime)) {
        sumUpDayAndWriteToFlash(latestTimestamp);
    }
}

void HistoryService::sumUpDayAndWriteToFlash(std::uint32_t localTimestamp)
{
    UtcTime localTime(localTimestamp);
    FlashHistoryEntry historyEntry {};
    historyEntry.fromTimestamp = INVALID_VALUE;

    for (auto& entry : m_newestHistory) {
        if (!isEntryValid(entry)) {
            continue;
        }

        auto entryTime = Device::get().getLocalTimeForUtcTimestamp(entry.timestamp);
        if (!areTheSameDay(localTime, entryTime)) {
            continue;
        }

        int hour = entryTime.getHour();
        if (hour < 0 || hour >= 24) {
            continue;
        }

        historyEntry.hourVolumesMl.at(hour) += entry.volumeMl;

        if (entry.timestamp < historyEntry.fromTimestamp) {
            historyEntry.fromTimestamp = entry.timestamp;
        }

        if (entry.timestamp > historyEntry.toTimestamp) {
            historyEntry.toTimestamp = entry.timestamp;
        }
    }

    if (historyEntry.fromTimestamp == INVALID_VALUE) {
        // No data found
        return;
    }

    historyEntry.year = localTime.getYear();
    historyEntry.month = localTime.getMonth();
    historyEntry.day = localTime.getDay();
    historyEntry.crc = calculateFlashEntryCrc(historyEntry);

    if (m_flashDataUpToTimestamp > historyEntry.fromTimestamp) {
        return;
    }

    m_flashDataUpToTimestamp = historyEntry.toTimestamp;

    {
        auto flashDriver = Device::get().getFlashDriver();
        if (!flashDriver->writeObject(m_flashWriteIndex++, historyEntry)) {
            Device::get().setError(Device::ErrorCode::FLASH_ERROR);
            m_disabled = true;
        }
    }
}

std::uint32_t HistoryService::calculateFlashEntryCrc(const FlashHistoryEntry& entry)
{
    portDISABLE_INTERRUPTS();
    auto crc = HAL_CRC_Calculate(&hcrc,
        const_cast<std::uint32_t*>(reinterpret_cast<const std::uint32_t*>(&entry)),
        sizeof(FlashHistoryEntry) / sizeof(std::uint32_t) - 1);
    portENABLE_INTERRUPTS();

    return crc;
}

bool HistoryService::isFlashEntryOk(const FlashHistoryEntry& entry)
{
    return entry.crc == calculateFlashEntryCrc(entry);
}

void HistoryService::findFlashWriteIndex()
{
    auto flashDriver = Device::get().getFlashDriver();
    m_flashWriteIndex = 0;
    m_flashDataUpToTimestamp = 0;

    while (m_flashWriteIndex < FlashDriver::FLASH_PAGE_COUNT) {
        auto pageAddr = flashDriver->getPageAddress(m_flashWriteIndex);
        auto pageWordAddr = reinterpret_cast<std::uint32_t*>(pageAddr);
        bool empty = true;

        for (size_t i = 0; i < FlashDriver::FLASH_PAGE_SIZE / sizeof(std::uint32_t); ++i) {
            if (pageWordAddr[i] != INVALID_VALUE) {
                empty = false;
                break;
            }
        }

        if (empty) {
            break;
        }

        auto entryPtr = reinterpret_cast<const FlashHistoryEntry*>(pageWordAddr);
        if (isFlashEntryOk(*entryPtr)) {
            if (m_flashDataUpToTimestamp < entryPtr->toTimestamp) {
                m_flashDataUpToTimestamp = entryPtr->toTimestamp;
            }
        }

        ++m_flashWriteIndex;
    }

    if (m_flashWriteIndex == FlashDriver::FLASH_PAGE_COUNT) {
        m_disabled = true;
    }
}

void HistoryService::clearEepromHistory()
{
    auto eepromDriver = Device::get().getEepromDriver();
    eepromDriver->enableWrites();

    for (std::size_t i = 0; i < m_newestHistory.size(); ++i) {
        m_newestHistory.at(i).checksum ^= 0xFFFFFFFF;

        eepromDriver->writeSmallObject(getEepromAddress(i), m_newestHistory.at(i));
    }

    eepromDriver->disableWrites();
}

}
