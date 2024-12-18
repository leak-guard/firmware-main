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
    void reloadCredentialsOneShot();

    void mqttPublishLeak(const char* data);

    [[nodiscard]] const StaticString<32> getAccessPointSsid() const { return m_apSsid; }
    [[nodiscard]] const StaticString<64> getAccessPointPassword() const { return m_apPassword; }

    [[nodiscard]] const StaticString<20> getMacAddress() const { return m_macAddress; }
    [[nodiscard]] const StaticString<16> getIpAddress() const { return m_ipAddress; }

private:
    using WifiMode = EspAtDriver::EspWifiMode;

    void generateAccessPointCredentials();
    void networkManagerMain();
    bool loadCredentialsFromSettings();
    void updateRssi();
    void generateMdnsHostname();
    void configureSntp();
    void updateIpAddress();
    EspAtDriver::EspResponse enableMdns();
    bool shouldForceApMode();
    void requestUpdateDeviceTime();
    void updateDeviceTime();
    void publishMqtt();

    StaticString<32> m_apSsid;
    StaticString<64> m_apPassword;

    TaskHandle_t m_networkManagerTaskHandle {};
    StaticTask_t m_networkManagerTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 512> m_networkManagerTaskStack {};

    volatile std::uint32_t m_credentialsReload {};
    volatile bool m_oneShotMode { false };
    volatile bool m_shouldUpdateTime { false };
    bool m_retryConnect { false };
    bool m_sntpConfigured { false };

    WifiMode m_currentMode { WifiMode::OFF };
    StaticString<32> m_wifiSsid;
    StaticString<64> m_wifiPassword;
    StaticString<32> m_mdnsHostname;
    StaticString<20> m_macAddress;
    StaticString<16> m_ipAddress;

    StaticString<128> m_mqttBuffer;
    int m_mqttRetriesLeft { 0 };
    bool m_mqttConnected { false };

    bool m_mdnsEnabled { false };
    uint32_t m_mdnsRetryLeft { 0 };
};

};
