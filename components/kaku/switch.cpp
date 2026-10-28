#include "switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace kaku {

static const char *const TAG = "kaku.switch";

void KakuSwitch::write_state(bool state) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGE(TAG, "No remote transmitter configured!");
    return;
  }

  KakuData data{
      .address = this->address_,
      .unit = this->unit_,
      .group = this->group_,
      .state = state,
      .dim_level = 0,
  };

  this->transmitter_->transmit<KakuProtocol>(data, this->repeats_, 0);
  this->publish_state(state);
}

void KakuSwitch::dump_config() {
  LOG_SWITCH("", "Kaku Switch", this);
  ESP_LOGCONFIG(TAG, "  Address: 0x%07" PRIX32, this->address_);
  ESP_LOGCONFIG(TAG, "  Unit: %u", this->unit_);
  ESP_LOGCONFIG(TAG, "  Group: %s", YESNO(this->group_));
  ESP_LOGCONFIG(TAG, "  Repeats: %u", this->repeats_);
}

}  // namespace kaku
}  // namespace esphome
