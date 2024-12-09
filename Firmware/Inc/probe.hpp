#pragma once
#include <cstdint>
#include <limits>

#include <leakguard/staticvector.hpp>

namespace lg {

enum class MsgType : std::uint8_t {
    PING = 0,
    BATTERY = 1,
    ALARM = 2
};

struct ProbeMessage {
    MsgType messageType;
    uint8_t dipId;
    uint16_t batMvol;
    uint32_t uid1;
    uint32_t uid2;
    uint32_t uid3;
    uint32_t crc;
};

class ProbeService {
public:
    static constexpr auto MAX_PROBES = 256;
    static constexpr auto INVALID_RSSI = std::numeric_limits<std::int32_t>::max();

    struct ProbeInfo {
        std::uint32_t id1 {};
        std::uint32_t id2 {};
        std::uint32_t id3 {};
        std::uint8_t masterAddress {};
        std::uint8_t batteryPercent {};
        std::uint16_t batteryMv {};
        std::uint32_t lastPingTicks {};
        std::int32_t lastRssi {};
        bool isAlerted {};
        bool isDead {};
        bool isIgnored {};
    };

    ProbeService() = default;

    void initialize();
    void receivePacket(const ProbeMessage& packet, std::int32_t rssi);

    [[nodiscard]] bool isInPairingMode() const { return m_pairingMode; }
    bool enterPairingMode();
    bool leavePairingMode();

    bool unpairProbe(std::uint8_t masterAddress);

    [[nodiscard]] const ProbeInfo* getPairedProbeInfo(std::uint8_t masterAddress) const;

    [[nodiscard]] const StaticVector<ProbeInfo, MAX_PROBES>&
    getPairedProbesInfo() const { return m_pairedProbes; }

    bool setProbeIgnored(std::uint8_t masterAddress, bool ignored);

    void stopAlarm();

private:
    static constexpr auto MAX_PAIRING_TIME_MS = 120000; // 2 min
    static constexpr auto NUM_ALKALINE_CELLS = 2;

    static constexpr auto MAX_PROBE_INACTIVITY_MS = 2 * 24 * 60 * 60 * 1000; // 2 days

    bool m_pairingMode {};
    std::uint32_t m_pairingModeEnterTime {};
    StaticVector<ProbeInfo, MAX_PROBES> m_pairedProbes;

    bool verifyChecksum(const ProbeMessage& packet);
    void intervalHandler();
    void checkAllProbesAlive();

    void readProbesFromConfig();
    [[nodiscard]] ProbeInfo* findProbeForPacket(const ProbeMessage& packet);
    [[nodiscard]] ProbeInfo* findProbeForAddress(std::uint8_t masterAddress);

    bool handlePairingPacket(const ProbeMessage& packet, std::int32_t rssi);
    void handlePingPacket(const ProbeMessage& packet, std::int32_t rssi);
    void handleAlarmPacket(const ProbeMessage& packet, std::int32_t rssi);
    void updateBatteryLevel(ProbeInfo& probe, std::uint32_t newMillivolts);
    std::uint8_t millivoltsToPercent(std::uint32_t millivolts);

    void startAlarm(ProbeInfo& probe);
};

};
