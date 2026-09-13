#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct BatteryMonitorState {
  bool valid;
  bool voltage_valid;
  bool current_valid;
  bool soc_valid;
  bool temp_valid;
  float voltage_v;
  float current_a;
  float soc_pct;
  float temp_c;  // °C when aux_mode = temperature
  float power_w;
  uint16_t alarm;
  uint32_t updated_ms;
};

// Parse LSB-first bit-packed battery monitor plaintext (record 0x02).
bool victron_parse_battery_monitor(const uint8_t* plain, size_t len,
                                   BatteryMonitorState* out);
