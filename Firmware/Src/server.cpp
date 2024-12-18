#include <device.hpp>
#include <initializer_list>
#include <server.hpp>

#include <stm32f7xx_hal.h>

#include <ArduinoJson.hpp>

#include <array>
#include <initializer_list>
#include <limits>

extern "C" {
#include <base64.h>
}

namespace lg {

enum class JsonType {
    JSON_SIGNED,
    JSON_UNSIGNED,
    JSON_STRING,
    JSON_OBJECT,
};

struct JsonRule {
    const char* fieldName {};
    JsonType type {};
    bool optional {};
    bool nullable {};
};

template <size_t N>
bool validateJson(const ArduinoJson::StaticJsonDocument<N>& doc,
    std::initializer_list<JsonRule> rules)
{
    for (auto& rule : rules) {
        if (!doc.containsKey(rule.fieldName)) {
            if (rule.optional) {
                continue;
            }

            return false;
        }

        ArduinoJson::JsonVariantConst variant = doc[rule.fieldName];
        if (rule.nullable && variant.isNull()) {
            continue;
        }

        switch (rule.type) {
        case JsonType::JSON_SIGNED:
            if (!variant.is<std::int32_t>()) {
                return false;
            }
            break;
        case JsonType::JSON_UNSIGNED:
            if (!variant.is<std::uint32_t>()) {
                return false;
            }
            break;
        case JsonType::JSON_STRING:
            if (!variant.is<const char*>()) {
                return false;
            }
            break;
        case JsonType::JSON_OBJECT:
            if (!variant.is<ArduinoJson::JsonObjectConst>()) {
                return false;
            }
        }
    }

    return true;
}

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
        this /* Parameters */,
        configMAX_PRIORITIES - 1 /* Priority */,
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
    addGeneralRoutes();
    addBlockRoutes();
    addConfigRoutes();
    addProbeRoutes();
    addWaterRoutes();
    addCriteriaRoutes();

    m_server.start();
    vTaskSuspend(nullptr);
}

void Server::addGeneralRoutes()
{
    m_server.get("/hello", [this](Request& req, Response& res) {
        std::array<std::uint32_t, 3> id = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };

        res << "<h1>It works!</h1><p>The STM32 device id is: <b>";
        res << id[0] << '-' << id[1] << '-' << id[2] << "</b></p>";
    });

    m_server.get("/me", [this](Request& req, Response& res) {
        addJsonHeader(res);

        auto networkMgr = Device::get().getNetworkManager();

        res << R"({"id":")";
        res << ToHex(HAL_GetUIDw0()).ToCStr() << '-'
            << ToHex(HAL_GetUIDw1()).ToCStr() << '-'
            << ToHex(HAL_GetUIDw2()).ToCStr();
        res << R"(","mac":")";
        res << networkMgr->getMacAddress().ToCStr();
        res << R"("})";
    });

    m_server.get("/check-auth", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        addJsonHeader(res);
        res << R"({"status":"ok"})";
    });
}

void Server::addBlockRoutes()
{
    static constexpr std::array<const char*, 7> weekdays = {
        "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
    };

    m_server.get("/water-block/schedule", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        std::array<std::uint32_t, 7> weeklySchedule {};
        {
            auto configService = Device::get().getConfigService();
            auto& currentConfig = configService->getCurrentConfig();

            weeklySchedule = currentConfig.weeklySchedule;
        }

        addJsonHeader(res);
        res << '{';

        for (size_t i = 0; i < weekdays.size(); ++i) {
            std::uint32_t value = weeklySchedule.at(i);
            bool enabled = (value & ConfigService::BLOCKADE_ENABLED_FLAG) != 0;

            res << '"';
            res << weekdays.at(i);
            res << R"(":{"enabled":)";
            res << (enabled ? "true" : "false");
            res << R"(,"hours":[)";

            for (size_t hour = 0; hour < 24; ++hour) {
                bool hourBlocked = (value & (1 << hour)) != 0;
                res << (hourBlocked ? "true" : "false");

                if (hour < 23) {
                    res << ',';
                }
            }
            res << "]}";
            if (i < weekdays.size() - 1) {
                res << ',';
            }
        }

        res << '}';
    });

    m_server.put("/water-block/schedule", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        ArduinoJson::StaticJsonDocument<4096> doc;
        auto error = ArduinoJson::deserializeJson(
            doc, req.body.begin(), req.body.GetSize());

        if (error != ArduinoJson::DeserializationError::Ok) {
            return respondBadRequest(res);
        }

        if (!validateJson(doc,
                {
                    JsonRule { weekdays[0], JsonType::JSON_OBJECT },
                    JsonRule { weekdays[1], JsonType::JSON_OBJECT },
                    JsonRule { weekdays[2], JsonType::JSON_OBJECT },
                    JsonRule { weekdays[3], JsonType::JSON_OBJECT },
                    JsonRule { weekdays[4], JsonType::JSON_OBJECT },
                    JsonRule { weekdays[5], JsonType::JSON_OBJECT },
                    JsonRule { weekdays[6], JsonType::JSON_OBJECT },
                })) {
            return respondBadRequest(res);
        }

        // Additional validation is required
        for (auto weekday : weekdays) {
            auto dayObject = doc[weekday];
            if (!dayObject.containsKey("enabled") || !dayObject.containsKey("hours")) {
                return respondBadRequest(res);
            }

            if (!dayObject["enabled"].is<bool>() || !dayObject["hours"].is<ArduinoJson::JsonArray>()) {
                return respondBadRequest(res);
            }

            if (dayObject["hours"].as<ArduinoJson::JsonArray>().size() != 24) {
                return respondBadRequest(res);
            }
        }

        std::array<std::uint32_t, 7> weeklySchedule {};
        for (size_t i = 0; i < weekdays.size(); ++i) {
            auto dayObject = doc[weekdays.at(i)];
            std::uint32_t value = 0;
            if (dayObject["enabled"].as<bool>()) {
                value |= ConfigService::BLOCKADE_ENABLED_FLAG;
            }

            for (size_t hour = 0; hour < 24; ++hour) {
                if (dayObject["hours"][hour].as<bool>()) {
                    value |= (1 << hour);
                }
            }

            weeklySchedule.at(i) = value;
        }

        {
            auto configService = Device::get().getConfigService();
            auto& currentConfig = configService->getCurrentConfig();
            currentConfig.weeklySchedule = weeklySchedule;
            configService->commit();
        }

        Device::get().getValveService()->update();

        res.status(HttpStatusCode::NoContent_204);
    });

    m_server.get("/water-block", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        addJsonHeader(res);

        auto valveService = Device::get().getValveService();
        res << R"({"block":)";
        res << (valveService->isValveBlocked() ? R"("active")" : R"("inactive")");
        res << '}';
    });

    m_server.post("/water-block", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        ArduinoJson::StaticJsonDocument<128> doc;
        auto error = ArduinoJson::deserializeJson(
            doc, req.body.begin(), req.body.GetSize());

        if (error != ArduinoJson::DeserializationError::Ok) {
            return respondBadRequest(res);
        }

        if (!validateJson(doc,
                {
                    JsonRule { "block", JsonType::JSON_STRING },
                })) {
            return respondBadRequest(res);
        }

        StaticString<16> action = doc["block"].as<const char*>();
        if (action == STR("active")) {
            auto valveService = Device::get().getValveService();
            valveService->blockDueTo(ValveService::BlockReason::USER_BLOCK);
        } else if (action == STR("inactive")) {
            {
                auto probeService = Device::get().getProbeService();
                probeService->stopAlarm();
            }
            {
                auto valveService = Device::get().getValveService();
                valveService->unblock();
            }
        } else {
            return respondBadRequest(res);
        }
    });
}

void Server::addConfigRoutes()
{
    m_server.get("/config", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        auto configService = Device::get().getConfigService();
        auto& currentConfig = configService->getCurrentConfig();

        ArduinoJson::StaticJsonDocument<512> doc;
        doc["ssid"] = currentConfig.wifiSsid.ToCStr();
        doc["passphrase"] = currentConfig.wifiPassword.ToCStr();
        doc["flow_meter_impulses"] = currentConfig.impulsesPerLiter;
        doc["valve_type"] = currentConfig.valveTypeNC ? "nc" : "no";
        doc["timezone_id"] = currentConfig.timezoneId;

        addJsonHeader(res);
        ArduinoJson::serializeJson(doc, res);
    });

    m_server.put("/config", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        ArduinoJson::StaticJsonDocument<512> doc;
        auto error = ArduinoJson::deserializeJson(
            doc, req.body.begin(), req.body.GetSize());

        if (error != ArduinoJson::DeserializationError::Ok) {
            return respondBadRequest(res);
        }

        if (!validateJson(doc,
                {
                    JsonRule { "ssid", JsonType::JSON_STRING },
                    JsonRule { "passphrase", JsonType::JSON_STRING },
                    JsonRule { "flow_meter_impulses", JsonType::JSON_UNSIGNED },
                    JsonRule { "valve_type", JsonType::JSON_STRING },
                    JsonRule { "timezone_id", JsonType::JSON_UNSIGNED },
                })) {
            return respondBadRequest(res);
        }

        {
            auto configService = Device::get().getConfigService();
            auto& currentConfig = configService->getCurrentConfig();
            currentConfig.wifiSsid = doc["ssid"].as<const char*>();
            currentConfig.wifiPassword = doc["passphrase"].as<const char*>();
            currentConfig.impulsesPerLiter = doc["flow_meter_impulses"].as<std::uint32_t>();
            currentConfig.timezoneId = doc["timezone_id"].as<std::uint32_t>();
            StaticString<2> valveType = doc["valve_type"].as<const char*>();
            currentConfig.valveTypeNC = valveType == STR("nc");
            configService->commit();

            Device::get().setLocalTimezone(currentConfig.timezoneId);
        }

        Device::get().getNetworkManager()->reloadCredentialsOneShot();
        Device::get().getValveService()->update();
        res.status(HttpStatusCode::NoContent_204);
    });

    m_server.put("/config/password", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        ArduinoJson::StaticJsonDocument<128> doc;
        auto error = ArduinoJson::deserializeJson(
            doc, req.body.begin(), req.body.GetSize());

        if (error != ArduinoJson::DeserializationError::Ok) {
            return respondBadRequest(res);
        }

        if (!validateJson(doc, { JsonRule { "password", JsonType::JSON_STRING } })) {
            return respondBadRequest(res);
        }

        {
            auto configService = Device::get().getConfigService();
            auto& currentConfig = configService->getCurrentConfig();
            currentConfig.adminPassword = doc["password"].as<const char*>();
            configService->commit();
        }

        res.status(HttpStatusCode::NoContent_204);
    });
}

void Server::addCriteriaRoutes()
{
    m_server.get("/criteria", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        const auto leakLogicManager = Device::get().getLeakLogicManager();
        const auto criteriaString = leakLogicManager->getCriteriaString();

        addJsonHeader(res);
        res << R"({"criteria":")";
        res << criteriaString.ToCStr();
        res << R"("})";
    });

    m_server.post("/criteria", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        ArduinoJson::StaticJsonDocument<512> doc;
        auto error = ArduinoJson::deserializeJson(
            doc, req.body.begin(), req.body.GetSize());

        if (error != ArduinoJson::DeserializationError::Ok) {
            return respondBadRequest(res);
        }

        if (!validateJson(doc, { JsonRule { "criteria", JsonType::JSON_STRING } })) {
            return respondBadRequest(res);
        }

        const auto leakLogicManager = Device::get().getLeakLogicManager();
        const auto criteriaString = StaticString<64>(doc["criteria"].as<const char*>());

        leakLogicManager->loadFromString(criteriaString); // TODO: Check for malformed criteria strings - WILL CAUSE A SEGFAULT if they're malformed
        leakLogicManager->saveConfiguration();

        res.status(HttpStatusCode::OK_200);
    });
}

static void printProbeInfo(Server::Response& res, const ProbeService::ProbeInfo& info)
{
    res << R"({"id":[)";
    res << info.id1 << ',' << info.id2 << ',' << info.id3;
    res << R"(],"battery_level":)";
    if (info.batteryPercent == 0xFF) {
        res << "null";
    } else {
        res << static_cast<std::uint32_t>(info.batteryPercent);
    }
    res << R"(,"blocked":)";
    res << (info.isIgnored ? "true" : "false");
    res << R"(,"is_alerted":)";
    res << (info.isAlerted ? "true" : "false");
    res << R"(,"last_rssi":)";
    if (info.lastRssi == ProbeService::INVALID_RSSI) {
        res << "null";
    } else {
        res << info.lastRssi;
    }
    res << '}';
}

void Server::addProbeRoutes()
{
    m_server.post("/probe/pair/enter", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        auto probeService = Device::get().getProbeService();

        if (probeService->enterPairingMode()) {
            res.status(HttpStatusCode::NoContent_204);
        } else {
            res.status(HttpStatusCode::Conflict_409);
        }
    });

    m_server.post("/probe/pair/exit", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        auto probeService = Device::get().getProbeService();

        if (probeService->leavePairingMode()) {
            res.status(HttpStatusCode::NoContent_204);
        } else {
            res.status(HttpStatusCode::Conflict_409);
        }
    });

    m_server.get("/probe/pair", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        bool isPairing = false;

        {
            auto probeService = Device::get().getProbeService();
            isPairing = probeService->isInPairingMode();
        }

        addJsonHeader(res);
        res << R"({"pairing":)";
        res << (isPairing ? "true" : "false");
        res << R"(})";
    });

    m_server.get("/probe", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        addJsonHeader(res);
        auto probeService = Device::get().getProbeService();
        bool first = true;

        res << '{';
        for (auto& probe : probeService->getPairedProbesInfo()) {
            if (!first) {
                res << ',';
            }

            res << '"' << static_cast<std::uint16_t>(probe.masterAddress) << '"' << ':';
            printProbeInfo(res, probe);

            first = false;
        }
        res << '}';
    });

    m_server.get("/probe/id/:1", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        if (req.params.at(1) > std::numeric_limits<std::uint8_t>::max()) {
            return respondBadRequest(res);
        }

        auto probeService = Device::get().getProbeService();
        auto probeInfo = probeService->getPairedProbeInfo(
            static_cast<std::uint8_t>(req.params.at(1)));

        if (!probeInfo) {
            res.status(HttpStatusCode::NotFound_404);
            return;
        }

        addJsonHeader(res);
        printProbeInfo(res, *probeInfo);
    });

    m_server.put("/probe/id/:1", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        if (req.params.at(1) > std::numeric_limits<std::uint8_t>::max()) {
            return respondBadRequest(res);
        }

        ArduinoJson::StaticJsonDocument<128> doc;
        auto error = ArduinoJson::deserializeJson(
            doc, req.body.begin(), req.body.GetSize());

        if (error != ArduinoJson::DeserializationError::Ok) {
            return respondBadRequest(res);
        }

        if (!validateJson(doc, { JsonRule { "verb", JsonType::JSON_STRING } })) {
            return respondBadRequest(res);
        }

        StaticString<8> verb = doc["verb"].as<const char*>();
        auto probeService = Device::get().getProbeService();

        if (verb == STR("block")) {
            if (probeService->setProbeIgnored(
                    static_cast<std::uint8_t>(req.params.at(1)), true)) {

                res.status(HttpStatusCode::NoContent_204);
            } else {
                res.status(HttpStatusCode::NotFound_404);
            }
        } else if (verb == STR("unblock")) {
            if (probeService->setProbeIgnored(
                    static_cast<std::uint8_t>(req.params.at(1)), false)) {

                res.status(HttpStatusCode::NoContent_204);
            } else {
                res.status(HttpStatusCode::NotFound_404);
            }
        } else {
            return respondBadRequest(res);
        }
    });

    m_server.del("/probe/id/:1", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        if (req.params.at(1) > std::numeric_limits<std::uint8_t>::max()) {
            return respondBadRequest(res);
        }

        auto probeService = Device::get().getProbeService();
        if (probeService->unpairProbe(static_cast<std::uint8_t>(req.params.at(1)))) {
            res.status(HttpStatusCode::NoContent_204);
        } else {
            res.status(HttpStatusCode::NotFound_404);
        }
    });
}

void Server::addWaterRoutes()
{
    m_server.get("/water-usage", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        std::uint32_t flowMl = 0, totalMl = 0, todayMl = 0;
        {
            auto flowMeter = Device::get().getFlowMeterService();
            flowMl = flowMeter->getCurrentFlowInMlPerMinute();
            totalMl = flowMeter->getTotalVolumeInMl();
            todayMl = flowMeter->getTodayFlowInMl();
        }

        addJsonHeader(res);
        res << R"({"flow_rate":)";
        res << flowMl;
        res << R"(,"total_volume":)";
        res << totalMl;
        res << R"(,"today_volume":)";
        res << todayMl;
        res << R"(})";
    });

    m_server.get("/water-usage/today", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        auto historyService = Device::get().getHistoryService();
        addJsonHeader(res);
        res << R"({"interval_minutes":1,"usages":{)";

        historyService->forEachNewestHistoryEntry(
            [&res](std::size_t index, const HistoryService::EepromHistoryEntry& entry) {
                if (index) {
                    res << ',';
                }

                res << '"' << entry.timestamp << '"' << ':';
                res << entry.volumeMl;
            });

        res << "}}";
    });

    m_server.get("/water-usage/:1/:2", [this](Request& req, Response& res) {
        if (!checkAuthorization(req, res)) {
            return;
        }

        auto historyService = Device::get().getHistoryService();
        addJsonHeader(res);
        res << R"({"interval_minutes":60,"usages":{)";

        historyService->forEachFlashHistoryEntry(
            req.params.at(1), req.params.at(2),
            [&res](std::size_t index, const HistoryService::FlashHistoryEntry& entry) {
                UtcTime entryTime(entry.fromTimestamp);
                entryTime.setMinute(0);
                entryTime.setSecond(0);
                auto initialUtcTimestamp = entryTime.toTimestamp();

                UtcTime localTime = Device::get().getLocalTimeForUtcTimestamp(
                    initialUtcTimestamp);
                auto timestamp = initialUtcTimestamp - localTime.getMinute() * 60 - localTime.getSecond();

                int day = localTime.getDay();
                bool first = true;

                while (true) {
                    UtcTime currentTime = Device::get().getLocalTimeForUtcTimestamp(timestamp);
                    if (currentTime.getDay() != day) {
                        break;
                    }

                    if (index || !first) {
                        res << ',';
                    }

                    int hour = currentTime.getHour();
                    res << '"' << timestamp << '"' << ':';
                    res << entry.hourVolumesMl.at(hour);

                    timestamp += 3600;
                    first = false;
                }
            });

        res << "}}";
    });
}

bool Server::checkAuthorization(Request& req, Response& res)
{
    int authorizationTag = req.headers.find(StaticString<32>("authorization"));
    if (authorizationTag < 0) {
        addJsonHeader(res);
        addAuthenticateHeader(res);
        res.status(HttpStatusCode::Unauthorized_401);
        res << R"({"status":"unauthorized"})";
        return false;
    }

    auto& authorizationValue = req.headers[authorizationTag].second;
    if (!authorizationValue.StartsWith(STR("Basic "))) {
        addJsonHeader(res);
        addAuthenticateHeader(res);
        res.status(HttpStatusCode::Unauthorized_401);
        res << R"({"status":"unauthorized"})";
        return false;
    }

    StaticString<64> expectedCredentials = "root:";
    expectedCredentials += Device::get().getConfigService()->getCurrentConfig().adminPassword;

    std::array<char, 128> out {};
    base64_encode(reinterpret_cast<const unsigned char*>(expectedCredentials.begin()),
        expectedCredentials.GetSize(), out.data());

    bool ok = strcmp(authorizationValue.ToCStr() + 6, out.data()) == 0;

    if (!ok) {
        addJsonHeader(res);
        addAuthenticateHeader(res);
        res.status(HttpStatusCode::Unauthorized_401);
        res << R"({"status":"unauthorized"})";
    }

    return ok;
}

void Server::addJsonHeader(Response& res)
{
    res.headers.add("Content-Type", "application/json");
}

void Server::addAuthenticateHeader(Response& res)
{
    res.headers.add("WWW-Authenticate", R"(Basic realm="LeakGuard")");
}

void Server::respondBadRequest(Response& res)
{
    addJsonHeader(res);
    res.status(HttpStatusCode::BadRequest_400);
    res << R"({"status":"bad request"})";
}

};
