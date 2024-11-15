#pragma once
#include <FreeRTOS.h>
#include <semphr.h>

namespace lg {

template <typename T>
class ScopedResource {
public:
    ScopedResource(T& ref, SemaphoreHandle_t mutex)
        : m_res(ref)
        , m_mutex(mutex)
    {
        xSemaphoreTake(m_mutex, portMAX_DELAY);
    }

    ~ScopedResource()
    {
        xSemaphoreGive(m_mutex);
    }

    ScopedResource(const ScopedResource&) = delete;
    ScopedResource(ScopedResource&&) = delete;
    ScopedResource& operator=(const ScopedResource&) = delete;
    ScopedResource& operator=(ScopedResource&&) = delete;

    T* operator->() const
    {
        return &m_res;
    }

private:
    T& m_res;
    SemaphoreHandle_t m_mutex;
};

};
