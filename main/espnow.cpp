#include "espnow.h"

// 3rdparty lib includes
#include <esp_log.h>
#include <espwifistack.h>
#include <fmt/core.h>
#include <driver/uart.h>

// local includes
#include "config.h"

constexpr const char * const TAG = "ESP_NOW";

namespace {
enum class InitState : uint8_t {
  UNINITIALIZED,
  ESP_NOW_INIT,
  REGISTER_RECEIVE_CALLBACK,
  REGISTER_SEND_CALLBACK,
  ADD_PEER,
  INIT_DONE
};
InitState initState{InitState::UNINITIALIZED};
} // namespace

namespace espnow {
bool initAllowed()
{
    const auto wifi_mode = wifi_stack::get_wifi_mode();
    return (wifi_mode == WIFI_MODE_STA || wifi_mode == WIFI_MODE_AP || wifi_mode == WIFI_MODE_APSTA);
}
esp_err_t _sendEspNowImpl(uint8_t *data, size_t size, const uint8_t *destination)
{
    if (initState != InitState::INIT_DONE)
        return ESP_ERR_ESPNOW_NOT_INIT;

    if (!configs.wifiApEnabled.value && !configs.wifiStaEnabled.value)
        return ESP_ERR_ESPNOW_IF;

    if (peers.empty())
        return ESP_FAIL;

    for (auto &peer : peers)
    {
        if (peer.peer_addr == destination)
        {
            if (configs.wifiApEnabled.value)
                peer.ifidx = WIFI_IF_AP;
            else if (configs.wifiStaEnabled.value)
                peer.ifidx = WIFI_IF_STA;
            else
                return ESP_ERR_ESPNOW_IF;

            if (const auto error = esp_now_send(peer.peer_addr, data, size); error != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(error));
                return error;
            }
            return ESP_OK;
        }
    }
    return ESP_ERR_ESPNOW_NOT_FOUND;
}

extern "C" void _recvCb(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    const std::string_view data_str{(const char*) data, size_t(data_len)};
    char macStr[18] = {0};
    sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    const std::string out = fmt::format("\u001b[32m[{}] --> {}\u001b[0m\n", macStr, data_str);
    uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, out.data(), out.size());
}

extern "C" void _sendCb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    /*
    char macStr[18] = {0};
    sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, "[%s] --> %s", macStr, status == ESP_NOW_SEND_SUCCESS ? "success" : "fail");
     */
}

std::vector<esp_now_peer_info_t> peers{};
} // namespace espnow

void initEspNow()
{
    ESP_LOGI(TAG, "Initializing ESP-NOW");

    switch (initState)
    {
    case InitState::UNINITIALIZED:
        if (!espnow::initAllowed())
        {
            ESP_LOGI(TAG, "cannot init espnow: wifi stack down");
            return;
        }
        initState = InitState::ESP_NOW_INIT;
    case InitState::ESP_NOW_INIT:
        if (const auto error = esp_now_init(); error != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_now_init failed with %s", esp_err_to_name(error));
            return;
        }
        initState = InitState::REGISTER_RECEIVE_CALLBACK;
    case InitState::REGISTER_RECEIVE_CALLBACK:
        if (const auto error = esp_now_register_recv_cb(espnow::_recvCb); error != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_now_register_recv_cb failed with %s", esp_err_to_name(error));
            return;
        }
        initState = InitState::REGISTER_SEND_CALLBACK;
    case InitState::REGISTER_SEND_CALLBACK:
        if (const auto error = esp_now_register_send_cb(espnow::_sendCb); error != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_now_register_send_cb failed with %s", esp_err_to_name(error));
            return;
        }
        initState = InitState::ADD_PEER;
    case InitState::ADD_PEER:
    {
        espnow::peers.push_back(esp_now_peer_info_t{});
        esp_now_peer_info_t &peer = espnow::peers.back();
        std::memcpy(peer.peer_addr, broadcastAddress, sizeof(peer.peer_addr));
        peer.channel = 0;

        if (configs.wifiApEnabled.value)
            peer.ifidx = WIFI_IF_AP;
        else if (configs.wifiStaEnabled.value)
            peer.ifidx = WIFI_IF_STA;
        else {
            ESP_LOGE(TAG, "cannot init espnow: wifi stack down");
            return;
        }

        if (const auto error = esp_now_add_peer(&espnow::peers.back()); error != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_add_peer failed with %s", esp_err_to_name(error));
            return;
        }
        initState = InitState::INIT_DONE;
    }
    case InitState::INIT_DONE:
        ESP_LOGI(TAG, "ESP-NOW initialized");
        break;
    }
}

void deinitEspNow()
{
    if (initState != InitState::INIT_DONE)
        return;

    ESP_LOGI(TAG, "Deinitializing ESP-NOW");

    switch (initState)
    {
    case InitState::INIT_DONE:
        // del all peers
        for (const auto &peer: espnow::peers) {
            if (const auto error = esp_now_del_peer(peer.peer_addr); error != ESP_OK) {
                if (error == ESP_ERR_ESPNOW_NOT_FOUND) {
                    initState = InitState::UNINITIALIZED;
                    return;
                }
                ESP_LOGE(TAG, "esp_now_del_peer failed with %s", esp_err_to_name(error));
                return;
            }
        }
        espnow::peers.clear();
        initState = InitState::ADD_PEER;
    case InitState::ADD_PEER:
        if (const auto error = esp_now_unregister_send_cb(); error != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_unregister_send_cb failed with %s", esp_err_to_name(error));
            return;
        }
        else if (error == ESP_ERR_ESPNOW_NOT_FOUND) {
            initState = InitState::UNINITIALIZED;
            return;
        }
        initState = InitState::REGISTER_SEND_CALLBACK;
    case InitState::REGISTER_SEND_CALLBACK:
        if (const auto error = esp_now_unregister_recv_cb(); error != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_unregister_recv_cb failed with %s", esp_err_to_name(error));
            return;
        }
        else if (error == ESP_ERR_ESPNOW_NOT_FOUND) {
            initState = InitState::UNINITIALIZED;
            return;
        }
        initState = InitState::REGISTER_RECEIVE_CALLBACK;
    case InitState::REGISTER_RECEIVE_CALLBACK:
        if (const auto error = esp_now_deinit(); error != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_deinit failed with %s", esp_err_to_name(error));
            return;
        }
        else if (error == ESP_ERR_ESPNOW_NOT_FOUND) {
            initState = InitState::UNINITIALIZED;
            return;
        }
        initState = InitState::ESP_NOW_INIT;
    case InitState::ESP_NOW_INIT:
        initState = InitState::UNINITIALIZED;
        break;
    case InitState::UNINITIALIZED:
        ESP_LOGI(TAG, "ESP-NOW deinitialized");
        break;
    }
}

void handleEspNow()
{
    const auto initAllowed = espnow::initAllowed();
    if (initState != InitState::INIT_DONE && initAllowed)
    {
        initEspNow();
        return;
    }
    else if (!initAllowed && initState == InitState::INIT_DONE)
    {
        deinitEspNow();
        return;
    }

    if (initState != InitState::INIT_DONE)
        return;
}

esp_err_t sendEspNow(std::string data)
{
    return espnow::_sendEspNowImpl(reinterpret_cast<uint8_t *>(data.data()), data.size(), broadcastAddress);
}

esp_err_t sendEspNow(std::string data, uint8_t *destination)
{
    return espnow::_sendEspNowImpl(reinterpret_cast<uint8_t *>(data.data()), data.size(), destination);
}

esp_err_t sendEspNow(uint8_t *data, size_t size)
{
    return espnow::_sendEspNowImpl(data, size, broadcastAddress);
}

esp_err_t sendEspNow(uint8_t *data, size_t size, uint8_t *destination)
{
    return espnow::_sendEspNowImpl(data, size, destination);
}
