#include "webserver.h"

#include "sdkconfig.h"

// system includes
#include <chrono>

// esp-idf includes
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_server.h>

// 3rdparty lib includes
#include <htmlbuilder.h>
#include <espcppmacros.h>
#include <esphttpdutils.h>
#include <tickchrono.h>
#include <espstrutils.h>
#include <fmt/core.h>
#include <espasyncota.h>
#include <futurecpp.h>
#include <espchrono.h>
#include <cpputils.h>
#include <numberparsing.h>

// local includes
#include "ota.h"
#include "config.h"

using namespace std::chrono_literals;
using esphttpdutils::HtmlTag;

httpd_handle_t httpdHandle;

namespace {
constexpr const char * const TAG = "WEBSERVER";

esp_err_t webserver_reboot_handler(httpd_req_t *req);

esp_err_t webserver_ota_handler(httpd_req_t *req);
esp_err_t webserver_trigger_ota_handler(httpd_req_t *req);

esp_err_t webserver_settings_handler(httpd_req_t *req);
esp_err_t webserver_saveSettings_handler(httpd_req_t *req);
esp_err_t webserver_resetSettings_handler(httpd_req_t *req);
} // namespace

void initWebserver()
{
    {
        httpd_config_t httpConfig HTTPD_DEFAULT_CONFIG();
        httpConfig.core_id = 1;
        httpConfig.max_uri_handlers = 16;
        httpConfig.stack_size = 8192;

        const auto result = httpd_start(&httpdHandle, &httpConfig);
        ESP_LOG_LEVEL_LOCAL((result == ESP_OK ? ESP_LOG_INFO : ESP_LOG_ERROR), TAG, "httpd_start(): %s", esp_err_to_name(result));
        if (result != ESP_OK)
            return;
    }

    for (const httpd_uri_t &uri : {
        httpd_uri_t { .uri = "/",                .method = HTTP_GET, .handler = webserver_settings_handler,        .user_ctx = NULL },
        httpd_uri_t { .uri = "/saveSettings",    .method = HTTP_GET, .handler = webserver_saveSettings_handler,    .user_ctx = NULL },
        httpd_uri_t { .uri = "/resetSettings",   .method = HTTP_GET, .handler = webserver_resetSettings_handler,   .user_ctx = NULL },

        httpd_uri_t { .uri = "/reboot",             .method = HTTP_GET, .handler = webserver_reboot_handler,             .user_ctx = NULL },

        httpd_uri_t { .uri = "/ota",                .method = HTTP_GET, .handler = webserver_ota_handler,                .user_ctx = NULL },
        httpd_uri_t { .uri = "/triggerOta",         .method = HTTP_GET, .handler = webserver_trigger_ota_handler,        .user_ctx = NULL },
    })
    {
        const auto result = httpd_register_uri_handler(httpdHandle, &uri);
        ESP_LOG_LEVEL_LOCAL((result == ESP_OK ? ESP_LOG_INFO : ESP_LOG_ERROR), TAG, "httpd_register_uri_handler() for %s: %s", uri.uri, esp_err_to_name(result));
        //if (result != ESP_OK)
        //    return result;
    }
}

void handleWebserver()
{
}

namespace {
esp_err_t webserver_reboot_handler(httpd_req_t *req)
{
    esp_restart();

    CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::Ok, "text/plain", "REBOOT called...")
}

esp_err_t webserver_ota_handler(httpd_req_t *req)
{
    std::string body;

    HtmlTag htmlTag{"html", body};

    {
        HtmlTag headTag{"head", body};

        {
            HtmlTag titleTag{"title", body};
            body += "Update";
        }

        body += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\" />";
    }

    {
        HtmlTag bodyTag{"body", body};

        {
            HtmlTag h1Tag{"h1", body};
            body += "Update";
        }

        {
            HtmlTag pTag{"p", body};
            body += "<a href=\"/\">Settings</a> - "
                    "<b>Update</b>";
        }

        if (const esp_app_desc_t *app_desc = esp_ota_get_app_description())
        {
            HtmlTag tableTag{"table", "border=\"1\"", body};

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Current project_name"; }
                { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(app_desc->project_name); }
            }

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Current version"; }
                { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(app_desc->version); }
            }

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Current secure_version"; }
                { HtmlTag tdTag{"td", body}; body += std::to_string(app_desc->secure_version); }
            }

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Current timestamp"; }
                { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(fmt::format("{} {}", app_desc->date, app_desc->time)); }
            }

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Current idf_ver"; }
                { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(app_desc->idf_ver); }
            }

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Current sha256"; }
                { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(espcpputils::toHexString({app_desc->app_elf_sha256, 8})); }
            }
        }
        else
        {
            constexpr const std::string_view msg = "esp_ota_get_app_description() failed";
            ESP_LOGE(TAG, "%.*s", msg.size(), msg.data());
            HtmlTag pTag{"p", "style=\"color: red;\"", body};
            body += esphttpdutils::htmlentities(msg);
        }

        {
            HtmlTag tableTag{"table", "border=\"1\"", body};

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Update status"; }
                { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(toString(otaClient.status())); }
            }

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Update progress"; }
                {
                    HtmlTag tdTag{"td", body};
                    const auto progress = otaClient.progress();
                    const auto totalSize = otaClient.totalSize();
                    body += fmt::format("{} / {}{}",
                                        progress,
                                        totalSize ? std::to_string(*totalSize) : "?",
                                        (totalSize && *totalSize > 0) ? fmt::format(" ({:.02f}%)", float(progress) / *totalSize * 100) : "");
                }
            }

            {
                HtmlTag trTag{"tr", body};
                { HtmlTag tdTag{"td", body}; body += "Update message"; }
                { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(otaClient.message()); }
            }

            if (const auto &appDesc = otaClient.appDesc())
            {
                {
                    HtmlTag trTag{"tr", body};
                    { HtmlTag tdTag{"td", body}; body += "New project_name"; }
                    { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(appDesc->project_name); }
                }

                {
                    HtmlTag trTag{"tr", body};
                    { HtmlTag tdTag{"td", body}; body += "New version"; }
                    { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(appDesc->version); }
                }

                {
                    HtmlTag trTag{"tr", body};
                    { HtmlTag tdTag{"td", body}; body += "New secure_version"; }
                    { HtmlTag tdTag{"td", body}; body += std::to_string(appDesc->secure_version); }
                }

                {
                    HtmlTag trTag{"tr", body};
                    { HtmlTag tdTag{"td", body}; body += "New timestamp"; }
                    { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(fmt::format("{} {}", appDesc->date, appDesc->time)); }
                }

                {
                    HtmlTag trTag{"tr", body};
                    { HtmlTag tdTag{"td", body}; body += "New idf_ver"; }
                    { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(appDesc->idf_ver); }
                }

                {
                    HtmlTag trTag{"tr", body};
                    { HtmlTag tdTag{"td", body}; body += "New sha256"; }
                    { HtmlTag tdTag{"td", body}; body += esphttpdutils::htmlentities(espcpputils::toHexString({appDesc->app_elf_sha256, 8})); }
                }
            }
        }

        {
            HtmlTag formTag{"form", "action=\"/triggerOta\" method=\"GET\"", body};
            HtmlTag fieldsetTag{"fieldset", body};
            {
                HtmlTag legendTag{"legend", body};
                body += "Trigger Update";
            }

            body += fmt::format("<input type=\"text\" name=\"url\" value=\"{}\" />", esphttpdutils::htmlentities(configs.otaUrl.value));

            {
                HtmlTag buttonTag{"button", "type=\"submit\"", body};
                body += "Go";
            }

            body += "url is only used temporarely and not persisted in flash";
        }
    }

    CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::Ok, "text/html", body)
}

esp_err_t webserver_trigger_ota_handler(httpd_req_t *req)
{
    std::string query;
    if (auto result = esphttpdutils::webserver_get_query(req))
        query = *result;
    else
    {
        ESP_LOGE(TAG, "%.*s", result.error().size(), result.error().data());
        CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::BadRequest, "text/plain", result.error());
    }

    std::string url;
    constexpr const std::string_view urlParamName{"url"};

    {
        char valueBufEncoded[256];
        if (const auto result = httpd_query_key_value(query.data(), urlParamName.data(), valueBufEncoded, 256); result != ESP_OK)
        {
            if (result == ESP_ERR_NOT_FOUND)
            {
                const auto msg = fmt::format("{} not set", urlParamName);
                ESP_LOGW(TAG, "%.*s", msg.size(), msg.data());
                CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::BadRequest, "text/plain", msg);
            }
            else
            {
                const auto msg = fmt::format("httpd_query_key_value() {} failed with {}", urlParamName, esp_err_to_name(result));
                ESP_LOGE(TAG, "%.*s", msg.size(), msg.data());
                CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::BadRequest, "text/plain", msg);
            }
        }

        char valueBuf[257];
        esphttpdutils::urldecode(valueBuf, valueBufEncoded);

        url = valueBuf;
    }

    if (const auto result = otaClientTrigger(url); !result)
    {
        ESP_LOGE(TAG, "%.*s", result.error().size(), result.error().data());
        CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::BadRequest, "text/plain", result.error());
    }

    CALL_AND_EXIT_ON_ERROR(httpd_resp_set_hdr, req, "Location", "/ota")
    CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::TemporaryRedirect, "text/html", "Ok, continue at <a href=\"/ota\">/</a>")
}

template<class T>
struct is_duration : std::false_type {};

template<class Rep, class Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

template <typename _Tp>
inline constexpr bool is_duration_v = is_duration<_Tp>::value;

template<typename T>
typename std::enable_if<
    !std::is_same_v<T, bool> &&
    !std::is_integral_v<T> &&
    !is_duration_v<T> &&
    !std::is_same_v<T, std::string> &&
    !std::is_same_v<T, wifi_stack::ip_address_t> &&
    !std::is_same_v<T, wifi_stack::mac_t> &&
    !std::is_same_v<T, std::optional<wifi_stack::mac_t>> &&
    !std::is_same_v<T, wifi_auth_mode_t> &&
    !std::is_same_v<T, sntp_sync_mode_t> &&
    !std::is_same_v<T, espchrono::DayLightSavingMode>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    HtmlTag spanTag{"span", "style=\"color: red;\"", body};
    body += "Unsupported config type";
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, bool>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    body += fmt::format("<input type=\"checkbox\" name=\"{}\" value=\"true\" {}/>"
                        "<input type=\"hidden\" name=\"{}\" value=\"false\" />",
                        esphttpdutils::htmlentities(key),
                        value ? "checked " : "",
                        esphttpdutils::htmlentities(key));
}

template<typename T>
typename std::enable_if<
    !std::is_same_v<T, bool> &&
    std::is_integral_v<T>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    body += fmt::format("<input type=\"number\" name=\"{}\" value=\"{}\" min=\"{}\" max=\"{}\" step=\"1\" />",
                        esphttpdutils::htmlentities(key),
                        value,
                        std::numeric_limits<T>::min(),
                        std::numeric_limits<T>::max());
}

template<typename T>
typename std::enable_if<
    is_duration_v<T>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    body += fmt::format("<input type=\"number\" name=\"{}\" value=\"{}\" step=\"1\" />",
                        esphttpdutils::htmlentities(key),
                        value.count());
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, std::string>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    body += fmt::format("<input type=\"text\" name=\"{}\" value=\"{}\" />",
                        esphttpdutils::htmlentities(key),
                        esphttpdutils::htmlentities(value));
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, wifi_stack::ip_address_t>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    body += fmt::format("<input type=\"text\" name=\"{}\" value=\"{}\" pattern=\"[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+\" />",
                        esphttpdutils::htmlentities(key),
                        esphttpdutils::htmlentities(wifi_stack::toString(value)));
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, wifi_stack::mac_t>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    body += fmt::format("<input type=\"text\" name=\"{}\" value=\"{}\" pattern=\"[0-9a-fA-F]{{2}}(?:\\:[0-9a-fA-F]{{2}}){{5}}\" />",
                        esphttpdutils::htmlentities(key),
                        esphttpdutils::htmlentities(wifi_stack::toString(value)));
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, std::optional<wifi_stack::mac_t>>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    body += fmt::format("<input type=\"text\" name=\"{}\" value=\"{}\" pattern=\"(?:[0-9a-fA-F]{{2}}(?:\\:[0-9a-fA-F]{{2}}){{5}})?\" /> ?",
                        esphttpdutils::htmlentities(key),
                        value ? esphttpdutils::htmlentities(wifi_stack::toString(*value)) : std::string{});
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, wifi_auth_mode_t>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    HtmlTag select{"select", fmt::format("name=\"{}\"", esphttpdutils::htmlentities(key)), body};

#define HANDLE_ENUM_KEY(x) \
    { \
            HtmlTag option{"option", fmt::format("value=\"{}\"{}", std::to_underlying(x), value == x ? " selected" : ""), body}; \
            body += esphttpdutils::htmlentities(#x); \
    }
    HANDLE_ENUM_KEY(WIFI_AUTH_OPEN)
    HANDLE_ENUM_KEY(WIFI_AUTH_WEP)
    HANDLE_ENUM_KEY(WIFI_AUTH_WPA_PSK)
    HANDLE_ENUM_KEY(WIFI_AUTH_WPA2_PSK)
    HANDLE_ENUM_KEY(WIFI_AUTH_WPA_WPA2_PSK)
    HANDLE_ENUM_KEY(WIFI_AUTH_WPA2_ENTERPRISE)
    HANDLE_ENUM_KEY(WIFI_AUTH_WPA3_PSK)
    HANDLE_ENUM_KEY(WIFI_AUTH_WPA2_WPA3_PSK)
    HANDLE_ENUM_KEY(WIFI_AUTH_WAPI_PSK)
    HANDLE_ENUM_KEY(WIFI_AUTH_MAX)
#undef HANDLE_ENUM_KEY
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, sntp_sync_mode_t>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    HtmlTag select{"select", fmt::format("name=\"{}\"", esphttpdutils::htmlentities(key)), body};

#define HANDLE_ENUM_KEY(x) \
    { \
            HtmlTag option{"option", fmt::format("value=\"{}\"{}", std::to_underlying(x), value == x ? " selected" : ""), body}; \
            body += esphttpdutils::htmlentities(#x); \
    }
    HANDLE_ENUM_KEY(SNTP_SYNC_MODE_IMMED)
    HANDLE_ENUM_KEY(SNTP_SYNC_MODE_SMOOTH)
#undef HANDLE_ENUM_KEY
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, espchrono::DayLightSavingMode>
, void>::type
showInputForSetting(std::string_view key, T value, std::string &body)
{
    HtmlTag select{"select", fmt::format("name=\"{}\"", esphttpdutils::htmlentities(key)), body};

    espchrono::iterateDayLightSavingMode([&](T enumVal, std::string_view enumKey){
        HtmlTag option{"option", fmt::format("value=\"{}\"{}", std::to_underlying(enumVal), value == enumVal ? " selected" : ""), body};
        body += esphttpdutils::htmlentities(enumKey);
    });
}

esp_err_t webserver_settings_handler(httpd_req_t *req)
{
    std::string body;

    {
        HtmlTag htmlTag{"html", body};

        {
            HtmlTag headTag{"head", body};

            {
                HtmlTag titleTag{"title", body};
                body += "Settings";
            }

            body += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\" />";

            HtmlTag styleTag{"style", "type=\"text/css\"", body};
            body +=
                ".form-table {"
                "display: table;"
                "border-collapse: separate;"
                "border-spacing: 10px 0;"
                "}"

                ".form-table .form-table-row {"
                "display: table-row;"
                "}"

                ".form-table .form-table-row .form-table-cell {"
                "display: table-cell;"
                "}";
        }

        {
            HtmlTag bodyTag{"body", body};

            {
                HtmlTag h1Tag{"h1", body};
                body += "Settings";
            }

            {
                HtmlTag pTag{"p", body};
                body += "<b>Settings</b> - "
                        "<a href=\"/ota\">Update</a>";
            }

            HtmlTag divTag{"div", "class=\"form-table\"", body};

            configs.callForEveryConfig([&](const auto &config){
                if (body.size() > 2048)
                {
                    if (const auto result = httpd_resp_send_chunk(req, body.data(), body.size()); result != ESP_OK)
                    {
                        ESP_LOGE(TAG, "httpd_resp_send_chunk() failed with %s", esp_err_to_name(result));
                        //return result;
                    }
                    body.clear();
                }

                const std::string_view nvsName{config.nvsName()};

                HtmlTag formTag{"form", "class=\"form-table-row\" action=\"/saveSettings\" method=\"GET\"", body};

                {
                    HtmlTag divTag{"div", "class=\"form-table\"", body};
                    HtmlTag bTag{"b", body};
                    body += esphttpdutils::htmlentities(nvsName);
                }

                {
                    HtmlTag divTag{"div", "class=\"form-table-cell\"", body};
                    showInputForSetting(nvsName, config.value, body);
                }

                {
                    HtmlTag divTag{"div", "class=\"form-table-cell\"", body};
                    HtmlTag buttonTag{"button", "type=\"submit\"", body};
                    body += "Save";
                }

                {
                    HtmlTag divTag{"div", "class=\"form-table-cell\"", body};

                    if (config.touched())
                    {
                        {
                            HtmlTag buttonTag{"span", "style=\"color: red;\"", body};
                            body += "Touched";
                        }

                        body += ' ';

                        if (config.allowReset())
                        {
                            HtmlTag buttonTag{"a", fmt::format("href=\"/resetSettings?{}=1\"", esphttpdutils::htmlentities(nvsName)), body};
                            body += "Reset";
                        }
                        else
                        {
                            HtmlTag buttonTag{"span", "style=\"color: yellow;\"", body};
                            body += "No Reset";
                        }
                    }
                }

                return false; // dont abort loop
            });
        }
    }

    CALL_AND_EXIT_ON_ERROR(httpd_resp_send_chunk, req, body.data(), body.size());
    body.clear();
    CALL_AND_EXIT(httpd_resp_send_chunk, req, nullptr, 0);
    //CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::Ok, "text/html", body)
}

template<typename T>
typename std::enable_if<
    !std::is_same_v<T, bool> &&
    !std::is_integral_v<T> &&
    !std::is_same_v<T, std::string> &&
    !std::is_same_v<T, wifi_stack::ip_address_t> &&
    !std::is_same_v<T, wifi_stack::mac_t> &&
    !std::is_same_v<T, std::optional<wifi_stack::mac_t>> &&
    !std::is_same_v<T, wifi_auth_mode_t> &&
    !std::is_same_v<T, sntp_sync_mode_t> &&
    !std::is_same_v<T, espchrono::DayLightSavingMode>
, tl::expected<void, std::string>>::type
saveSetting(ConfigWrapper<T> &config, std::string_view newValue)
{
    return tl::make_unexpected("Unsupported config type");
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, bool>
, tl::expected<void, std::string>>::type
saveSetting(ConfigWrapper<T> &config, std::string_view newValue)
{
    if (cpputils::is_in(newValue, "true", "false"))
        return configs.write_config(config, newValue == "true");
    else
        return tl::make_unexpected(fmt::format("only true and false allowed, not {}", newValue));
}

template<typename T>
typename std::enable_if<
    !std::is_same_v<T, bool> &&
    std::is_integral_v<T>
, tl::expected<void, std::string>>::type
saveSetting(ConfigWrapper<T> &config, std::string_view newValue)
{
    if (auto parsed = cpputils::fromString<T>(newValue))
        return configs.write_config(config, *parsed);
    else
        return tl::make_unexpected(fmt::format("could not parse {}", newValue));
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, std::string>
, tl::expected<void, std::string>>::type
saveSetting(ConfigWrapper<T> &config, std::string_view newValue)
{
    return configs.write_config(config, std::string{newValue});
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, wifi_stack::ip_address_t>
, tl::expected<void, std::string>>::type
saveSetting(ConfigWrapper<T> &config, std::string_view newValue)
{
    if (const auto parsed = wifi_stack::fromString<wifi_stack::ip_address_t>(newValue); parsed)
        return configs.write_config(config, *parsed);
    else
        return tl::make_unexpected(parsed.error());
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, wifi_stack::mac_t>
, tl::expected<void, std::string>>::type
saveSetting(ConfigWrapper<T> &config, std::string_view newValue)
{
    if (const auto parsed = wifi_stack::fromString<wifi_stack::mac_t>(newValue); parsed)
        return configs.write_config(config, *parsed);
    else
        return tl::make_unexpected(parsed.error());
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, std::optional<wifi_stack::mac_t>>
, tl::expected<void, std::string>>::type
saveSetting(ConfigWrapper<T> &config, std::string_view newValue)
{
    if (newValue.empty())
        return configs.write_config(config, std::nullopt);
    else if (const auto parsed = wifi_stack::fromString<wifi_stack::mac_t>(newValue); parsed)
        return configs.write_config(config, *parsed);
    else
        return tl::make_unexpected(parsed.error());
}

template<typename T>
typename std::enable_if<
    std::is_same_v<T, wifi_auth_mode_t> ||
    std::is_same_v<T, sntp_sync_mode_t> ||
    std::is_same_v<T, espchrono::DayLightSavingMode>
, tl::expected<void, std::string>>::type
saveSetting(ConfigWrapper<T> &config, std::string_view newValue)
{
    if (auto parsed = cpputils::fromString<std::underlying_type_t<T>>(newValue))
        return configs.write_config(config, T(*parsed));
    else
        return tl::make_unexpected(fmt::format("could not parse {}", newValue));
}

esp_err_t webserver_saveSettings_handler(httpd_req_t *req)
{
    std::string query;
    if (auto result = esphttpdutils::webserver_get_query(req))
        query = *result;
    else
    {
        ESP_LOGE(TAG, "%.*s", result.error().size(), result.error().data());
        CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::BadRequest, "text/plain", result.error());
    }

    std::string body;
    bool success{true};

    configs.callForEveryConfig([&](auto &config){
        const std::string_view nvsName{config.nvsName()};

        char valueBufEncoded[256];
        if (const auto result = httpd_query_key_value(query.data(), nvsName.data(), valueBufEncoded, 256); result != ESP_OK)
        {
            if (result != ESP_ERR_NOT_FOUND)
            {
                const auto msg = fmt::format("{}: httpd_query_key_value() failed with {}", nvsName, esp_err_to_name(result));
                ESP_LOGE(TAG, "%.*s", msg.size(), msg.data());
                body += msg;
                body += '\n';
                success = false;
            }
            return false; // dont abort loop
        }

        char valueBuf[257];
        esphttpdutils::urldecode(valueBuf, valueBufEncoded);

        if (const auto result = saveSetting(config, valueBuf); result)
            body += fmt::format("{} succeeded!\n", esphttpdutils::htmlentities(nvsName));
        else
        {
            body += fmt::format("{} failed: {}\n", esphttpdutils::htmlentities(nvsName), esphttpdutils::htmlentities(result.error()));
            success = false;
        }

        return false; // dont abort loop
    });

    if (body.empty())
        CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::Ok, "text/plain", "nothing changed?!")

    if (success)
    {
        CALL_AND_EXIT_ON_ERROR(httpd_resp_set_hdr, req, "Location", "/")
        body += "\nOk, continue at /";
    }

    CALL_AND_EXIT(esphttpdutils::webserver_resp_send,
                  req,
                  success ? esphttpdutils::ResponseStatus::TemporaryRedirect : esphttpdutils::ResponseStatus::BadRequest,
                  "text/plain",
                  body)
}

esp_err_t webserver_resetSettings_handler(httpd_req_t *req)
{
    std::string query;
    if (auto result = esphttpdutils::webserver_get_query(req))
        query = *result;
    else
    {
        ESP_LOGE(TAG, "%.*s", result.error().size(), result.error().data());
        CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::BadRequest, "text/plain", result.error());
    }

    std::string body;
    bool success{true};

    configs.callForEveryConfig([&](auto &config){
        const std::string_view nvsName{config.nvsName()};

        char valueBufEncoded[256];
        if (const auto result = httpd_query_key_value(query.data(), nvsName.data(), valueBufEncoded, 256); result != ESP_OK)
        {
            if (result != ESP_ERR_NOT_FOUND)
            {
                const auto msg = fmt::format("{}: httpd_query_key_value() failed with {}", nvsName, esp_err_to_name(result));
                ESP_LOGE(TAG, "%.*s", msg.size(), msg.data());
                body += msg;
                body += '\n';
                success = false;
            }
            return false; // dont abort loop
        }

        body += nvsName;
        body += ' ';

        if (const auto result = configs.reset_config(config); result)
            body += "reset successful";
        else
        {
            body += result.error();
            success = false;
        }

        body += '\n';

        return false; // dont abort loop
    });

    if (body.empty())
        CALL_AND_EXIT(esphttpdutils::webserver_resp_send, req, esphttpdutils::ResponseStatus::Ok, "text/plain", "nothing changed?!")

    if (success)
    {
        CALL_AND_EXIT_ON_ERROR(httpd_resp_set_hdr, req, "Location", "/")
        body += "\nOk, continue at /";
    }

    CALL_AND_EXIT(esphttpdutils::webserver_resp_send,
                  req,
                  success ? esphttpdutils::ResponseStatus::TemporaryRedirect : esphttpdutils::ResponseStatus::BadRequest,
                  "text/plain",
                  body)
}
} // namespace
