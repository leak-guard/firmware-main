#pragma once
#include <array>
#include <cstdint>

#include <stm32f7xx_hal.h>

namespace lg {

class FlashDriver {
public:
    // W25Q64JVSS Flash

    static constexpr auto FLASH_SIZE = (1 << 22); // 4 MB
    static constexpr auto FLASH_PAGE_SIZE = 256; // 256 B
    static constexpr auto FLASH_PAGE_COUNT = FLASH_SIZE / FLASH_PAGE_SIZE;
    static constexpr auto FLASH_SECTOR_SIZE = 4096; // 4 KB
    static constexpr auto FLASH_SECTOR_COUNT = FLASH_SIZE / FLASH_SECTOR_SIZE;
    static constexpr auto FLASH_PAGES_PER_SECTOR = FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE;

    explicit FlashDriver(QSPI_HandleTypeDef* qspi)
        : m_qspi(qspi)
    {
    }

    void initialize();
    void* getBasePtr() const;
    void* getPageAddress(std::uint32_t page) const;

    bool writePage(std::uint32_t pageId, const std::uint8_t* data, std::size_t size);

    template <typename T>
    bool writeObject(std::uint32_t pageId, const T& object)
    {
        static_assert(sizeof(T) <= FLASH_PAGE_SIZE,
            "Object is too big to fit in one page");

        return writePage(pageId,
            reinterpret_cast<const std::uint8_t*>(&object), sizeof(T));
    }

    bool eraseSector(std::uint32_t sectorId);

private:
    QSPI_HandleTypeDef* m_qspi;

    void delay(std::uint32_t millis);
    bool testFlash();

    bool resetChip();
    bool eraseChip();
    bool enableMemoryMappedMode();
    bool disableMemoryMappedMode();

    bool writeEnable();
    bool autoPollingMemReady();
    bool configureQspi();
    bool doWritePage(std::uint32_t pageId,
        const std::uint8_t* data, std::size_t count);
    bool doEraseSector(std::uint32_t sectorId);
};

};
