#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <esp_now.h>

namespace esphome {
namespace espnow_sender {

static const uint8_t ESPNOW_MAGIC = 0xE5;

enum ESPNowSendAction : uint8_t {
  ACTION_OFF = 0x00,
  ACTION_ON = 0x01,
  ACTION_TOGGLE = 0x02,
  ACTION_MIRROR_STATE = 0x03,
};

struct ESPNowPacket {
  uint8_t magic;
  uint8_t action;
};

enum class FallbackState {
  NORMAL,
  PENDING_FALLBACK,   // connectivity lost, waiting for fallback_timeout
  FALLBACK,           // in ESP-NOW mode
  PENDING_RECOVERY,   // connectivity restored, waiting for recovery_timeout
};

class ESPNowSenderComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_peer_mac(std::vector<uint8_t> mac) { peer_mac_ = mac; }
  void set_fallback_timeout(uint32_t ms) { fallback_timeout_ms_ = ms; }
  void set_recovery_timeout(uint32_t ms) { recovery_timeout_ms_ = ms; }
  void set_action(ESPNowSendAction action) { configured_action_ = action; }
  void set_binary_sensor(binary_sensor::BinarySensor *bs);

  /// Call from YAML lambda (e.g. button on_press) to trigger an ESP-NOW send
  /// when in fallback mode. In normal mode this is a no-op (HA handles it).
  void send_command(bool state);

  bool is_fallback_mode() const { return state_ == FallbackState::FALLBACK; }

 protected:
  bool check_connectivity_();
  void send_packet_(uint8_t action_byte);

  std::vector<uint8_t> peer_mac_;
  uint32_t fallback_timeout_ms_{30000};
  uint32_t recovery_timeout_ms_{10000};
  ESPNowSendAction configured_action_{ESPNowSendAction::ACTION_TOGGLE};
  binary_sensor::BinarySensor *binary_sensor_{nullptr};

  FallbackState state_{FallbackState::NORMAL};
  uint32_t state_change_time_{0};

  bool last_sensor_state_{false};
};

}  // namespace espnow_sender
}  // namespace esphome
