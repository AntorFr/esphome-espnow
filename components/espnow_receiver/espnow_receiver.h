#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_now.h>

namespace esphome {
namespace espnow_receiver {

static const uint8_t ESPNOW_MAGIC = 0xE5;

enum class ESPNowAction : uint8_t {
  ACTION_OFF = 0x00,
  ACTION_ON = 0x01,
  ACTION_TOGGLE = 0x02,
};

struct ESPNowPacket {
  uint8_t magic;
  uint8_t action;
};

class ESPNowReceiverComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_peer_mac(std::vector<uint8_t> mac) { peer_mac_ = mac; }
  void set_light(light::LightState *light) { light_ = light; }

  /// Returns true if src matches the configured peer MAC.
  bool matches_peer(const uint8_t *src) const {
    return peer_mac_.size() == 6 && memcmp(peer_mac_.data(), src, 6) == 0;
  }

  QueueHandle_t recv_queue_{nullptr};

 protected:
  std::vector<uint8_t> peer_mac_;
  light::LightState *light_{nullptr};
};

}  // namespace espnow_receiver
}  // namespace esphome
