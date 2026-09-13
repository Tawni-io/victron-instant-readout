#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

constexpr uint16_t kVictronCompanyId = 0x02E1;
constexpr size_t kMaxMfrLen = 62;
constexpr size_t kGapNameMax = 24;
constexpr size_t kBleNearbyMax = 8;
constexpr size_t kBleMaxCreds = 8;

struct VictronAdvert {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t mfr[kMaxMfrLen];
  uint8_t mfr_len;
  uint16_t model_id;
  uint8_t record_type;
  uint16_t nonce;
  uint8_t key_check;
  bool key_check_ok;
  bool key_configured;
  uint8_t device_index;  // index into credentials; 0xFF if unmatched
  char gap_name[kGapNameMax];
  uint32_t seen_ms;
};

struct BleNearbyDevice {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t record_type;
  uint16_t model_id;
  char gap_name[kGapNameMax];
  uint32_t age_ms;
};

struct BleDeviceCred {
  bool have_mac;
  uint8_t mac[6];
  bool have_key;
  uint8_t key[16];
};

bool ble_scan_start(void);
void ble_scan_loop(void);

bool ble_scan_pause(void);
bool ble_scan_resume(void);
bool ble_scan_is_running(void);

// Multi-device credentials (up to kBleMaxCreds).
bool ble_scan_set_credentials_list(const BleDeviceCred* creds, size_t count);
// Legacy single-device helper.
bool ble_scan_set_credentials(const char* key_hex, const char* mac_or_null);

bool ble_scan_have_key(void);
bool ble_scan_get_key(uint8_t key_out[16]);
bool ble_scan_get_key_for(size_t device_index, uint8_t key_out[16]);

bool ble_scan_latest(VictronAdvert* out);
bool ble_scan_latest_for(size_t device_index, VictronAdvert* out);
uint32_t ble_scan_unique_count(void);
uint32_t ble_scan_hit_count(void);

size_t ble_scan_nearby(BleNearbyDevice* out, size_t max_out);
