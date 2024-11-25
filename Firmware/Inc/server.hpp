#pragma once
#include "socket.hpp"

#include <leakguard/microhttp.hpp>

#include <array>

#include <FreeRTOS.h>
#include <task.h>

namespace lg {

class Server {
public:
    static void initHttpEntryPoint(void* params);

    using Server_t = HttpServer<EspSocketImpl, EspSocketImpl::MAX_CONNECTIONS>;
    using Request = Server_t::Request;
    using Response = Server_t::Response;

    Server() = default;

    void initialize();

private:
    void initHttpMain();
    void addGeneralRoutes();
    void addConfigRoutes();

    bool checkAuthorization(Request& req, Response& res);
    void addJsonHeader(Response& res);
    void respondBadRequest(Response& res);

    Server_t m_server;

    TaskHandle_t m_httpRootTaskHandle {};
    StaticTask_t m_httpRootTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 512> m_httpRootTaskStack {};
};

};
