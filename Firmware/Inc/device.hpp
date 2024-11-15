#pragma once
#include "drivers/esp-at.hpp"
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
    Server& getHttpServer() { return m_server; }

private:
    static std::optional<Device> m_instance;
    volatile ErrorCode m_error { ErrorCode::NO_ERROR };

    EspAtDriver m_espDriver;
    Server m_server;
};

};
