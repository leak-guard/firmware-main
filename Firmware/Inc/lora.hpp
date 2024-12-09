#pragma once
#include <cstdint>

#include <stm32f7xx_hal.h>

namespace lg {

class LoraService {
public:
    LoraService() = default;

    static void loraServiceEntryPoint(void* params);

    void initialize();

private:
    void handlePacket(const std::uint8_t* buffer, std::size_t length, std::int32_t rssi);
};

}