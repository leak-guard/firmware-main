#include <device.hpp>
#include <optional>

#include <usart.h>

namespace lg {

std::optional<Device> Device::m_instance;

Device::Device()
    : m_espDriver(&huart1)
{
}

Device& Device::get()
{
    if (!m_instance.has_value()) {
        m_instance.emplace();
    }

    return m_instance.value();
}

void Device::initializeDrivers()
{
    m_espDriver.initialize();
    m_server.initialize();
}

void Device::setError(ErrorCode code)
{
    m_error = code;
}

};
