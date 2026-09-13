#pragma once

#include <stdint.h>
#include <stddef.h>

// Short product label from Instant Readout model_id (not VictronConnect custom name).
// Unknown IDs write "0xXXXX" into out. Returns out.
const char* victron_model_label(uint16_t model_id, char* out, size_t out_len);

// Coarse family from IR record_type (0x01 solar, 0x02 battery, 0x04 DC-DC).
const char* victron_record_label(uint8_t record_type);

// Smart Battery Sense — V (+ temp); not a full shunt / SoC monitor.
bool victron_model_is_battery_sense(uint16_t model_id);
