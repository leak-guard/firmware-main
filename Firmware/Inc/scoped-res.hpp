#pragma once
#include <FreeRTOS.h>
#include <semphr.h>

#include <stm32f7xx_hal.h>
#include <utility>

namespace lg {

// Forward declarations

template <typename T>
class ScopedResource;

template <typename T>
class ProtectedResource {
    friend class ScopedResource<T>;

public:
    template <class... Args>
    ProtectedResource(Args&&... args)
        : m_res(std::forward<Args>(args)...)
    {
        createMutex();
    }

    T* operator->()
    {
        return &m_res;
    }

    const T* operator->() const
    {
        return &m_res;
    }

private:
    void createMutex()
    {
        m_mutex = xSemaphoreCreateMutexStatic(&m_mutexBlock);
    }

    T m_res;
    SemaphoreHandle_t m_mutex {};
    StaticSemaphore_t m_mutexBlock {};
};

template <typename T>
class ScopedResource {
public:
    ScopedResource(ProtectedResource<T>& resource)
        : ScopedResource(resource.m_res, resource.m_mutex)
    {
    }

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
    SemaphoreHandle_t m_mutex {};
};

};
