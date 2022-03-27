#include "ota.h"

// system includes
#include <chrono>
#include <string>

// esp-idf includes
#include <esp_log.h>

// 3rdparty lib includes
#include <fmt/core.h>
#include <delayedconstruction.h>
#include <espasyncota.h>
#include <espwifistack.h>
#include <recursivelockhelper.h>

// local includes
#include "config.h"

using namespace std::chrono_literals;

namespace {
constexpr const char * const TAG = "OTA_CLIENT";

cpputils::DelayedConstruction<EspAsyncOta> _otaClient;
} // namespace

EspAsyncOta &otaClient{_otaClient.getUnsafe()};

void ota_client_init()
{
    ESP_LOGI(TAG, "called");

    _otaClient.construct("asyncOtaTask", 8192u);
}

void ota_client_update()
{
    _otaClient->update();
}

tl::expected<void, std::string> otaClientTrigger(std::string_view url)
{
    if (auto result = _otaClient->trigger(url, {}, {}, {}); !result)
        return tl::make_unexpected(std::move(result).error());

    wifi_stack::delete_scan_result();

    return {};
}

tl::expected<void, std::string> otaClientAbort()
{
    if (auto result = _otaClient->abort(); !result)
        return tl::make_unexpected(std::move(result).error());

    return {};
}
