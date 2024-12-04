#pragma once
#include <cstdint>

#include <leakguard/staticvector.hpp>

namespace lg {

enum class MsgType : std::uint8_t {
    PING = 0,
    BATTERY = 1,
    ALARM = 2
};

struct ProbeMessage {
    MsgType messageType;
    uint32_t uid1;
    uint32_t uid2;
    uint32_t uid3;
    uint8_t dipId;
    uint16_t batMvol;
    uint32_t crc;
};

class ProbeService {
public:
    static constexpr auto MAX_PROBES = 256;

    struct ProbeInfo {
        std::uint32_t id1 {};
        std::uint32_t id2 {};
        std::uint32_t id3 {};
        std::uint8_t masterAddress {};
        std::uint8_t batteryPercent {};
        std::uint16_t batteryMv {};
        std::uint32_t lastPingTicks {};
        bool isAlerted {};
        bool isDead {};
    };

    ProbeService() = default;

    void initialize();
    void receivePacket(const ProbeMessage& packet);

    [[nodiscard]] bool isInPairingMode() const { return m_pairingMode; }
    void enterPairingMode();
    void leavePairingMode();

    void unpairProbe(std::uint8_t masterAddress);
    [[nodiscard]] const StaticVector<ProbeInfo, MAX_PROBES>&
    getPairedProbesInfo() const { return m_pairedProbes; }

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
    ProbeInfo* findProbeForPacket(const ProbeMessage& packet);

    bool handlePairingPacket(const ProbeMessage& packet);
    void handlePingPacket(const ProbeMessage& packet);
    void handleAlarmPacket(const ProbeMessage& packet);
    void updateBatteryLevel(ProbeInfo& probe, std::uint32_t newMillivolts);
    std::uint8_t millivoltsToPercent(std::uint32_t millivolts);

    void startAlarm(ProbeInfo& probe);
};

};
