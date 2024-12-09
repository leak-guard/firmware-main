#include <lora.hpp>

#include <device.hpp>

namespace lg {

void LoraService::initialize()
{
    auto loraDriver = Device::get().getLoraDriver();
    if (!loraDriver->setModeRx(sizeof(ProbeMessage), 2000)) {
        Device::get().setError(Device::ErrorCode::LORA_ERROR);
    }

    loraDriver->onPacket = [this](const std::uint8_t* buffer, std::size_t length, std::int32_t rssi) {
        handlePacket(buffer, length, rssi);
    };
}

void LoraService::handlePacket(const std::uint8_t* buffer, std::size_t length, std::int32_t rssi)
{
    if (length == sizeof(ProbeMessage)) {
        auto packet = reinterpret_cast<const ProbeMessage*>(buffer);
        Device::get().getProbeService()->receivePacket(*packet, rssi);
    }
}

}