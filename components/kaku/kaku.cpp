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

  // Search through buffer for sync pulse: 1T high, ~10.44T low
  // In real RF receivers, 1T high is ~150-500us, and 10.44T low is ~1800-4000us.
  bool found_sync = false;
  uint32_t period = 260;

  while (src.size() - src.get_index() >= 130) {
    int32_t high = src.peek(0);
    int32_t low = src.peek(1);

    // High pulse followed by long low gap
    if (high >= 100 && high <= 600 && low <= -1500 && low >= -4500) {
      uint32_t low_abs = -low;
      if (low_abs >= (uint32_t)high * 5 && low_abs <= (uint32_t)high * 20) {
        period = low_abs / 10;
        src.advance(2);
        found_sync = true;
        break;
      }
    }
    src.advance(1);
  }

  if (!found_sync)
    return {};

  // Dynamic thresholds based on measured sync pulse period (Randy Simons algorithm)
  uint32_t max_1t = std::max<uint32_t>(period * 3, 600);
  uint32_t min_5t = period * 2;
  uint32_t max_5t = period * 8;

  auto read_kaku_bit = [&](bool &bit_val, bool &is_dim) -> bool {
    if (src.size() - src.get_index() < 4)
      return false;

    int32_t h1 = src.peek(0);
    int32_t l1 = -src.peek(1);
    int32_t h2 = src.peek(2);
    int32_t l2 = -src.peek(3);

    if (h1 <= 0 || l1 <= 0 || h2 <= 0 || l2 <= 0)
      return false;
    if ((uint32_t)h1 > max_1t || (uint32_t)h2 > max_1t)
      return false;

    bool l1_is_5t = ((uint32_t)l1 >= min_5t && (uint32_t)l1 <= max_5t);
    bool l1_is_1t = ((uint32_t)l1 <= max_1t);
    bool l2_is_5t = ((uint32_t)l2 >= min_5t && (uint32_t)l2 <= max_5t);
    bool l2_is_1t = ((uint32_t)l2 <= max_1t);

    src.advance(4);

    if (l1_is_5t && l2_is_1t) {
      // Bit '1': T, 5T, T, T
      bit_val = true;
      is_dim = false;
      return true;
    } else if (l1_is_1t && l2_is_5t) {
      // Bit '0': T, T, T, 5T
      bit_val = false;
      is_dim = false;
      return true;
    } else if (l1_is_1t && l2_is_1t) {
      // Bit 'dim': T, T, T, T
      bit_val = false;
      is_dim = true;
      return true;
    }

    return false;
  };

  // 26 Address bits
  for (uint8_t i = 0; i < 26; i++) {
    bool bit = false, is_dim = false;
    if (!read_kaku_bit(bit, is_dim) || is_dim)
      return {};
    out.address = (out.address << 1) | (bit ? 1 : 0);
  }

  // 1 Group bit
  {
    bool bit = false, is_dim = false;
    if (!read_kaku_bit(bit, is_dim) || is_dim)
      return {};
    out.group = bit;
  }

  // 1 State / Dim bit
  {
    bool bit = false, is_dim = false;
    if (!read_kaku_bit(bit, is_dim))
      return {};
    if (is_dim) {
      out.state = true;
      out.dim_level = 1;  // Marker for dim presence
    } else {
      out.state = bit;
    }
  }

  // 4 Unit bits
  for (uint8_t i = 0; i < 4; i++) {
    bool bit = false, is_dim = false;
    if (!read_kaku_bit(bit, is_dim) || is_dim)
      return {};
    out.unit = (out.unit << 1) | (bit ? 1 : 0);
  }

  // If dim signal, decode 4 dim level bits
  if (out.dim_level == 1) {
    out.dim_level = 0;
    for (uint8_t i = 0; i < 4; i++) {
      bool bit = false, is_dim = false;
      if (!read_kaku_bit(bit, is_dim) || is_dim)
        break;
      out.dim_level = (out.dim_level << 1) | (bit ? 1 : 0);
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
