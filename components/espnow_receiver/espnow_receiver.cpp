#include "espnow_receiver.h"
#include "esphome/core/log.h"

namespace esphome {
namespace espnow_receiver {

static const char *const TAG = "espnow_receiver";

// Global queue handle used by the ISR-like receive callback.
static QueueHandle_t s_recv_queue = nullptr;

static void recv_callback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
  if (s_recv_queue == nullptr) return;
  if (data_len != sizeof(ESPNowPacket)) return;

  ESPNowPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  if (pkt.magic != ESPNOW_MAGIC) return;

  // Push from WiFi task to main loop via queue; don't block.
  xQueueSendFromISR(s_recv_queue, &pkt, nullptr);
}

void ESPNowReceiverComponent::setup() {
  recv_queue_ = xQueueCreate(8, sizeof(ESPNowPacket));
  if (recv_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create receive queue");
    this->mark_failed();
    return;
  }
  s_recv_queue = recv_queue_;

  esp_err_t err = esp_now_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  esp_now_peer_info_t peer_info = {};
  memcpy(peer_info.peer_addr, peer_mac_.data(), 6);
  peer_info.channel = 0;
  peer_info.encrypt = false;
  err = esp_now_add_peer(&peer_info);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_add_peer failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = esp_now_register_recv_cb(recv_callback);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_register_recv_cb failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "ESP-NOW receiver ready, listening for peer %02X:%02X:%02X:%02X:%02X:%02X",
           peer_mac_[0], peer_mac_[1], peer_mac_[2],
           peer_mac_[3], peer_mac_[4], peer_mac_[5]);
}

void ESPNowReceiverComponent::loop() {
  ESPNowPacket pkt;
  while (xQueueReceive(recv_queue_, &pkt, 0) == pdTRUE) {
    if (light_ == nullptr) continue;

    auto action = static_cast<ESPNowAction>(pkt.action);
    switch (action) {
      case ESPNowAction::ACTION_ON:
        ESP_LOGI(TAG, "ESP-NOW -> light ON");
        light_->turn_on().perform();
        break;
      case ESPNowAction::ACTION_OFF:
        ESP_LOGI(TAG, "ESP-NOW -> light OFF");
        light_->turn_off().perform();
        break;
      case ESPNowAction::ACTION_TOGGLE:
        ESP_LOGI(TAG, "ESP-NOW -> light TOGGLE");
        light_->toggle().perform();
        break;
      default:
        ESP_LOGW(TAG, "Unknown action 0x%02X", pkt.action);
        break;
    }
  }
}

}  // namespace espnow_receiver
}  // namespace esphome
