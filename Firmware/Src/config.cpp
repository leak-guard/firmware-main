#include <config.hpp>

#include <cstdint>
#include <cstring>
#include <device.hpp>

#include <crc.h>

namespace lg {

static_assert(
    sizeof(ConfigService::Config) < ConfigService::CONFIG_PAGES * EepromDriver::EEPROM_PAGE_SIZE_BYTES,
    "Oops, config is too large");

static_assert(
    sizeof(ConfigService::Config) % sizeof(std::uint32_t) == 0, "Config size must be word-aligned");

void ConfigService::initialize()
{
    if (readConfigFromEeprom()) {
        copyCurrentToStored();
    } else {
        auto storedPtr = reinterpret_cast<std::uint32_t*>(&m_storedConfig);
        for (int i = 0; i < sizeof(m_storedConfig) / sizeof(std::uint32_t); ++i) {
            *(storedPtr++) = 0xFFFFFFFFU;
        }
    }
}

void ConfigService::resetToDefault()
{
    m_currentConfig.configVersion = CURRENT_CONFIG_VERSION;
    m_currentConfig.wifiSsid.Clear();
    m_currentConfig.wifiPassword.Clear();
    m_currentConfig.impulsesPerLiter = 500;
    m_currentConfig.valveTypeNC = false;
    m_currentConfig.adminPassword = "admin1";
    m_currentConfig.weeklySchedule.fill(0);
    m_currentConfig.timezoneId = 40; // London timezone (GMT/UTC+0)

    m_currentConfig.unused = 0;

    m_currentConfig.crc = calculateCrc(m_currentConfig);
}

static inline bool compareWords(const uint32_t* first, const uint32_t* second, size_t nWords)
{
    while (nWords) {
        if (*first != *second)
            return false;
        ++first;
        ++second;
        --nWords;
    }

    return true;
}

bool ConfigService::commit()
{
    static constexpr auto PAGE_WORDS
        = EepromDriver::EEPROM_PAGE_SIZE_BYTES / sizeof(std::uint32_t);

    m_currentConfig.crc = calculateCrc(m_currentConfig);

    std::size_t remainingBytes = sizeof(Config);
    std::uint16_t currentPage = FIRST_CONFIG_PAGE;
    auto currentPtr = reinterpret_cast<const uint32_t*>(&m_currentConfig);
    auto storedPtr = reinterpret_cast<const uint32_t*>(&m_storedConfig);
    std::array<bool, CONFIG_PAGES> writeFlags {};

    auto eeprom = Device::get().getEepromDriver();
    eeprom->enableWrites();

    // Overwrite first copy of config
    while (remainingBytes > 0) {
        std::size_t pageBytes = remainingBytes;
        if (pageBytes > EepromDriver::EEPROM_PAGE_SIZE_BYTES) {
            pageBytes = EepromDriver::EEPROM_PAGE_SIZE_BYTES;
        }

        bool same = compareWords(currentPtr, storedPtr, pageBytes / sizeof(std::uint32_t));
        if (!same) {
            writeFlags.at(currentPage) = true;
            if (!eeprom->writePage(currentPage,
                    reinterpret_cast<const uint8_t*>(currentPtr), pageBytes)) {

                eeprom->disableWrites();
                return false;
            }
        }

        currentPtr += PAGE_WORDS;
        storedPtr += PAGE_WORDS;
        remainingBytes -= pageBytes;
        ++currentPage;
    }

    // Overwrite second copy of config
    remainingBytes = sizeof(Config);
    currentPage = SECOND_CONFIG_PAGE;
    currentPtr = reinterpret_cast<const uint32_t*>(&m_currentConfig);

    while (remainingBytes > 0) {
        std::size_t pageBytes = remainingBytes;
        if (pageBytes > EepromDriver::EEPROM_PAGE_SIZE_BYTES) {
            pageBytes = EepromDriver::EEPROM_PAGE_SIZE_BYTES;
        }

        if (writeFlags.at(currentPage - SECOND_CONFIG_PAGE)) {
            if (!eeprom->writePage(currentPage,
                    reinterpret_cast<const uint8_t*>(currentPtr), pageBytes)) {

                eeprom->disableWrites();
                return false;
            }
        }

        currentPtr += PAGE_WORDS;
        remainingBytes -= pageBytes;
        ++currentPage;
    }

    eeprom->disableWrites();
    copyCurrentToStored();

    return true;
}

bool ConfigService::readConfigFromEeprom()
{
    auto eeprom = Device::get().getEepromDriver();

    if (!eeprom->readObject(
            FIRST_CONFIG_PAGE * EepromDriver::EEPROM_PAGE_SIZE_BYTES, m_currentConfig)) {

        resetToDefault();
        return false;
    }

    auto correctCrc = calculateCrc(m_currentConfig);
    if (m_currentConfig.crc != correctCrc) {
        if (!eeprom->readObject(
                SECOND_CONFIG_PAGE * EepromDriver::EEPROM_PAGE_SIZE_BYTES, m_currentConfig)) {

            resetToDefault();
            return false;
        }

        correctCrc = calculateCrc(m_currentConfig);
        if (m_currentConfig.crc != correctCrc) {
            resetToDefault();
            return false;
        }
    }

    if (!isConfigSupported(m_currentConfig.configVersion)) {
        resetToDefault();
        return false;
    }

    if (m_currentConfig.configVersion != CURRENT_CONFIG_VERSION) {
        migrate();
        return false;
    }

    return true;
}

bool ConfigService::isConfigSupported(std::uint32_t version)
{
    return version > 0 && version <= CURRENT_CONFIG_VERSION;
}

void ConfigService::migrate()
{
    // Implement version migration in the future
}

std::uint32_t ConfigService::calculateCrc(Config& config)
{
    return HAL_CRC_Calculate(
        &hcrc,
        reinterpret_cast<uint32_t*>(&config) + 1,
        sizeof(Config) / sizeof(std::uint32_t) - 1);
}

void ConfigService::copyCurrentToStored()
{
    memcpy(&m_storedConfig, &m_currentConfig, sizeof(Config));
}

};
