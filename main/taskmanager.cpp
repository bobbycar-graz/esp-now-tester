#include "taskmanager.h"

// system includes
#include <iterator>
#include <chrono>

// esp-idf includes
#include <esp_log.h>

// 3rdparty lib includes
#include <schedulertask.h>

// local includes
#include "wifi.h"
#include "debugconsole.h"
#include "ota.h"
#include "webserver.h"

using namespace std::chrono_literals;

namespace {
constexpr const char * const TAG = "TASKS";

void not_needed() {}

espcpputils::SchedulerTask schedulerTasksArr[] {
    espcpputils::SchedulerTask { "wifi",         wifi_begin,        wifi_update,         100ms },
    espcpputils::SchedulerTask { "debugconsole", init_debugconsole, update_debugconsole, 50ms },
    espcpputils::SchedulerTask { "ota_client",   ota_client_init,   ota_client_update,   100ms },
    espcpputils::SchedulerTask { "webserver",    initWebserver,     handleWebserver,     100ms },
};
} // namespace

cpputils::ArrayView<espcpputils::SchedulerTask> schedulerTasks{std::begin(schedulerTasksArr), std::end(schedulerTasksArr)};

void sched_pushStats(bool printTasks)
{
    if (printTasks)
        ESP_LOGI(TAG, "begin listing tasks...");

    for (auto &schedulerTask : schedulerTasks)
        schedulerTask.pushStats(printTasks);

    if (printTasks)
        ESP_LOGI(TAG, "end listing tasks");
}
