#pragma once
#include "drivers/esp-at.hpp"

#include <leakguard/staticstring.hpp>

#include <array>
#include <cstdint>

#include <FreeRTOS.h>
#include <task.h>

namespace lg {

class NetworkManager {
public:
    static void networkManagerEntryPoint(void* params);

    NetworkManager() = default;

    void initialize();
    void reloadCredentials();

    [[nodiscard]] const StaticString<32> getAccessPointSsid() const { return m_apSsid; }
    [[nodiscard]] const StaticString<64> getAccessPointPassword() const { return m_apPassword; }

private:
    using WifiMode = EspAtDriver::EspWifiMode;

    void generateAccessPointCredentials();
    void networkManagerMain();
    bool loadCredentialsFromSettings();
    void updateRssi();
    void generateMdnsHostname();
    EspAtDriver::EspResponse enableMdns();

    StaticString<32> m_apSsid;
    StaticString<64> m_apPassword;

    TaskHandle_t m_networkManagerTaskHandle {};
    StaticTask_t m_networkManagerTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 512> m_networkManagerTaskStack {};

    volatile std::uint32_t m_credentialsReload {};

    WifiMode m_currentMode { WifiMode::OFF };
    StaticString<32> m_wifiSsid;
    StaticString<64> m_wifiPassword;
    StaticString<32> m_mdnsHostname;

    bool m_mdnsEnabled { false };
    uint32_t m_mdnsRetryLeft { 0 };
};

};
