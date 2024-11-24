#pragma once
#include <leakguard/staticstring.hpp>
#include <leakguard/staticvector.hpp>

#include <array>
#include <cstdint>
#include <functional>

#include <FreeRTOS.h>
#include <semphr.h>
#include <stm32f7xx_hal.h>
#include <task.h>

#define ESP_AT_MAX_IPD_BYTES 2920
#define ESP_LINE_BUFFER_SIZE 512
#define ESP_RESPONSE_BUFFER_SIZE 2048
#define ESP_AP_LIST_SIZE 20
#define ESP_IP_STRING_SIZE 16
#define ESP_MAC_STRING_SIZE 20
#define ESP_ASCTIME_STRING_SIZE 32
#define ESP_RX_DONE (1 << 0)
#define ESP_READY (1 << 1)

namespace lg {

class EspAtDriver {
public:
    static inline constexpr auto MAX_CONNECTIONS = 5;
    static inline constexpr auto MAX_DATA_CHUNK_SIZE = 256;
    static inline constexpr auto MAX_INACTIVITY_TIME_MS = 5000;
    static inline constexpr auto DEFAULT_TIMEOUT = 3000;

    static void initTaskEntryPoint(void* params);
    static void uartRxTaskEntryPoint(void* params);
    static void connectionCloserEntryPoint(void* params);

    enum class EspWifiStatus {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        DHCP_GOT_IP,
    };

    enum class EspResponse {
        OK,
        ERROR,
        SEND_OK,
        SEND_FAIL,
        SET_OK,
        DRIVER_ERROR,
        ESP_TIMEOUT,
    };

    enum class EspWifiMode {
        OFF,
        STATION,
        AP,
        STATION_AND_AP,
    };

    enum class Encryption {
        OPEN,
        WEP,
        WPA_PSK,
        WPA2_PSK,
        WPA_WPA2_PSK,
        WPA2_ENTERPRISE,
        WPA3_PSK,
        WPA2_WPA3_PSK,
        WAPI_PSK,
        OWE
    };

    enum class CipherType {
        NONE,
        WEP40,
        WEP104,
        TKIP,
        CCMP,
        TKIP_AND_CCMP,
        AES_CMAC_128,
        UNKNOWN,
    };

    enum class MqttScheme {
        INVALID,
        MQTT_TCP,
        MQTT_TLS_NO_VERIFY,
        MQTT_TLS_VERIFY_SERVER,
        MQTT_TLS_PROVIDE_CLIENT,
        MQTT_TLS_VERIFY_SERVER_AND_PROVIDE_CLIENT,
        MQTT_WS,
        MQTT_WSS_NO_VERIFY,
        MQTT_WSS_VERIFY_SERVER,
        MQTT_WSS_PROVIDE_CLIENT,
        MQTT_WSS_VERIFY_SERVER_AND_PROVIDE_CLIENT,
    };

    enum class MqttQoS {
        AT_MOST_ONCE,
        AT_LEAST_ONCE,
        EXACTLY_ONCE
    };

    struct AccessPoint {
        Encryption encryption;
        StaticString<32> ssid;
        int rssi;
        StaticString<18> mac;
        int channel;
        int freqOffset;
        int freqCalVal;
        CipherType pairwiseCipher;
        CipherType groupCipher;
        int bgn;
        bool wpsEnabled;
    };

    EspAtDriver(UART_HandleTypeDef* iface)
        : m_usart(iface)
        , m_connectionOpen({ false })
        , m_connectionLastActivity({ 0 })
    {
    }

    void initialize();

    bool isReady() const { return m_ready; }
    EspWifiStatus getWifiStatus() const { return m_wifiStatus; }

    EspResponse startTcpServer(std::uint16_t portNumber);
    EspResponse stopTcpServer();
    EspResponse closeConnection(int linkId);
    EspResponse closeAllConnections();
    EspResponse sendData(int linkId, const char* data, std::size_t size);

    EspResponse setWifiMode(EspWifiMode mode);
    EspResponse joinAccessPoint(const char* ssid, const char* password);
    EspResponse listAccessPoints(StaticVector<AccessPoint, ESP_AP_LIST_SIZE>& out);
    EspResponse quitAccessPoint();
    EspResponse setupSoftAp(const char* ssid,
        const char* password, int channel, Encryption encryption);
    EspResponse queryStationIp(StaticString<ESP_IP_STRING_SIZE>& out);

    EspResponse disableMdns();
    EspResponse enableMdns(const char* hostname, const char* service, std::uint16_t port);

    EspResponse setHostname(const char* hostname);

    EspResponse getRssi(int& rssiOut);
    EspResponse getStationMacAddress(StaticString<ESP_MAC_STRING_SIZE>& out);
    EspResponse getApMacAddress(StaticString<ESP_MAC_STRING_SIZE>& out);

    EspResponse configureSntp(int timezone,
        const char* server1, const char* server2 = nullptr, const char* server3 = nullptr);
    EspResponse setSntpUpdateInterval(int seconds);
    EspResponse querySntpTime(StaticString<ESP_ASCTIME_STRING_SIZE>& asctime);

    EspResponse configureMqttUser(MqttScheme scheme, const char* clientId,
        const char* username, const char* password, const char* path);
    EspResponse mqttConnectToBroker(const char* host, std::uint16_t port = MQTT_PORT);
    EspResponse mqttPublish(const char* topic, const char* data, MqttQoS qos, bool retain);
    EspResponse mqttSubscribe(const char* topic, MqttQoS qos);

    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)

    std::function<void(int)> onConnected;
    std::function<void(int)> onClosed;
    std::function<void(int, const char*, std::size_t)> onData;
    std::function<void()> onSntpTime;

    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

private:
    static constexpr auto MQTT_TIMEOUT_MS = 10000;
    static constexpr auto MQTT_PORT = 1883;

    class Lock {
    public:
        Lock(EspAtDriver* owner)
            : m_owner(owner)
        {
            xSemaphoreTake(owner->m_mutex, portMAX_DELAY);
        }

        ~Lock() { xSemaphoreGive(m_owner->m_mutex); }

        // Rule of 5
        Lock(const Lock&) = delete;
        Lock(Lock&&) = delete;
        Lock& operator=(const Lock&) = delete;
        Lock& operator=(Lock&&) = delete;

    private:
        EspAtDriver* m_owner;
    };

    Lock acquireLock() { return Lock(this); }
    void initTaskMain();
    void uartRxTaskMain();
    void connectionCloserMain();

    void clearResponsePrefix();
    void setResponsePrefix(const char* data);
    void appendAtString(const char* data);

    int readAtInteger(
        const StaticString<ESP_RESPONSE_BUFFER_SIZE>& buffer, std::size_t& position);

    template <std::size_t outSize>
    void readAtString(
        const StaticString<ESP_RESPONSE_BUFFER_SIZE>& buffer,
        std::size_t& position, StaticString<outSize>& out);

    void parseEspResponse(const StaticString<ESP_LINE_BUFFER_SIZE>& buffer);
    bool parseEspNotification(const StaticString<ESP_LINE_BUFFER_SIZE>& buffer);
    void finishRequest(EspResponse response);
    bool gotPrompt();
    void gotConnect(int linkId);
    void gotClosed(int linkId);
    void gotData(int linkId, const char* data, std::size_t size);
    void gotTimeUpdated();
    void closeIdleConnections();

    void parseInputData(const StaticString<ESP_LINE_BUFFER_SIZE>& buffer);

    EspResponse sendCommandDirectAndWait(const char* data,
        std::uint32_t timeout = DEFAULT_TIMEOUT);
    EspResponse sendCommandBufferAndWait(std::uint32_t timeout = DEFAULT_TIMEOUT);
    void waitForDmaReady();

    volatile bool m_ready { false };

    UART_HandleTypeDef* m_usart;

    TaskHandle_t m_initTaskHandle {};
    TaskHandle_t m_uartRxTaskHandle {};
    TaskHandle_t m_connectionCloserTaskHandle {};
    StaticTask_t m_initTaskTcb {};
    StaticTask_t m_uartRxTaskTcb {};
    StaticTask_t m_connectionCloserTaskTcb {};
    std::array<configSTACK_DEPTH_TYPE, 128> m_initTaskStack {};
    std::array<configSTACK_DEPTH_TYPE, 256> m_uartRxTaskStack {};
    std::array<configSTACK_DEPTH_TYPE, 64> m_connectionCloserTaskStack {};

    SemaphoreHandle_t m_mutex {};
    StaticSemaphore_t m_mutexBuffer {};

    StaticQueue_t m_connectionsToCloseQ {};
    QueueHandle_t m_connectionsToCloseHandle {};
    std::array<int, 16> m_connectionsToCloseBuffer {};

    StaticString<ESP_LINE_BUFFER_SIZE> m_txLineBuffer {};
    StaticString<32> m_responsePrefix;
    StaticString<ESP_RESPONSE_BUFFER_SIZE> m_responseBuffer;

    std::array<char, 16384> m_uartRxBuffer {};
    std::array<volatile bool, MAX_CONNECTIONS> m_connectionOpen;
    std::array<volatile TickType_t, MAX_CONNECTIONS> m_connectionLastActivity;

    volatile EspResponse m_currentResponse {};
    volatile TaskHandle_t m_requestInitiator {};
    volatile bool m_waitingForPrompt {};
    volatile EspWifiStatus m_wifiStatus { EspWifiStatus::DISCONNECTED };

    int m_ipdLinkId {};
    uint32_t m_ipdRemainingBytes {};
};

};
