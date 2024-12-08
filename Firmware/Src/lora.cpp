#include <lora.hpp>

#include <device.hpp>

namespace lg {

void LoraService::initialize()
{
    auto loraDriver = Device::get().getLoraDriver();
    loraDriver->setModeRx(sizeof(ProbeMessage), 2000);

    loraDriver->onPacket = [this](const std::uint8_t* buffer, std::size_t length) {
        handlePacket(buffer, length);
    };
}

void LoraService::handlePacket(const std::uint8_t* buffer, std::size_t length)
{
    if (length == sizeof(ProbeMessage)) {
        auto packet = reinterpret_cast<const ProbeMessage*>(buffer);
        Device::get().getProbeService()->receivePacket(*packet);
    }
}

}