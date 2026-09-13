#include "victron/ir_dcdc.h"

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

bool victron_parse_dcdc_converter(const uint8_t* plain, size_t len, DcdcConverterState* out) {
  if (!plain || !out || len < 8) return false;

  BitReader r(plain, len);
  const uint32_t device_state = r.read_unsigned(8);
  const uint32_t charger_error = r.read_unsigned(8);
  const uint32_t input_voltage = r.read_unsigned(16);
  const int32_t output_voltage = r.read_signed(16);
  const uint32_t off_reason = r.read_unsigned(32);

  *out = {};
  out->valid = true;
  out->device_state = (uint8_t)device_state;
  out->charger_error = (uint8_t)charger_error;
  out->off_reason = off_reason;
  out->state_valid = device_state != 0xFFu;

  if (input_voltage != 0xFFFFu) {
    out->input_valid = true;
    out->input_v = input_voltage / 100.0f;
  }
  if ((uint16_t)output_voltage != 0x7FFFu) {
    out->output_valid = true;
    out->output_v = output_voltage / 100.0f;
  }
  return out->input_valid || out->output_valid || out->state_valid;
}
