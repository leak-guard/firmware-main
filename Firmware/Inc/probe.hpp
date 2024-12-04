#pragma once
#include <cstdint>

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
    ProbeService() = default;

    void initialize();
    void receivePacket(const ProbeMessage& packet);

    [[nodiscard]] bool isInPairingMode() const { return m_pairingMode; }
    void enterPairingMode();
    void leavePairingMode();

private:
    static constexpr auto MAX_PAIRING_TIME_MS = 120000; // 2 min

    volatile bool m_pairingMode {};
    std::uint32_t m_pairingModeEnterTime {};

    bool verifyChecksum(const ProbeMessage& packet);
    void intervalHandler();

    void handlePairingPacket(const ProbeMessage& packet);
    void handlePingPacket(const ProbeMessage& packet);
    void handleAlarmPacket(const ProbeMessage& packet);
};

};
