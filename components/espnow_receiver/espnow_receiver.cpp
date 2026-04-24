#include "espnow_receiver.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace espnow_receiver {

static const char *const TAG = "espnow_receiver";

// ── File-scope state (shared across all instances) ───────────────────────────

// All registered receiver instances — populated in setup(), iterated in callback.
static std::vector<ESPNowReceiverComponent *> s_instances;

// Guard: esp_now_init() and esp_now_register_recv_cb() must be called once.
static bool s_initialized = false;

// ── Receive callback (runs in WiFi task) ─────────────────────────────────────

static void recv_callback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
  if (data_len != static_cast<int>(sizeof(ESPNowPacket))) return;

  ESPNowPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));
  if (pkt.magic != ESPNOW_MAGIC) return;

  // Route packet to the instance whose peer_mac matches the sender address.
  for (auto *inst : s_instances) {
    if (inst->matches_peer(recv_info->src_addr)) {
      xQueueSendFromISR(inst->recv_queue_, &pkt, nullptr);
      return;  // each packet goes to at most one instance
    }
  }
}

// ── Component setup ──────────────────────────────────────────────────────────

void ESPNowReceiverComponent::setup() {
  // Each instance owns its own queue.
  recv_queue_ = xQueueCreate(8, sizeof(ESPNowPacket));
  if (recv_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create receive queue");
    this->mark_failed();
    return;
  }

  // One-time global init.
  if (!s_initialized) {
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
    err = esp_now_register_recv_cb(recv_callback);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_now_register_recv_cb failed: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
    s_initialized = true;
    ESP_LOGD(TAG, "ESP-NOW initialized");
  }

  // Per-instance peer registration.
  esp_now_peer_info_t peer_info = {};
  memcpy(peer_info.peer_addr, peer_mac_.data(), 6);
  peer_info.channel = 0;
  peer_info.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer_info);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_add_peer failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // Register this instance for dispatch.
  s_instances.push_back(this);

  ESP_LOGI(TAG, "ESP-NOW receiver ready (#%d), peer %02X:%02X:%02X:%02X:%02X:%02X",
           (int) s_instances.size(),
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
