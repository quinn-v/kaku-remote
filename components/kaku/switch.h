#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter_component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include "kaku.h"

namespace esphome {
namespace kaku {

class KakuSwitch : public switch_::Switch, public Component {
public:
  void
  set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_address(uint32_t address) { this->address_ = address; }
  void set_unit(uint8_t unit) { this->unit_ = unit; }
  void set_group(bool group) { this->group_ = group; }
  void set_repeats(uint8_t repeats) { this->repeats_ = repeats; }

  void dump_config() override;

protected:
  void write_state(bool state) override;

  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  uint32_t address_{0};
  uint8_t unit_{0};
  bool group_{false};
  uint8_t repeats_{8};
};

} // namespace kaku
} // namespace esphome
