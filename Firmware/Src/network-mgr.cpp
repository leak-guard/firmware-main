#include <drivers/esp-at.hpp>
#include <network-mgr.hpp>

#include <device.hpp>

#include <gpio.h>
#include <stm32f7xx_hal.h>

namespace lg {

void NetworkManager::networkManagerEntryPoint(void* params)
{
    auto instance = reinterpret_cast<NetworkManager*>(params);
    instance->networkManagerMain();
}

void NetworkManager::initialize()
{
    m_networkManagerTaskHandle = xTaskCreateStatic(
        &NetworkManager::networkManagerEntryPoint /* Task function */,
        "Network Mgr" /* Task name */,
        m_networkManagerTaskStack.size() /* Stack size */,
        this /* parameters */,
        3 /* Prority */,
        m_networkManagerTaskStack.data() /* Task stack address */,
        &m_networkManagerTaskTcb /* Task control block */
    );

    generateAccessPointCredentials();

    if (!shouldForceApMode()) {
        m_credentialsReload = 1;
    }
}

void NetworkManager::reloadCredentials()
{
    // The task will periodically check, if this value has changed.
    // If it did, it will load new credentials from settings

    auto previous = m_credentialsReload;
    m_credentialsReload = previous + 1;
}

void NetworkManager::reloadCredentialsOneShot()
{
    m_oneShotMode = true;
    reloadCredentials();
}

void NetworkManager::generateAccessPointCredentials()
{
    m_apSsid = "LeakGuardConfig";

    std::uint32_t password = HAL_GetUIDw0();
    password ^= HAL_GetUIDw1();
    password ^= HAL_GetUIDw2();

    m_apPassword = "LG";
    for (int i = 0; i < 8; ++i) {
        std::uint32_t character = password & 0xF;
        if (character < 10) {
            m_apPassword += static_cast<char>('0' + character);
        } else {
            m_apPassword += static_cast<char>('a' + (character - 10));
        }

        password >>= 4;
    }
}

void NetworkManager::networkManagerMain()
{
    static constexpr auto RSSI_CHECK_INTERVAL = 10000; // 10s
    static constexpr auto TASK_INTERVAL_MS = 1000; // 1s

    std::uint32_t prevCredentialsReload = 0;
    TickType_t prevRssiTicks = 0;
    auto& esp = Device::get().getEspAtDriver();

    while (true) {
        bool reconnect = false;

        vTaskDelay(TASK_INTERVAL_MS);

        if (prevCredentialsReload != m_credentialsReload) {
            reconnect = loadCredentialsFromSettings();
            ++prevCredentialsReload;
        }

        if (m_mdnsHostname.IsEmpty()) {
            generateMdnsHostname();
        }

        WifiMode targetMode = WifiMode::AP;
        if (!m_wifiSsid.IsEmpty()) {
            targetMode = WifiMode::STATION;
        }

        if (targetMode != m_currentMode) {
            if (esp.setWifiMode(targetMode) != EspAtDriver::EspResponse::OK) {
                Device::get().setError(Device::ErrorCode::WIFI_MODULE_FAILURE);
            }
            m_currentMode = targetMode;

            reconnect = true;
        }

        if (reconnect || m_retryConnect) {
            m_retryConnect = false;

            if (m_currentMode == WifiMode::AP) {
                Device::get().setSignalStrength(Device::SignalStrength::HOTSPOT);

                esp.disableMdns();
                m_mdnsEnabled = false;

                if (esp.setupSoftAp(m_apSsid.ToCStr(), m_apPassword.ToCStr(),
                        6, EspAtDriver::Encryption::WPA2_PSK)
                    != EspAtDriver::EspResponse::OK) {

                    Device::get().setSignalStrength(Device::SignalStrength::NO_STRENGTH);
                    Device::get().setError(Device::ErrorCode::WIFI_MODULE_FAILURE);
                }
            } else if (m_currentMode == WifiMode::STATION) {
                Device::get().setSignalStrength(Device::SignalStrength::CONNECTING);

                esp.setHostname(m_mdnsHostname.ToCStr());

                if (esp.joinAccessPoint(
                        m_wifiSsid.ToCStr(), m_wifiPassword.ToCStr())
                    == EspAtDriver::EspResponse::OK) {

                    prevRssiTicks = xTaskGetTickCount() + 1000;
                    m_oneShotMode = false;
                    Device::get().setSignalStrength(Device::SignalStrength::STRENGTH_0);

                    if (enableMdns() == EspAtDriver::EspResponse::OK) {
                        m_mdnsEnabled = true;
                    } else {
                        m_mdnsEnabled = false;
                        m_mdnsRetryLeft = 10;
                    }
                } else if (m_oneShotMode) {
                    m_oneShotMode = false;
                    m_wifiSsid.Clear();
                    m_wifiPassword.Clear();
                } else {
                    m_retryConnect = true;
                }
            }
        }

        if (m_currentMode == WifiMode::STATION) {
            if (!m_mdnsEnabled && m_mdnsRetryLeft > 0) {
                if (enableMdns() == EspAtDriver::EspResponse::OK) {
                    m_mdnsEnabled = true;
                    m_mdnsRetryLeft = 0;
                } else {
                    --m_mdnsRetryLeft;
                }
            }

            switch (esp.getWifiStatus()) {
            case EspAtDriver::EspWifiStatus::DISCONNECTED:
            case EspAtDriver::EspWifiStatus::CONNECTING:
            case EspAtDriver::EspWifiStatus::CONNECTED:
                Device::get().setSignalStrength(Device::SignalStrength::CONNECTING);
                break;
            case EspAtDriver::EspWifiStatus::DHCP_GOT_IP:
                if (Device::get().getSignalStrength() == Device::SignalStrength::CONNECTING) {
                    Device::get().setSignalStrength(Device::SignalStrength::STRENGTH_0);
                }
                break;
            }
        }

        if (esp.getWifiStatus() == EspAtDriver::EspWifiStatus::DHCP_GOT_IP
            && xTaskGetTickCount() - prevRssiTicks > RSSI_CHECK_INTERVAL) {
            updateRssi();
            prevRssiTicks = xTaskGetTickCount();
        }
    }
}

bool NetworkManager::loadCredentialsFromSettings()
{
    auto config = Device::get().getConfigService();
    auto& currentConfig = config->getCurrentConfig();

    bool differentSsid = currentConfig.wifiSsid != m_wifiSsid;
    bool differentPassword = currentConfig.wifiPassword != m_wifiPassword;

    if (differentSsid || differentPassword) {
        m_wifiSsid = currentConfig.wifiSsid;
        m_wifiPassword = currentConfig.wifiPassword;
    }

    return differentSsid || differentPassword;
}

void NetworkManager::updateRssi()
{
    auto& esp = Device::get().getEspAtDriver();
    int rssi = 0;
    auto signalStrength = Device::SignalStrength::STRENGTH_0;

    if (esp.getRssi(rssi) == EspAtDriver::EspResponse::OK) {
        if (rssi > -60) {
            signalStrength = Device::SignalStrength::STRENGTH_4;
        } else if (rssi > -70) {
            signalStrength = Device::SignalStrength::STRENGTH_3;
        } else if (rssi > -80) {
            signalStrength = Device::SignalStrength::STRENGTH_2;
        } else if (rssi > -100) {
            signalStrength = Device::SignalStrength::STRENGTH_1;
        }

        Device::get().setSignalStrength(signalStrength);
    }
}

void NetworkManager::generateMdnsHostname()
{
    auto& esp = Device::get().getEspAtDriver();
    StaticString<ESP_MAC_STRING_SIZE> mac;

    if (esp.getStationMacAddress(mac) == EspAtDriver::EspResponse::OK) {
        for (auto& c : mac) {
            if (c == ':') {
                c = '-';
            }
        }

        m_mdnsHostname = "leakguard-";
        m_mdnsHostname += mac;

        m_macAddress = mac;
    } else {
        Device::get().setError(Device::ErrorCode::WIFI_MODULE_FAILURE);
    }
}

EspAtDriver::EspResponse NetworkManager::enableMdns()
{
    auto& esp = Device::get().getEspAtDriver();
    return esp.enableMdns(m_mdnsHostname.ToCStr(), "_leakguard", 80);
}

bool NetworkManager::shouldForceApMode()
{
    // FIXME: Maybe we should eextract it to another driver?
    auto pinState = HAL_GPIO_ReadPin(
        BTN_UNLOCK_GPIO_Port, BTN_UNLOCK_Pin);
    return pinState == GPIO_PIN_RESET;
}

};
