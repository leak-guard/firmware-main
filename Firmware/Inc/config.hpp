#pragma once
#include <leakguard/leak_logic.hpp>
#include <leakguard/staticstring.hpp>

#include <array>
#include <cstdint>

namespace lg {

class ConfigService {
public:
    static constexpr auto CONFIG_PAGES = 64;
    static constexpr auto CURRENT_CONFIG_VERSION = 1;
    static constexpr auto BLOCKADE_ENABLED_FLAG = 1U << 31;
    static constexpr auto MAX_PROBES = 256;

    struct ProbeId {
        std::uint32_t word1, word2, word3;

        auto operator<=>(const ProbeId& other) const = default;
    };

    static constexpr ProbeId INVALID_PROBE_ID = { 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU };

    // Only memcopyable types can be used here
    struct ConfigV1 {
        friend class ConfigService;

        std::uint32_t crc {};
        std::uint32_t configVersion {};
        StaticString<32> wifiSsid;
        StaticString<64> wifiPassword;
        std::uint32_t impulsesPerLiter {};
        bool valveTypeNC {};
        StaticString<32> adminPassword;
        std::array<std::uint32_t, 7> weeklySchedule {};
        std::uint32_t timezoneId {};
        StaticString<64> leakLogicConfig;
        std::array<ProbeId, MAX_PROBES> pairedProbes {};
        std::array<bool, MAX_PROBES> ignoredProbes {};

        uint32_t unused {}; // <- this will force config size to be word-aligned

        ~ConfigV1() = default;

    private:
        ConfigV1() = default;
        ConfigV1(const ConfigV1&) = default;
        ConfigV1(ConfigV1&&) = default;
        ConfigV1& operator=(const ConfigV1&) = default;
        ConfigV1& operator=(ConfigV1&&) = default;
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

    bool readConfigFromEeprom();
    bool isConfigSupported(std::uint32_t version);
    void migrate();
    static std::uint32_t calculateCrc(Config& config);
    void copyCurrentToStored();

    Config m_currentConfig;
    Config m_storedConfig;
    bool m_needFullWrite {};
};

};
