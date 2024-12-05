#pragma once

#include <stm32f7xx_hal.h>
#include "SX1278.h"

namespace lg {

class LoraService {
public:
    explicit LoraService(void* spi, GPIO_TypeDef* dio0Port, int dio0Pin, GPIO_TypeDef* nssPort, int nssPin, GPIO_TypeDef* resetPort, int resetPin) : m_spi(spi), m_dio0Port(dio0Port), m_dio0Pin(dio0Pin), m_nssPort(nssPort), m_nssPin(nssPin), m_resetPort(resetPort), m_resetPin(resetPin) {}

    static void loraServiceEntryPoint(void* params);

    void initialize();

private:
    void loraServiceMain();

    void* m_spi;
    SX1278_hw_t SX1278_hw{};
    SX1278_t SX1278{};
    GPIO_TypeDef* m_dio0Port;
    int m_dio0Pin;
    GPIO_TypeDef* m_nssPort;
    int m_nssPin;
    GPIO_TypeDef* m_resetPort;
    int m_resetPin;

    int ret;

    typedef struct __attribute__((packed)) {
        uint8_t MsgType;
        uint32_t uid1;
        uint32_t uid2;
        uint32_t uid3;
        uint8_t dipId;
        uint16_t batMvol;
        uint32_t crc;
    } Message;

    // Message type enum
    enum MsgType {
        PING = 0,
        BATTERY = 1,
        ALARM = 2
    };
};

}