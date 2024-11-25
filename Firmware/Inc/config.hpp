#pragma once
#include <leakguard/staticstring.hpp>

#include <cstdint>

namespace lg {

class ConfigService {
public:
    static constexpr auto CONFIG_PAGES = 64;
    static constexpr auto CURRENT_CONFIG_VERSION = 1;

    // Only memcopyable types can be used here
    struct ConfigV1 {
        std::uint32_t crc;
        std::uint32_t configVersion;
        StaticString<32> wifiSsid;
        StaticString<64> wifiPassword;
        uint32_t impulsesPerLiter;

        uint32_t unused; // <- this will config size to be word-aligned
    };

    using Config = ConfigV1;

    ConfigService() = default;

    void initialize();
    Config& getCurrentConfig() { return m_currentConfig; };
    const Config& getCurrentConfig() const { return m_currentConfig; }
    void resetToDefault();
    bool commit();

private:
    static constexpr auto FIRST_CONFIG_PAGE = 0;
    static constexpr auto SECOND_CONFIG_PAGE = CONFIG_PAGES;

    void readConfigFromEeprom();
    bool isConfigSupported(std::uint32_t version);
    void migrate();
    static std::uint32_t calculateCrc(Config& config);
    void copyCurrentToStored();

    Config m_currentConfig;
    Config m_storedConfig;
};

};
