#pragma once

// system includes
#include <string>
#include <string_view>
#include <map>

// 3rdparty lib includes
#include <tl/expected.hpp>

// forward declares
class EspAsyncOta;

extern EspAsyncOta &otaClient;

void ota_client_init();
void ota_client_update();

tl::expected<void, std::string> otaClientTrigger(std::string_view url);
tl::expected<void, std::string> otaClientAbort();
