#pragma once

#include <stm32f7xx_hal.h>

namespace lg {

class LoraService {
public:
    LoraService() = default;

    static void loraServiceEntryPoint(void* params);

    void initialize();

private:
    void loraServiceMain();
};

}