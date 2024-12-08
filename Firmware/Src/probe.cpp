#include <probe.hpp>

#include <device.hpp>

#include <FreeRTOS.h>
#include <crc.h>
#include <task.h>

namespace lg {

static bool isPaired(const ConfigService::ProbeId& probeId)
{
    return probeId != ConfigService::INVALID_PROBE_ID;
}

static bool isProbeValid(
    const ConfigService::ProbeId& probeId, const ProbeMessage& probeMessage)
{
    return probeId.word1 == probeMessage.uid1
        && probeId.word2 == probeMessage.uid2
        && probeId.word3 == probeMessage.uid3;
}

void ProbeService::initialize()
{
    Device::get().getCronService()->registerJob([] {
        Device::get().getProbeService()->intervalHandler();
    });

    readProbesFromConfig();
}

void ProbeService::receivePacket(const ProbeMessage& packet)
{
    if (!verifyChecksum(packet)) {
        return;
    }

    if (m_pairingMode) {
        if (packet.messageType == MsgType::PING) {
            if (handlePairingPacket(packet)) {
                return;
            }
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

bool ProbeService::enterPairingMode()
{
    if (m_pairingMode) {
        return false;
    }

    m_pairingModeEnterTime = xTaskGetTickCount();
    m_pairingMode = true;
    return true;
}

bool ProbeService::leavePairingMode()
{
    if (!m_pairingMode) {
        return false;
    }

    m_pairingMode = false;
    m_pairingModeEnterTime = 0;
    return true;
}

bool ProbeService::unpairProbe(std::uint8_t masterAddress)
{
    {
        auto config = Device::get().getConfigService();
        auto& currentConfig = config->getCurrentConfig();

        if (!isPaired(currentConfig.pairedProbes.at(masterAddress))) {
            return false;
        }

        currentConfig.pairedProbes.at(masterAddress) = ConfigService::INVALID_PROBE_ID;
        config->commit();
    }

    for (std::size_t i = 0; i < m_pairedProbes.GetSize(); ++i) {
        if (m_pairedProbes[i].masterAddress == masterAddress) {
            m_pairedProbes.RemoveIndex(i);
            return true;
        }
    }

    return false;
}

auto ProbeService::getPairedProbeInfo(std::uint8_t masterAddress) const -> const ProbeInfo*
{
    for (auto& probe : m_pairedProbes) {
        if (probe.masterAddress == masterAddress) {
            return &probe;
        }
    }

    return nullptr;
}

bool ProbeService::setProbeIgnored(std::uint8_t masterAddress, bool ignored)
{
    {
        auto config = Device::get().getConfigService();
        auto& currentConfig = config->getCurrentConfig();

        if (!isPaired(currentConfig.pairedProbes.at(masterAddress))) {
            return false;
        }

        currentConfig.ignoredProbes.at(masterAddress) = ignored;
        config->commit();
    }

    auto probe = findProbeForAddress(masterAddress);
    if (probe) {
        probe->isIgnored = ignored;
        return true;
    }

    return false;
}

void ProbeService::stopAlarm()
{
    for (auto& probe : m_pairedProbes) {
        probe.isAlerted = false;
    }
}

bool ProbeService::verifyChecksum(const ProbeMessage& packet)
{
    // NOLINTBEGIN(*-const-cast)
    auto size = sizeof(ProbeMessage) - sizeof(packet.crc);
    auto crc = HAL_CRC_Calculate(&hcrc,
        const_cast<std::uint32_t*>(reinterpret_cast<const std::uint32_t*>(&packet)),
        size / sizeof(std::uint32_t));
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
    } else {
        checkAllProbesAlive();
    }
}

void ProbeService::checkAllProbesAlive()
{
    auto currentTick = xTaskGetTickCount();

    for (std::size_t i = 0; i < m_pairedProbes.GetSize(); ++i) {
        auto& probe = m_pairedProbes[i];

        if (currentTick - probe.lastPingTicks > MAX_PROBE_INACTIVITY_MS && !probe.isDead) {
            probe.isDead = true;
            startAlarm(probe);
        }
    }
}

void ProbeService::readProbesFromConfig()
{
    auto config = Device::get().getConfigService();
    auto& currentConfig = config->getCurrentConfig();
    auto currentTick = xTaskGetTickCount();

    for (std::size_t i = 0; i < MAX_PROBES; ++i) {
        auto& probeId = currentConfig.pairedProbes.at(i);
        if (isPaired(probeId)) {
            m_pairedProbes.Append(ProbeInfo {
                .id1 = probeId.word1,
                .id2 = probeId.word2,
                .id3 = probeId.word3,
                .masterAddress = static_cast<std::uint8_t>(i),
                .batteryPercent = millivoltsToPercent(0),
                .batteryMv = 0,
                .lastPingTicks = currentTick,
                .isAlerted = false,
                .isDead = false,
                .isIgnored = currentConfig.ignoredProbes.at(i),
            });
        }
    }
}

auto ProbeService::findProbeForPacket(const ProbeMessage& packet) -> ProbeInfo*
{
    for (auto& probe : m_pairedProbes) {
        if (probe.id1 == packet.uid1
            && probe.id2 == packet.uid2
            && probe.id3 == packet.uid3) {

            return &probe;
        }
    }

    return nullptr;
}

auto ProbeService::findProbeForAddress(std::uint8_t masterAddress) -> ProbeInfo*
{
    for (auto& probe : m_pairedProbes) {
        if (probe.masterAddress == masterAddress) {
            return &probe;
        }
    }

    return nullptr;
}

bool ProbeService::handlePairingPacket(const ProbeMessage& packet)
{
    auto config = Device::get().getConfigService();
    auto& currentConfig = config->getCurrentConfig();

    if (isPaired(currentConfig.pairedProbes.at(packet.dipId))) {
        return false;
    }

    auto& probeId = currentConfig.pairedProbes.at(packet.dipId);
    probeId.word1 = packet.uid1;
    probeId.word2 = packet.uid2;
    probeId.word3 = packet.uid3;

    currentConfig.ignoredProbes.at(packet.dipId) = false;

    config->commit();

    m_pairedProbes.Append(ProbeInfo {
        .id1 = packet.uid1,
        .id2 = packet.uid2,
        .id3 = packet.uid3,
        .masterAddress = packet.dipId,
        .batteryPercent = millivoltsToPercent(packet.batMvol),
        .batteryMv = packet.batMvol,
        .lastPingTicks = xTaskGetTickCount(),
        .isAlerted = false,
        .isDead = false,
        .isIgnored = false,
    });

    leavePairingMode();
    return true;
}

void ProbeService::handlePingPacket(const ProbeMessage& packet)
{
    auto config = Device::get().getConfigService();
    auto& currentConfig = config->getCurrentConfig();

    if (!isProbeValid(currentConfig.pairedProbes.at(packet.dipId), packet)) {
        return;
    }

    auto* probe = findProbeForPacket(packet);
    if (!probe) {
        return;
    }

    if (probe->masterAddress != packet.dipId) {
        return;
    }

    probe->lastPingTicks = xTaskGetTickCount();
    probe->isDead = false;
    updateBatteryLevel(*probe, packet.batMvol);
}

void ProbeService::handleAlarmPacket(const ProbeMessage& packet)
{
    auto config = Device::get().getConfigService();
    auto& currentConfig = config->getCurrentConfig();

    if (!isProbeValid(currentConfig.pairedProbes.at(packet.dipId), packet)) {
        return;
    }

    auto* probe = findProbeForPacket(packet);
    if (!probe) {
        return;
    }

    if (probe->masterAddress != packet.dipId) {
        return;
    }

    probe->lastPingTicks = xTaskGetTickCount();
    probe->isDead = false;
    updateBatteryLevel(*probe, packet.batMvol);

    startAlarm(*probe);
}

void ProbeService::updateBatteryLevel(ProbeInfo& probe, std::uint32_t newMillivolts)
{
    // We don't want the battery level to jump around.
    // So the millivolts won't go up, until we exceed this minimal increase

    static constexpr auto MIN_MV_INCREASE = 75;

    if (newMillivolts < probe.batteryMv
        || newMillivolts - MIN_MV_INCREASE > probe.batteryMv) {

        probe.batteryMv = newMillivolts;
        probe.batteryPercent = millivoltsToPercent(probe.batteryMv);
    }
}

std::uint8_t ProbeService::millivoltsToPercent(std::uint32_t millivolts)
{
    // Alkaline cell discharge curve
    // Based on: https://www.powerstream.com/z/AA-100mA.png
    static constexpr std::array<std::uint32_t, 11> DISCHARGE_CURVE = {
        1500, 1400, 1350, 1300, 1275, 1250,
        1220, 1190, 1150, 1100, 1000
    };

    if (millivolts == 0) {
        return 0xFF;
    }

    std::uint32_t cellMillivolts = millivolts / NUM_ALKALINE_CELLS;

    if (cellMillivolts >= DISCHARGE_CURVE[0]) {
        return 100;
    }

    if (cellMillivolts <= DISCHARGE_CURVE[10]) {
        return 0;
    }

    for (std::size_t i = 1; i < DISCHARGE_CURVE.size(); ++i) {
        if (cellMillivolts > DISCHARGE_CURVE.at(i)) {
            std::uint32_t delta = DISCHARGE_CURVE.at(i - 1) - DISCHARGE_CURVE.at(i);
            std::uint32_t currentPosition = cellMillivolts - DISCHARGE_CURVE.at(i);
            std::uint32_t currentPositionPercent = currentPosition * 10 / delta;
            std::uint32_t basePercent = 100 - i * 10;

            return basePercent + currentPositionPercent;
        }
    }

    // Should never reach this
    return 0;
}

void ProbeService::startAlarm(ProbeInfo& probe)
{
    probe.isAlerted = true;

    if (probe.isDead && probe.isAlerted) {
        Device::get().getValveService()->blockDueTo(ValveService::BlockReason::PROBE_DIED_BLOCK);
    }

    Device::get().getLeakLogicManager()->forceUpdate();
}

};
