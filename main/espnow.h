#pragma once

// system includes
#include <string>
#include <vector>

// 3rdparty lib includes
#include <esp_now.h>

void initEspNow();
void deinitEspNow();
void handleEspNow();

constexpr const uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

esp_err_t sendEspNow(std::string data);
esp_err_t sendEspNow(std::string data, uint8_t *destination);
esp_err_t sendEspNow(uint8_t *data, size_t size);
esp_err_t sendEspNow(uint8_t *data, size_t size, uint8_t *destination);

namespace espnow {
bool initAllowed();
esp_err_t _sendEspNowImpl(uint8_t *data, size_t size, const uint8_t *destination);
extern std::vector<esp_now_peer_info_t> peers;
} // namespace espnow
