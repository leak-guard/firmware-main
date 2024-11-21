#pragma once
#include "config.hpp"
#include "drivers/eeprom.hpp"
#include "drivers/esp-at.hpp"
#include "drivers/oled.hpp"
#include "network-mgr.hpp"
#include "scoped-res.hpp"
#include "server.hpp"
#include "ui.hpp"
#include "utc.hpp"

#include <optional>

namespace lg {

class Device {
public:
    enum class ErrorCode {
        NO_ERROR,
        UNKNOWN_ERROR,
        WIFI_MODULE_FAILURE,
        OLED_ERROR,
        EEPROM_ERROR,
    };

    enum class SignalStrength {
        NO_STRENGTH,
        HOTSPOT,
        CONNECTING,
        STRENGTH_0,
        STRENGTH_1,
        STRENGTH_2,
        STRENGTH_3,
        STRENGTH_4
    };

    static Device& get();

    Device();

    void initializeDrivers();
    bool hasError() const { return m_error != ErrorCode::NO_ERROR; }
    ErrorCode getError() const { return m_error; }
    void setError(ErrorCode code);

    void updateRtcTime(const UtcTime& newTime);

    SignalStrength getSignalStrength() const { return m_signalStrength; }
    bool hasWifiStationConnection() const { return m_signalStrength > SignalStrength::STRENGTH_0; }
    void setSignalStrength(SignalStrength strength) { m_signalStrength = strength; }

    ScopedResource<EepromDriver> getEepromDriver() { return m_eepromDriver; }
    EspAtDriver& getEspAtDriver() { return m_espDriver; }
    ScopedResource<OledDriver> getOledDriver() { return m_oledDriver; }

    ScopedResource<ConfigService> getConfigService() { return m_configService; }
    ScopedResource<NetworkManager> getNetworkManager() { return m_networkManager; }
    ScopedResource<Server> getHttpServer() { return m_server; }
    ScopedResource<UiService> getUiService() { return m_uiService; }

    BaseType_t notifyEepromFromIsr(bool tx);

private:
    static std::optional<Device> m_instance;
    volatile ErrorCode m_error { ErrorCode::NO_ERROR };
    volatile SignalStrength m_signalStrength { SignalStrength::NO_STRENGTH };

    // Drivers
    ProtectedResource<EepromDriver> m_eepromDriver;
    EspAtDriver m_espDriver; // <- this handles multithreading on its own
    ProtectedResource<OledDriver> m_oledDriver;

    // Services
    ProtectedResource<ConfigService> m_configService;
    ProtectedResource<NetworkManager> m_networkManager;
    ProtectedResource<Server> m_server;
    ProtectedResource<UiService> m_uiService;
};

};
