#pragma once
#include "drivers/esp-at.hpp"
#include "drivers/oled.hpp"
#include "network-mgr.hpp"
#include "scoped-res.hpp"
#include "server.hpp"
#include "ui.hpp"

#include <optional>

namespace lg {

class Device {
public:
    enum class ErrorCode {
        NO_ERROR,
        UNKNOWN_ERROR,
        WIFI_MODULE_FAILURE
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

    SignalStrength getSignalStrength() const { return m_signalStrength; }
    void setSignalStrength(SignalStrength strength) { m_signalStrength = strength; }

    EspAtDriver& getEspAtDriver() { return m_espDriver; }
    ScopedResource<OledDriver> getOledDriver() { return m_oledDriver; }

    ScopedResource<NetworkManager> getNetworkManager() { return m_networkManager; }
    ScopedResource<Server> getHttpServer() { return m_server; }
    ScopedResource<UiService> getUiService() { return m_uiService; }

private:
    static std::optional<Device> m_instance;
    volatile ErrorCode m_error { ErrorCode::NO_ERROR };
    volatile SignalStrength m_signalStrength { SignalStrength::NO_STRENGTH };

    // Drivers
    EspAtDriver m_espDriver;
    ProtectedResource<OledDriver> m_oledDriver;

    // Services
    ProtectedResource<NetworkManager> m_networkManager;
    ProtectedResource<Server> m_server;
    ProtectedResource<UiService> m_uiService;
};

};
