#include <probe.hpp>

#include <device.hpp>

#include <FreeRTOS.h>
#include <crc.h>
#include <task.h>

namespace lg {

void ProbeService::initialize()
{
    Device::get().getCronService()->registerJob([this] { intervalHandler(); });
}

void ProbeService::receivePacket(const ProbeMessage& packet)
{
    if (!verifyChecksum(packet)) {
        return;
    }

    if (m_pairingMode) {
        if (packet.messageType == MsgType::PING) {
            return handlePairingPacket(packet);
        }
    }

    switch (packet.messageType) {
    case MsgType::PING:
        return handlePingPacket(packet);
    case MsgType::BATTERY:
        return handlePingPacket(packet);
    case MsgType::ALARM:
        return handleAlarmPacket(packet);
    }
}

void ProbeService::enterPairingMode()
{
    m_pairingMode = true;
}

void ProbeService::leavePairingMode()
{
    m_pairingMode = false;
}

bool ProbeService::verifyChecksum(const ProbeMessage& packet)
{
    // NOLINTBEGIN(*-const-cast)
    auto size = sizeof(ProbeMessage) - sizeof(packet.crc);
    auto crc = HAL_CRC_Calculate(&hcrc,
        const_cast<std::uint32_t*>(reinterpret_cast<const std::uint32_t*>(&packet)), size / sizeof(std::uint32_t));
    // NOLINTEND(*-const-cast)

    return crc == packet.crc;
}

void ProbeService::intervalHandler()
{
    // This runs every minute

    if (m_pairingMode) {
        auto currentTick = xTaskGetTickCount();

        if (currentTick >= m_pairingModeEnterTime + MAX_PAIRING_TIME_MS) {
            leavePairingMode();
        }
    }
}

void ProbeService::handlePairingPacket(const ProbeMessage& packet)
{
}

void ProbeService::handlePingPacket(const ProbeMessage& packet)
{
}

void ProbeService::handleAlarmPacket(const ProbeMessage& packet)
{
}

};
