#include "espnow_sender.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/util.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace espnow_sender {

static const char *const TAG = "espnow_sender";

static void send_callback(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    const uint8_t *addr = tx_info->des_addr;
    ESP_LOGW(TAG, "ESP-NOW send failed to %02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  }
}

void ESPNowSenderComponent::setup() {
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

  esp_now_register_send_cb(send_callback);

  ESP_LOGI(TAG, "ESP-NOW sender ready, peer %02X:%02X:%02X:%02X:%02X:%02X",
           peer_mac_[0], peer_mac_[1], peer_mac_[2],
           peer_mac_[3], peer_mac_[4], peer_mac_[5]);
}

void ESPNowSenderComponent::set_binary_sensor(binary_sensor::BinarySensor *bs) {
  binary_sensor_ = bs;
  bs->add_on_state_callback([this](bool state) {
    last_sensor_state_ = state;
    if (state_ == FallbackState::FALLBACK) {
      // [Fix 3] For mirror_state send on both edges; for all other actions
      // (toggle/on/off) only send on the rising edge to avoid double-triggers.
      if (configured_action_ == ACTION_MIRROR_STATE || state) {
        this->send_command(state);
      }
    }
  });
}

bool ESPNowSenderComponent::check_connectivity_() {
  // [Feature 1] If an external connectivity sensor is configured, delegate to it.
  if (connectivity_sensor_ != nullptr) {
    return connectivity_sensor_->state;
  }
  bool connected = network::is_connected();
#ifdef USE_API
  if (connected && api::global_api_server != nullptr) {
    connected = api::global_api_server->is_connected();
  }
#endif
  return connected;
}

void ESPNowSenderComponent::loop() {
  bool connected = check_connectivity_();
  uint32_t now = millis();

  switch (state_) {
    case FallbackState::NORMAL:
      if (!connected) {
        state_ = FallbackState::PENDING_FALLBACK;
        state_change_time_ = now;
        ESP_LOGD(TAG, "Connectivity lost, starting fallback timer (%u ms)", fallback_timeout_ms_);
      }
      break;

    case FallbackState::PENDING_FALLBACK:
      if (connected) {
        // Recovered before timeout — go back to normal immediately.
        state_ = FallbackState::NORMAL;
        ESP_LOGD(TAG, "Connectivity restored before fallback, staying in NORMAL");
      } else if (now - state_change_time_ >= fallback_timeout_ms_) {
        state_ = FallbackState::FALLBACK;
        ESP_LOGI(TAG, "Entering ESP-NOW fallback mode");
        this->fallback_enter_callback_.call();  // [Feature 2]
      }
      break;

    case FallbackState::FALLBACK:
      if (connected) {
        state_ = FallbackState::PENDING_RECOVERY;
        state_change_time_ = now;
        ESP_LOGD(TAG, "Connectivity restored, starting recovery timer (%u ms)", recovery_timeout_ms_);
      }
      break;

    case FallbackState::PENDING_RECOVERY:
      if (!connected) {
        // Lost again before recovery — stay in fallback.
        state_ = FallbackState::FALLBACK;
        ESP_LOGD(TAG, "Connectivity lost again, staying in FALLBACK");
      } else if (now - state_change_time_ >= recovery_timeout_ms_) {
        state_ = FallbackState::NORMAL;
        ESP_LOGI(TAG, "Leaving ESP-NOW fallback mode, HA resumed");
        this->fallback_exit_callback_.call();  // [Feature 2]
      }
      break;
  }
}

void ESPNowSenderComponent::send_command(bool sensor_state) {
  if (state_ != FallbackState::FALLBACK) return;

  uint8_t action_byte;
  switch (configured_action_) {
    case ACTION_ON:
      action_byte = ACTION_ON;
      break;
    case ACTION_OFF:
      action_byte = ACTION_OFF;
      break;
    case ACTION_TOGGLE:
      action_byte = ACTION_TOGGLE;
      break;
    case ACTION_MIRROR_STATE:
      action_byte = sensor_state ? ACTION_ON : ACTION_OFF;
      break;
    default:
      action_byte = ACTION_TOGGLE;
      break;
  }

  send_packet_(action_byte);
}

void ESPNowSenderComponent::send_packet_(uint8_t action_byte) {
  ESPNowPacket pkt{ESPNOW_MAGIC, action_byte};
  esp_err_t err = esp_now_send(peer_mac_.data(), reinterpret_cast<const uint8_t *>(&pkt), sizeof(pkt));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "ESP-NOW packet sent: action=0x%02X", action_byte);
  }
}

}  // namespace espnow_sender
}  // namespace esphome
