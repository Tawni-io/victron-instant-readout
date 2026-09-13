#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct SolarChargerState {
  bool valid;
  bool voltage_valid;
  bool current_valid;
  bool power_valid;
  bool state_valid;
  float battery_v;
  float battery_a;
  float solar_w;
  float yield_today_wh;
  uint8_t charge_state;  // Victron OperationMode; 0xFF = NA
  uint8_t charger_error;
  uint32_t updated_ms;
};

bool victron_parse_solar_charger(const uint8_t* plain, size_t len, SolarChargerState* out);

const char* victron_charge_state_label(uint8_t state);
