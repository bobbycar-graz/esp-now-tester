#pragma once

// esp-idf includes
#include <esp_err.h>

// 3rdparty lib includes
#include <espwifiutils.h>

void wifi_begin();

void wifi_update();

esp_err_t wifi_scan();
