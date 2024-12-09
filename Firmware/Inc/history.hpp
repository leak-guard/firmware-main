#pragma once
#include <array>
#include <cstdint>
#include <functional>

namespace lg {

class HistoryService {
public:
    struct EepromHistoryEntry {
        std::uint32_t timestamp;
        std::uint32_t prevTimestamp;
        std::uint32_t volumeMl;
        std::uint32_t checksum;
    };

    struct FlashHistoryEntry {
        int year;
        int month;
        int day;
        std::uint32_t fromTimestamp;
        std::uint32_t toTimestamp;
        std::array<std::uint32_t, 24> hourVolumesMl;
        std::uint32_t crc;
    };

    HistoryService() = default;

    void initialize();

    void forEachNewestHistoryEntry(std::function<void(std::size_t, const EepromHistoryEntry&)> functor);

private:
    static constexpr auto INVALID_VALUE = 0xFFFFFFFFU;
    static constexpr auto NEWEST_HISTORY_ADDR = 0x8000;

    std::array<EepromHistoryEntry, 2048> m_newestHistory {};
    std::uint32_t m_newestHistoryWriteIndex {};
    std::uint32_t m_newestHistoryLastTimestamp {};
    bool m_disabled {};
    bool m_initialDumpDone {};

    std::uint32_t m_lastTotalVolume {};

    std::size_t m_flashWriteIndex {};
    std::uint32_t m_flashDataUpToTimestamp {};

    void handleInterval();
    void loadNewestHistoryFromEeprom();
    [[nodiscard]] std::uint32_t findNewestHistoryWriteIndex() const;
    [[nodiscard]] std::uint16_t getEepromAddress(std::size_t index) const;
    bool writeNewestHistoryDataPoint();
    static std::uint32_t calculateEntryChecksum(const EepromHistoryEntry& entry);
    static bool isEntryValid(const EepromHistoryEntry& entry);
    void performInitialDumpToFlash();
    void sumUpDayAndWriteToFlash(std::uint32_t timestamp);
    static std::uint32_t calculateFlashEntryCrc(const FlashHistoryEntry& entry);
    bool isFlashEntryOk(const FlashHistoryEntry& entry);
    void findFlashWriteIndex();
};

};
