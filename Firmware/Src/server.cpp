#include "leakguard/staticstring.hpp"
#include <device.hpp>
#include <server.hpp>

#include <stm32f7xx_hal.h>

namespace lg {

void Server::initHttpEntryPoint(void* params)
{
    auto instance = reinterpret_cast<Server*>(params);
    instance->initHttpMain();
}

void Server::initialize()
{
    m_httpRootTaskHandle = xTaskCreateStatic(
        &Server::initHttpEntryPoint /* Task function */,
        "HTTP Init Task" /* Task name */,
        m_httpRootTaskStack.size() /* Stack size */,
        this /* parameters */,
        configMAX_PRIORITIES - 1 /* Prority */,
        m_httpRootTaskStack.data() /* Task stack address */,
        &m_httpRootTaskTcb /* Task control block */
    );
}

static StaticString<8> ToHex(std::uint32_t in) 
{
    static auto alphabet = "0123456789abcdef";
    StaticString<8> out;

    for (int i = 0; i < 8; ++i) {
        out += alphabet[in & 0xF];
        in >>= 4;
    }

    return out;
}

void Server::initHttpMain()
{
    m_server.get("/hello", [&](Request& req, Response& res) {
        std::array<std::uint32_t, 3> id = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };

        res << "<h1>It works!</h1><p>The STM32 device id is: <b>";
        res << id[0] << '-' << id[1] << '-' << id[2] << "</b></p>";
    });

    m_server.get("/test", [&](Request& req, Response& res) {
        res << reinterpret_cast<uint32_t>(&res);
    });

    m_server.get("/test/:1", [&](Request& req, Response& res) {
        res << "I've got a parameter: " << req.params[1];
    });

    m_server.get("/me", [&](Request& req, Response& res) {
        res.headers.add("Content-Type", "application/json");

        auto networkMgr = Device::get().getNetworkManager();

        res << R"({"id":")";
        res << ToHex(HAL_GetUIDw0()).ToCStr() << '-' 
            << ToHex(HAL_GetUIDw1()).ToCStr() << '-' 
            << ToHex(HAL_GetUIDw2()).ToCStr();
        res << R"(","mac":")";
        res << networkMgr->getMacAddress().ToCStr();
        res << R"("})";
    });

    m_server.start();
    vTaskSuspend(nullptr);
}

};
