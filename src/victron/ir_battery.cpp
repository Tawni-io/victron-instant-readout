#include "victron/ir_battery.h"

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

bool victron_parse_battery_monitor(const uint8_t* plain, size_t len,
                                   BatteryMonitorState* out) {
  if (!plain || !out || len < 7) return false;

  BitReader r(plain, len);
  (void)r.read_unsigned(16);  // remaining_mins
  const uint32_t voltage_u = r.read_unsigned(16);
  const uint16_t alarm = (uint16_t)r.read_unsigned(16);
  const uint32_t aux = r.read_unsigned(16);
  const uint32_t aux_mode = r.read_unsigned(2);  // 0 starter, 1 mid, 2 temp, 3 off
  const uint32_t current_u = r.read_unsigned(22);
  (void)r.read_unsigned(20);  // consumed_ah
  const uint32_t soc_raw = r.read_unsigned(10);

  *out = {};
  out->valid = true;
  out->alarm = alarm;

  if (voltage_u != 0x7FFFu) {
    int32_t voltage_raw = (int32_t)voltage_u;
    if (voltage_u & 0x8000u) voltage_raw -= 0x10000;
    out->voltage_valid = true;
    out->voltage_v = voltage_raw / 100.0f;
  }
  // Aux temperature: Kelvin × 100 (keshavdv/victron-ble BatteryMonitor).
  if (aux_mode == 2u && aux != 0u && aux != 0xFFFFu) {
    out->temp_valid = true;
    out->temp_c = (aux / 100.0f) - 273.15f;
  }
  if (current_u != 0x3FFFFFu) {
    int32_t current_raw = (int32_t)current_u;
    if (current_u & 0x200000u) current_raw -= 0x400000;
    out->current_valid = true;
    out->current_a = current_raw / 1000.0f;
  }
  if (soc_raw != 0x3FFu) {
    out->soc_valid = true;
    out->soc_pct = soc_raw / 10.0f;
  }
  if (out->voltage_valid && out->current_valid) {
    out->power_w = out->voltage_v * out->current_a;
  }
  return out->voltage_valid || out->current_valid || out->soc_valid || out->temp_valid;
}
