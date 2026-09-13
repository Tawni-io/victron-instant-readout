#include "victron/ir_solar.h"

namespace {

class BitReader {
 public:
  BitReader(const uint8_t* data, size_t len) : data_(data), len_(len), bit_(0) {}

  uint32_t read_unsigned(int num_bits) {
    uint32_t value = 0;
    for (int i = 0; i < num_bits; i++) {
      const size_t byte_i = bit_ >> 3;
      if (byte_i >= len_) return 0;
      const uint32_t b = (data_[byte_i] >> (bit_ & 7)) & 1u;
      value |= (b << i);
      bit_++;
    }
    return value;
  }

  int32_t read_signed(int num_bits) {
    const uint32_t u = read_unsigned(num_bits);
    if (u & (1u << (num_bits - 1))) {
      return (int32_t)u - (int32_t)(1u << num_bits);
    }
    return (int32_t)u;
  }

 private:
  const uint8_t* data_;
  size_t len_;
  size_t bit_;
};

}  // namespace

const char* victron_charge_state_label(uint8_t state) {
  switch (state) {
    case 0:
      return "OFF";
    case 1:
      return "LOW POWER";
    case 2:
      return "FAULT";
    case 3:
      return "BULK";
    case 4:
      return "ABSORB";
    case 5:
      return "FLOAT";
    case 6:
      return "STORAGE";
    case 7:
      return "EQUALIZE";
    case 11:
      return "OTHER";
    default:
      return "----";
  }
}

bool victron_parse_solar_charger(const uint8_t* plain, size_t len, SolarChargerState* out) {
  if (!plain || !out || len < 10) return false;

  BitReader r(plain, len);
  const uint32_t charge_state = r.read_unsigned(8);
  const uint32_t charger_error = r.read_unsigned(8);
  const int32_t battery_voltage = r.read_signed(16);
  const int32_t battery_current = r.read_signed(16);
  const uint32_t yield_today = r.read_unsigned(16);
  const uint32_t solar_power = r.read_unsigned(16);
  (void)r.read_unsigned(9);  // external load

  *out = {};
  out->valid = true;
  out->charger_error = (uint8_t)charger_error;
  out->charge_state = (uint8_t)charge_state;
  out->state_valid = charge_state != 0xFFu;

  if ((uint16_t)battery_voltage != 0x7FFFu) {
    out->voltage_valid = true;
    out->battery_v = battery_voltage / 100.0f;
  }
  if ((uint16_t)battery_current != 0x7FFFu) {
    out->current_valid = true;
    out->battery_a = battery_current / 10.0f;
  }
  if (yield_today != 0xFFFFu) {
    out->yield_today_wh = yield_today * 10.0f;
  }
  if (solar_power != 0xFFFFu) {
    out->power_valid = true;
    out->solar_w = (float)solar_power;
  }
  return out->voltage_valid || out->current_valid || out->power_valid || out->state_valid;
}
