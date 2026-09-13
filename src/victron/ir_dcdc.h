#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct DcdcConverterState {
  bool valid;
  bool input_valid;
  bool output_valid;
  bool state_valid;
  float input_v;
  float output_v;
  uint8_t device_state;
  uint8_t charger_error;
  uint32_t off_reason;
  uint32_t updated_ms;
};

bool victron_parse_dcdc_converter(const uint8_t* plain, size_t len, DcdcConverterState* out);
