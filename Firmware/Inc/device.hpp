#pragma once
#include "drivers/esp-at.hpp"
#include "drivers/oled.hpp"
#include "scoped-res.hpp"
#include "server.hpp"

#include <optional>

namespace lg {

class Device {
public:
    enum class ErrorCode {
        NO_ERROR,
        UNKNOWN_ERROR,
        WIFI_MODULE_FAILURE
    };

    static Device& get();

    Device();

    void initializeDrivers();
    bool hasError() const { return m_error != ErrorCode::NO_ERROR; }
    ErrorCode getError() const { return m_error; }
    void setError(ErrorCode code);

    EspAtDriver& getEspAtDriver() { return m_espDriver; }
    ScopedResource<OledDriver> getOledDriver() { return m_oledDriver; }
    
    ScopedResource<Server> getHttpServer() { return m_server; }

private:
    static std::optional<Device> m_instance;
    volatile ErrorCode m_error { ErrorCode::NO_ERROR };

    // Drivers
    EspAtDriver m_espDriver;
    ProtectedResource<OledDriver> m_oledDriver;

    // Services
    ProtectedResource<Server> m_server;
};

};
