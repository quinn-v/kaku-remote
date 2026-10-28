#include "kaku.h"

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/log.h"

using namespace esphome::remote_base;

namespace esphome::kaku
{

static const char *const TAG = "kaku.protocol";

static constexpr uint32_t TX_T_US = 260;
static constexpr uint32_t TX_HEADER_HIGH_US = TX_T_US;
static constexpr uint32_t TX_HEADER_LOW_US = TX_T_US * 10 + (TX_T_US / 2); // ~2730us (10.5T)
static constexpr uint32_t TX_STOP_HIGH_US = TX_T_US;
static constexpr uint32_t TX_STOP_LOW_US = TX_T_US * 40; // 10400us

void
KakuProtocol::one(RemoteTransmitData *dst) const
{
    // '1' => 1T high, 5T low, 1T high, 1T low
    dst->item(TX_T_US, TX_T_US * 5);
    dst->item(TX_T_US, TX_T_US * 1);
}

void
KakuProtocol::zero(RemoteTransmitData *dst) const
{
    // '0' => 1T high, 1T low, 1T high, 5T low
    dst->item(TX_T_US, TX_T_US * 1);
    dst->item(TX_T_US, TX_T_US * 5);
}

void
KakuProtocol::sync(RemoteTransmitData *dst) const
{
    dst->item(TX_HEADER_HIGH_US, TX_HEADER_LOW_US);
}

void
KakuProtocol::encode_bits(RemoteTransmitData *dst, uint32_t data, uint8_t num_bits) const
{
    for (int8_t i = num_bits - 1; i >= 0; --i)
    {
        if ((data >> i) & 1)
        {
            this->one(dst);
        }
        else
        {
            this->zero(dst);
        }
    }
}

void
KakuProtocol::encode(RemoteTransmitData *dst, const KakuData &data)
{
    dst->set_carrier_frequency(0);

    this->sync(dst);

    this->encode_bits(dst, data.address, 26);

    this->encode_bits(dst, data.group ? 1 : 0, 1);

    if (data.dim_level > 0)
    {
        dst->item(TX_T_US, TX_T_US);
        dst->item(TX_T_US, TX_T_US);
    }
    else
    {
        this->encode_bits(dst, data.state ? 1 : 0, 1);
    }

    this->encode_bits(dst, data.unit, 4);

    if (data.dim_level > 0)
    {
        this->encode_bits(dst, data.dim_level, 4);
    }

    dst->item(TX_STOP_HIGH_US, TX_STOP_LOW_US);
}

bool
KakuProtocol::find_sync(RemoteReceiveData &src, uint32_t &period) const
{
    while (src.size() - src.get_index() >= 130)
    {
        int32_t high = src.peek(0);
        int32_t low = src.peek(1);

        // High pulse followed by long low gap
        if (high >= 100 && high <= 600 && low <= -1500 && low >= -4500)
        {
            uint32_t low_abs = -low;
            if (low_abs >= (uint32_t)high * 5 && low_abs <= (uint32_t)high * 20)
            {
                period = low_abs / 10;
                src.advance(2);
                return true;
            }
        }
        src.advance(1);
    }
    return false;
}

bool
KakuProtocol::decode_bit(RemoteReceiveData &src, uint32_t period, bool &val, bool &is_dim) const
{
    if (src.size() - src.get_index() < 4)
    {
        return false;
    }

    int32_t h1 = src.peek(0);
    int32_t l1 = -src.peek(1);
    int32_t h2 = src.peek(2);
    int32_t l2 = -src.peek(3);

    if (h1 <= 0 || l1 <= 0 || h2 <= 0 || l2 <= 0)
    {
        return false;
    }

    uint32_t max_1t = std::max<uint32_t>(period * 3, 600);
    uint32_t min_5t = period * 2;
    uint32_t max_5t = period * 8;

    if ((uint32_t)h1 > max_1t || (uint32_t)h2 > max_1t)
    {
        return false;
    }

    bool l1_is_5t = ((uint32_t)l1 >= min_5t && (uint32_t)l1 <= max_5t);
    bool l1_is_1t = ((uint32_t)l1 <= max_1t);
    bool l2_is_5t = ((uint32_t)l2 >= min_5t && (uint32_t)l2 <= max_5t);
    bool l2_is_1t = ((uint32_t)l2 <= max_1t);

    src.advance(4);

    if (l1_is_5t && l2_is_1t)
    {
        val = true;
        is_dim = false;
        return true;
    }
    else if (l1_is_1t && l2_is_5t)
    {
        val = false;
        is_dim = false;
        return true;
    }
    else if (l1_is_1t && l2_is_1t)
    {
        val = false;
        is_dim = true;
        return true;
    }

    return false;
}

bool
KakuProtocol::decode_bits(
    RemoteReceiveData &src, uint32_t period, uint8_t num_bits, uint32_t &out) const
{
    out = 0;
    for (uint8_t i = 0; i < num_bits; i++)
    {
        bool bit = false, is_dim = false;
        if (!this->decode_bit(src, period, bit, is_dim) || is_dim)
        {
            return false;
        }
        out = (out << 1) | (bit ? 1 : 0);
    }
    return true;
}

optional<KakuData>
KakuProtocol::decode(RemoteReceiveData src)
{
    KakuData out = {
        .address = 0,
        .unit = 0,
        .group = false,
        .state = false,
        .dim_level = 0,
    };

    uint32_t period = 260;
    if (!this->find_sync(src, period))
    {
        return {};
    }

    uint32_t temp_data = 0;

    if (!this->decode_bits(src, period, 26, temp_data))
    {
        return {};
    }
    out.address = temp_data;

    if (!this->decode_bits(src, period, 1, temp_data))
    {
        return {};
    }
    out.group = (temp_data == 1);

    bool bit = false, is_dim = false;
    if (!this->decode_bit(src, period, bit, is_dim))
    {
        return {};
    }

    if (is_dim)
    {
        out.state = true;
        out.dim_level = 1;
    }
    else
    {
        out.state = bit;
    }

    if (!this->decode_bits(src, period, 4, temp_data))
    {
        return {};
    }
    out.unit = temp_data;

    if (out.dim_level == 1)
    {
        this->decode_bits(src, period, 4, temp_data);
        out.dim_level = temp_data;
    }

    return {out};
}

void
KakuProtocol::dump(const KakuData &data)
{
    ESP_LOGI(TAG,
        "Received KAKU: address=0x%07" PRIX32 " unit=%u group=%s state=%s dim_level=%u",
        data.address,
        data.unit,
        data.group ? "true" : "false",
        data.state ? "ON" : "OFF",
        data.dim_level);
}

} // namespace esphome::kaku
