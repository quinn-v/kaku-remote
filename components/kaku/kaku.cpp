#include "kaku.h"
#include "esphome/core/log.h"

namespace esphome {
namespace kaku {

static const char *const TAG = "kaku.protocol";

static constexpr uint32_t TX_T_US = 260;
static constexpr uint32_t TX_HEADER_HIGH_US = TX_T_US;
static constexpr uint32_t TX_HEADER_LOW_US = TX_T_US * 10 + (TX_T_US / 2);  // ~2730us (10.5T)
static constexpr uint32_t TX_STOP_HIGH_US = TX_T_US;
static constexpr uint32_t TX_STOP_LOW_US = TX_T_US * 40;  // 10400us

static constexpr uint32_t RX_HEADER_HIGH_US = 300;
static constexpr uint32_t RX_HEADER_LOW_US = 2600;
static constexpr uint32_t RX_BIT_HIGH_US = 300;
static constexpr uint32_t RX_BIT_ONE_LOW_US = 1200;  // ~5T
static constexpr uint32_t RX_BIT_ZERO_LOW_US = 260;  // ~1T

void KakuProtocol::one(remote_base::RemoteTransmitData *dst) const {
  // '1' => 1T high, 5T low, 1T high, 1T low
  dst->item(TX_T_US, TX_T_US * 5);
  dst->item(TX_T_US, TX_T_US * 1);
}

void KakuProtocol::zero(remote_base::RemoteTransmitData *dst) const {
  // '0' => 1T high, 1T low, 1T high, 5T low
  dst->item(TX_T_US, TX_T_US * 1);
  dst->item(TX_T_US, TX_T_US * 5);
}

void KakuProtocol::sync(remote_base::RemoteTransmitData *dst) const {
  dst->item(TX_HEADER_HIGH_US, TX_HEADER_LOW_US);
}

void KakuProtocol::encode(remote_base::RemoteTransmitData *dst, const KakuData &data) {
  dst->set_carrier_frequency(0);

  // Sync / start pulse
  this->sync(dst);

  // 26-bit address (MSB first)
  for (int16_t i = 25; i >= 0; i--) {
    if ((data.address >> i) & 1) {
      this->one(dst);
    } else {
      this->zero(dst);
    }
  }

  // 1-bit group flag
  if (data.group) {
    this->one(dst);
  } else {
    this->zero(dst);
  }

  // 1-bit state (or special dim pulse)
  if (data.dim_level > 0) {
    // Dim pulse: 1T high, 1T low, 1T high, 1T low
    dst->item(TX_T_US, TX_T_US);
    dst->item(TX_T_US, TX_T_US);
  } else if (data.state) {
    this->one(dst);
  } else {
    this->zero(dst);
  }

  // 4-bit unit (MSB first)
  for (int16_t i = 3; i >= 0; i--) {
    if ((data.unit >> i) & 1) {
      this->one(dst);
    } else {
      this->zero(dst);
    }
  }

  // Optional 4-bit dim level if dimming
  if (data.dim_level > 0) {
    for (int16_t i = 3; i >= 0; i--) {
      if ((data.dim_level >> i) & 1) {
        this->one(dst);
      } else {
        this->zero(dst);
      }
    }
  }

  // Stop bit / inter-frame gap
  dst->item(TX_STOP_HIGH_US, TX_STOP_LOW_US);
}

optional<KakuData> KakuProtocol::decode(remote_base::RemoteReceiveData src) {
  KakuData out{
      .address = 0,
      .unit = 0,
      .group = false,
      .state = false,
      .dim_level = 0,
  };

  // Require sync pulse
  if (!src.expect_pulse_with_gap(RX_HEADER_HIGH_US, RX_HEADER_LOW_US))
    return {};

  // Decode 26 address bits
  for (uint8_t i = 0; i < 26; i++) {
    out.address <<= 1UL;
    if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US) &&
        src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US)) {
      out.address |= 0x01;
    } else if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US) &&
               src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US)) {
      out.address |= 0x00;
    } else {
      return {};
    }
  }

  // 1-bit group flag
  if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US) &&
      src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US)) {
    out.group = true;
  } else if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US) &&
             src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US)) {
    out.group = false;
  } else {
    return {};
  }

  // 1-bit state / dim indicator
  if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US)) {
    if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US)) {
      out.state = true;
    } else {
      return {};
    }
  } else if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US)) {
    if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US)) {
      out.state = false;
    } else if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US)) {
      // Dimming mode: 00 pulse pattern
      out.state = true;
      out.dim_level = 1;  // Marker to decode 4 dim bits below
    } else {
      return {};
    }
  } else {
    return {};
  }

  // 4-bit unit
  for (uint8_t i = 0; i < 4; i++) {
    out.unit <<= 1;
    if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US) &&
        src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US)) {
      out.unit |= 0x01;
    } else if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US) &&
               src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US)) {
      out.unit |= 0x00;
    } else {
      return {};
    }
  }

  // If dimming mode, decode 4 dim bits
  if (out.dim_level == 1 && int32_t(src.get_index() + 8) <= src.size()) {
    out.dim_level = 0;
    for (uint8_t i = 0; i < 4; i++) {
      out.dim_level <<= 1;
      if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US) &&
          src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US)) {
        out.dim_level |= 0x01;
      } else if (src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ZERO_LOW_US) &&
                 src.expect_pulse_with_gap(RX_BIT_HIGH_US, RX_BIT_ONE_LOW_US)) {
        out.dim_level |= 0x00;
      } else {
        break;
      }
    }
  }

  return out;
}

void KakuProtocol::dump(const KakuData &data) {
  ESP_LOGI(TAG, "Received KAKU: address=0x%07" PRIX32 " unit=%u group=%s state=%s dim_level=%u",
           data.address, data.unit, data.group ? "true" : "false", data.state ? "ON" : "OFF", data.dim_level);
}

}  // namespace kaku
}  // namespace esphome
