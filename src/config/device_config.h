#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

constexpr size_t kMaxDevices = 8;
constexpr size_t kDeviceNameMax = 32;
constexpr size_t kKeyHexLen = 32;  // 16 bytes as hex, no null
constexpr size_t kKeyHexBuf = kKeyHexLen + 1;
constexpr size_t kMacStrBuf = 18;   // stored form aa:bb:cc:dd:ee:ff + NUL
constexpr size_t kMacInputBuf = 32; // paste buffer (with/without separators)

enum DeviceType : uint8_t {
  kDeviceTypeAuto = 0,
  kDeviceTypeBattery = 1,
  kDeviceTypeSolar = 2,
  kDeviceTypeDcdc = 3,
};

struct DeviceEntry {
  char name[kDeviceNameMax];
  char mac[kMacInputBuf];    // empty = no MAC filter; accepts Victron paste
  char key_hex[kKeyHexBuf];  // 32 hex chars
  uint8_t type;              // DeviceType
  bool configured;           // true if key_hex is valid
};

struct DeviceList {
  DeviceEntry devices[kMaxDevices];
  size_t count;
};

// Legacy single-device shape (migration / SoftAP helpers).
struct DeviceConfig {
  char name[kDeviceNameMax];
  char mac[kMacInputBuf];
  char key_hex[kKeyHexBuf];
  bool configured;
};

// SoftAP screen flip: 0 = normal landscape, 1 = upside-down (180°).
constexpr uint8_t kOrientNormal = 0;
constexpr uint8_t kOrientFlip = 1;

bool device_config_begin(void);
bool device_config_load_all(DeviceList* out);
bool device_config_save_all(const DeviceList* list);
bool device_config_upsert(const DeviceEntry* entry);  // same MAC updates row
bool device_config_remove_at(size_t index);
bool device_config_clear(void);

uint8_t device_config_get_orient(void);
bool device_config_set_orient(uint8_t orient);
/** Reset display prefs (orient → normal). Factory reset path; not Clear-all. */
void device_config_reset_display_prefs(void);

// Convenience: load first configured device into legacy struct.
bool device_config_load(DeviceConfig* out);
bool device_config_save(const DeviceConfig* cfg);

// True if this boot cleared credentials because a new firmware was flashed.
bool device_config_wiped_for_new_fw(void);

// First Auto/Battery device index, or SIZE_MAX if none.
size_t device_config_battery_source_index(const DeviceList* list);

// Resolved cabin role from type + optional IR record_type hint.
DeviceType device_config_effective_type(const DeviceEntry* entry, uint8_t record_type_or_0);

const char* device_config_type_label(uint8_t type);

// Helpers shared with SoftAP / BLE
bool device_config_parse_key_hex(const char* hex, uint8_t out[16]);
bool device_config_parse_mac(const char* mac_str, uint8_t out[6]);
void device_config_format_mac(const uint8_t mac[6], char* out, size_t out_len);
bool device_config_parse_type(const char* s, uint8_t* out);
