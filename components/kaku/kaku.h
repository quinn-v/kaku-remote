#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

#include <cinttypes>

namespace esphome {
namespace kaku {

struct KakuData {
  uint32_t address;
  uint8_t unit;
  bool group;
  bool state;
  uint8_t dim_level;

  bool operator==(const KakuData &rhs) const {
    return address == rhs.address && unit == rhs.unit && group == rhs.group &&
           state == rhs.state && dim_level == rhs.dim_level;
  }
};

class KakuProtocol : public remote_base::RemoteProtocol<KakuData> {
public:
  void one(remote_base::RemoteTransmitData *dst) const;
  void zero(remote_base::RemoteTransmitData *dst) const;
  void sync(remote_base::RemoteTransmitData *dst) const;

  void encode(remote_base::RemoteTransmitData *dst, const KakuData &data);
  optional<KakuData> decode(remote_base::RemoteReceiveData src);
  void dump(const KakuData &data);
};

using KakuBinarySensor = remote_base::RemoteReceiverBinarySensor<KakuProtocol>;
using KakuTrigger = remote_base::RemoteReceiverTrigger<KakuProtocol>;
using KakuDumper = remote_base::RemoteReceiverDumper<KakuProtocol>;

template <typename... Ts>
class KakuAction : public remote_base::RemoteTransmitterActionBase<Ts...> {
public:
  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(uint8_t, unit)
  TEMPLATABLE_VALUE(bool, group)
  TEMPLATABLE_VALUE(bool, state)
  TEMPLATABLE_VALUE(uint8_t, dim_level)
  void set_is_dim(bool is_dim) { this->is_dim_ = is_dim; }

  void encode(remote_base::RemoteTransmitData *dst, Ts... x) override {
    KakuData data{};
    data.address = this->address_.value(x...);
    data.unit = this->unit_.value(x...);
    data.group = this->group_.value(x...);
    data.state = this->state_.value(x...);
    data.dim_level = this->is_dim_ ? this->dim_level_.value(x...) : 0;
    KakuProtocol().encode(dst, data);
  }

protected:
  bool is_dim_{false};
};

} // namespace kaku
} // namespace esphome
