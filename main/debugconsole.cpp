#include "debugconsole.h"

// system includes
#include <string_view>

// esp-idf includes
#include <driver/uart.h>
#include <esp_log.h>

// 3rdparty lib includes
#include <espstrutils.h>

namespace {
constexpr const char * const TAG = "DEBUG";

uint8_t consoleControlCharsReceived{};
bool uart0Initialized{};

void handleNormalChar(char c);
void handleSpecialChar(char c);
} // namespace

MemoryDebug memoryDebug{Off};
espchrono::millis_clock::time_point lastMemoryDebug;

void init_debugconsole()
{
    if (const auto result = uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); result != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_pin() failed with %s", esp_err_to_name(result));
    }

    if (const auto result = uart_driver_install(UART_NUM_0, SOC_UART_FIFO_LEN + 1, 0, 10, nullptr, 0); result != ESP_OK)
        ESP_LOGE(TAG, "uart_driver_install() failed with %s", esp_err_to_name(result));
    else
        uart0Initialized = true;
}

void update_debugconsole()
{
    if (!uart0Initialized)
        return;

    size_t length{};
    if (const auto result = uart_get_buffered_data_len(UART_NUM_0, &length); result != ESP_OK)
    {
        ESP_LOGW(TAG, "uart_get_buffered_data_len() failed with %s", esp_err_to_name(result));
    }
    else if (length)
    {
        char data[length];
        length = uart_read_bytes(UART_NUM_0, data, length, 0);

        for (char c : std::string_view{data, length})
        {
            if (consoleControlCharsReceived < 2)
            {
                switch (c)
                {
                case '\x1b':
                    if (consoleControlCharsReceived == 0)
                        consoleControlCharsReceived = 1;
                    else
                        consoleControlCharsReceived = 0;
                    break;
                case '\x5b':
                    if (consoleControlCharsReceived == 1)
                        consoleControlCharsReceived = 2;
                    else
                        consoleControlCharsReceived = 0;
                    break;
                default:
                    consoleControlCharsReceived = 0;
                    handleNormalChar(c);
                }
            }
            else
            {
                consoleControlCharsReceived = 0;
                handleSpecialChar(c);
            }
        }
    }
}

namespace {
void handleNormalChar(char c)
{
    constexpr auto rotateLogLevel = [](const char *tag){
        const auto oldLogLevel = esp_log_level_get(tag);
        const auto newLogLevel = oldLogLevel == ESP_LOG_INFO ? ESP_LOG_DEBUG : (oldLogLevel == ESP_LOG_DEBUG ? ESP_LOG_VERBOSE : ESP_LOG_INFO);
        esp_log_level_set(tag, newLogLevel);
        ESP_LOGI(TAG, "%s loglevel set to %s (previous=%s)", tag, espcpputils::toString(newLogLevel).c_str(), espcpputils::toString(oldLogLevel).c_str());
    };

    switch (c)
    {
    case 'r': case 'R':
        ESP_LOGI(TAG, "Rebooting...");
        esp_restart();
        break;
    case 'm': case 'M':
        switch (memoryDebug)
        {
        case Off:
            memoryDebug = Normal;
            ESP_LOGI(TAG, "memory debug set to %s", "Normal");
            break;
        case Normal:
            memoryDebug = Fast;
            ESP_LOGI(TAG, "memory debug set to %s", "Fast");
            break;
        case Fast:
            memoryDebug = Off;
            ESP_LOGI(TAG, "memory debug set to %s", "Off");
            break;
        }
        break;
    case 'w': case 'W':
        rotateLogLevel("WEBSERVER");
        break;
    }
}

void handleSpecialChar(char c)
{
    switch (c)
    {
    case 'A': // Up arrow pressed
        break;
    case 'B': // Down arrow pressed
        break;
    case 'C': // Right arrow pressed
        break;
    case 'D': // Left arrow pressed
        break;
    default:
        ESP_LOGI(TAG, "unknown control char received: %hhx", c);
    }
}
} // namespace
