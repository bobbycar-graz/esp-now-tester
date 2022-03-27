#include "sdkconfig.h"

// esp-idf includes
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#if defined(CONFIG_ESP_TASK_WDT_PANIC) || defined(CONFIG_ESP_TASK_WDT)
#include <freertos/task.h>
#include <esp_task_wdt.h>
#endif
#ifdef CONFIG_APP_ROLLBACK_ENABLE
#include <esp_ota_ops.h>
#endif
#include <esp_heap_caps.h>

// Arduino includes
#include <esp32-hal-gpio.h>

// 3rdparty lib includes
#include <schedulertask.h>
#include <espchrono.h>
#include <espasyncota.h>
#include <espstrutils.h>
#include <esprandom.h>
#include <strutils.h>

// local includes
#include "config.h"
#include "debugconsole.h"
#include "taskmanager.h"
#include "ota.h"
#include "wifi.h"

using namespace std::chrono_literals;

namespace {
constexpr const char * const TAG = "MAIN";

espchrono::millis_clock::time_point lastLoopCount;
int _loopCount{};
int loopCountTemp{};
} // namespace

const int &loopCount{_loopCount};

extern "C" void app_main()
{
#if defined(CONFIG_ESP_TASK_WDT_PANIC) || defined(CONFIG_ESP_TASK_WDT)
    {
        const auto taskHandle = xTaskGetCurrentTaskHandle();
        if (!taskHandle)
        {
            ESP_LOGE(TAG, "could not get handle to current main task!");
        }
        else if (const auto result = esp_task_wdt_add(taskHandle); result != ESP_OK)
        {
            ESP_LOGE(TAG, "could not add main task to watchdog: %s", esp_err_to_name(result));
        }
    }

    bool wasPreviouslyUpdating{};
#endif

#ifdef CONFIG_APP_ROLLBACK_ENABLE
    esp_ota_img_states_t ota_state;
    if (const esp_partition_t * const running = esp_ota_get_running_partition())
    {
        if (const auto result = esp_ota_get_state_partition(running, &ota_state); result == ESP_ERR_NOT_FOUND)
            ota_state = ESP_OTA_IMG_VALID;
        else if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_get_state_partition() failed with %s", esp_err_to_name(result));
            ota_state = ESP_OTA_IMG_UNDEFINED;
        }
    }
    else
    {
        ESP_LOGE(TAG, "esp_ota_get_running_partition() returned nullptr");
        ota_state = ESP_OTA_IMG_UNDEFINED;
    }
#endif

    //init proper ref tick value for PLL (uncomment if REF_TICK is different than 1MHz)
    //ESP_REG(APB_CTRL_PLL_TICK_CONF_REG) = APB_CLK_FREQ / REF_CLK_FREQ - 1;

    pinMode(3, INPUT_PULLUP);

    if (const auto result = configs.init("dmxnode"); result != ESP_OK)
        ESP_LOGE(TAG, "config_init_settings() failed with %s", esp_err_to_name(result));

    for (const auto &task : schedulerTasks)
        task.setup();

    while (true)
    {
        bool pushStats = espchrono::ago(lastLoopCount) >= 1s;
        if (pushStats)
        {
            lastLoopCount = espchrono::millis_clock::now();
            _loopCount = loopCountTemp;
            loopCountTemp = 0;
        }

        loopCountTemp++;

        for (auto &task : schedulerTasks)
        {
            task.loop();

#if defined(CONFIG_ESP_TASK_WDT_PANIC) || defined(CONFIG_ESP_TASK_WDT)
            if (wasPreviouslyUpdating)
            {
                if (const auto result = esp_task_wdt_reset(); result != ESP_OK)
                    ESP_LOGE(TAG, "esp_task_wdt_reset() failed with %s", esp_err_to_name(result));
                //vPortYield();
                vTaskDelay(1);
            }
#endif
        }

        if (pushStats)
        {
            sched_pushStats(memoryDebug == Fast);
        }

        if constexpr (LOG_LOCAL_LEVEL >= ESP_LOG_WARN)
        {
            const auto free8 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            if (free8 < 25000)
            {
                lastMemoryDebug = espchrono::millis_clock::now();
                ESP_LOGW(TAG, "MEMORY heap8=%zd (largest block: %zd) heap32=%zd lps=%i",
                         free8,
                         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
                         heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_32BIT),
                         _loopCount
                         );
            }
            else if constexpr (LOG_LOCAL_LEVEL >= ESP_LOG_INFO)
            {
                if (memoryDebug != Off &&
                    espchrono::ago(lastMemoryDebug) >= (memoryDebug == Fast ? 100ms : 1000ms))
                {
                    lastMemoryDebug = espchrono::millis_clock::now();
                    ESP_LOGI(TAG, "MEMORY heap8=%zd (largest block: %zd) heap32=%zd lps=%i",
                             free8,
                             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
                             heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_32BIT),
                             _loopCount
                             );
                }
            }
        }

#if defined(CONFIG_ESP_TASK_WDT_PANIC) || defined(CONFIG_ESP_TASK_WDT)
        if (const auto result = esp_task_wdt_reset(); result != ESP_OK)
            ESP_LOGE(TAG, "esp_task_wdt_reset() failed with %s", esp_err_to_name(result));

        if (const auto isUpdating = otaClient.status() == OtaCloudUpdateStatus::Updating; isUpdating != wasPreviouslyUpdating)
        {
            wasPreviouslyUpdating = isUpdating;

            constexpr bool panic =
#ifdef CONFIG_ESP_TASK_WDT_PANIC
                true
#else
                false
#endif
                ;

            const uint32_t timeout = isUpdating ?
                                         CONFIG_ESP_TASK_WDT_TIMEOUT_S * 4 :
                                         CONFIG_ESP_TASK_WDT_TIMEOUT_S;

            const auto result = esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, panic);
            ESP_LOG_LEVEL_LOCAL((result == ESP_OK ? ESP_LOG_INFO : ESP_LOG_ERROR), TAG, "esp_task_wdt_init() with new timeout %u returned: %s", timeout, esp_err_to_name(result));
        }
#endif

#ifdef CONFIG_APP_ROLLBACK_ENABLE
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            const auto result = esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOG_LEVEL_LOCAL((result == ESP_OK ? ESP_LOG_INFO : ESP_LOG_ERROR), TAG, "esp_ota_mark_app_valid_cancel_rollback() returned: %s", esp_err_to_name(result));
            ota_state = ESP_OTA_IMG_VALID;
        }
#endif

        //vPortYield();
        vTaskDelay(1);
    }
}
