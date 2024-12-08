#pragma once
#include "config.hpp"
#include "cron.hpp"
#include "drivers/eeprom.hpp"
#include "drivers/esp-at.hpp"
#include "drivers/flash.hpp"
#include "drivers/lora.hpp"
#include "drivers/oled.hpp"
#include "flow-meter.hpp"
#include "lora.hpp"
#include "network-mgr.hpp"
#include "probe.hpp"
#include "scoped-res.hpp"
#include "server.hpp"
#include "ui.hpp"
#include "utc.hpp"
#include "valve.hpp"

#include <buzzer.hpp>
#include <drivers/buzzer.hpp>
#include <leak-logic-mgr.hpp>
#include <memory>
#include <optional>

// Forward declarations
struct uzone_t;

namespace lg {

class Device {
public:
    enum class ErrorCode {
        NO_ERROR,
        UNKNOWN_ERROR,
        WIFI_MODULE_FAILURE,
        OLED_ERROR,
        EEPROM_ERROR,
        FLASH_ERROR,
        ERROR_TYPE_COUNT
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
    bool setLocalTimezone(const char* timezoneName);
    bool setLocalTimezone(std::uint32_t timezoneId);
    UtcTime getLocalTime(
        std::uint32_t* subseconds = nullptr, std::uint32_t* secondFraction = nullptr);
    [[nodiscard]] bool isTimeValid() const { return m_timeIsValid; }
    std::uint32_t getMonotonicTimestamp() const;

    SignalStrength getSignalStrength() const { return m_signalStrength; }
    bool hasWifiStationConnection() const { return m_signalStrength > SignalStrength::STRENGTH_0; }
    void setSignalStrength(SignalStrength strength) { m_signalStrength = strength; }

    ScopedResource<EepromDriver> getEepromDriver() { return m_eepromDriver; }
    EspAtDriver& getEspAtDriver() { return m_espDriver; }
    ScopedResource<FlashDriver> getFlashDriver() { return m_flashDriver; }
    ScopedResource<OledDriver> getOledDriver() { return m_oledDriver; }
    ScopedResource<BuzzerDriver> getBuzzerDriver() { return m_buzzerDriver; }
    ScopedResource<LoraDriver> getLoraDriver() { return m_loraDriver; }

    ScopedResource<CronService> getCronService() { return m_cronService; }
    ScopedResource<ConfigService> getConfigService() { return m_configService; }
    ScopedResource<NetworkManager> getNetworkManager() { return m_networkManager; }
    ScopedResource<Server> getHttpServer() { return m_server; }
    ScopedResource<ValveService> getValveService() { return m_valveService; }
    ScopedResource<UiService> getUiService() { return m_uiService; }
    ScopedResource<FlowMeterService> getFlowMeterService() { return m_flowMeterService; }
    ScopedResource<LeakLogicManager> getLeakLogicManager() { return m_leakLogicManager; }
    ScopedResource<LoraService> getLoraService() { return m_loraService; }
    ScopedResource<ProbeService> getProbeService() { return m_probeService; }
    ScopedResource<BuzzerService> getBuzzerService() { return m_buzzerService; }

private:
    static std::optional<Device> m_instance;
    volatile ErrorCode m_error { ErrorCode::NO_ERROR };
    volatile SignalStrength m_signalStrength { SignalStrength::NO_STRENGTH };
    volatile bool m_timeIsValid {};

    mutable std::uint32_t m_monotonicTime {};
    mutable std::uint32_t m_monotonicLastTicks {};
    mutable std::uint32_t m_monotonicRemainder {};

    // We want to avoid dynamic memory at all, but we also don't want to
    // mess in code by using a not-C++-ready header <utz.h>
    // So there is a single exception in the entire codebase:

    std::unique_ptr<uzone_t> m_currentZone;

    // Drivers
    ProtectedResource<EepromDriver> m_eepromDriver;
    EspAtDriver m_espDriver; // <- this handles multithreading on its own
    ProtectedResource<FlashDriver> m_flashDriver;
    ProtectedResource<OledDriver> m_oledDriver;
    ProtectedResource<BuzzerDriver> m_buzzerDriver;
    ProtectedResource<LoraDriver> m_loraDriver;

    // Services
    ProtectedResource<CronService> m_cronService;
    ProtectedResource<ConfigService> m_configService;
    ProtectedResource<NetworkManager> m_networkManager;
    ProtectedResource<Server> m_server;
    ProtectedResource<ValveService> m_valveService;
    ProtectedResource<UiService> m_uiService;
    ProtectedResource<FlowMeterService> m_flowMeterService;
    ProtectedResource<LeakLogicManager> m_leakLogicManager;
    ProtectedResource<ProbeService> m_probeService;
    ProtectedResource<BuzzerService> m_buzzerService;
    ProtectedResource<LoraService> m_loraService;
};

};
