#include "config/device_config.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

namespace {

Preferences g_prefs;
constexpr const char* kNs = "vtdash";
constexpr const char* kKeyCount = "n";
constexpr const char* kKeyFwMd5 = "fw_md5";
constexpr const char* kKeyOrient = "orient";
// Legacy single-device keys (migrated on load).
constexpr const char* kLegacyName = "name";
constexpr const char* kLegacyMac = "mac";
constexpr const char* kLegacyKey = "key";

bool g_wiped_for_new_fw = false;

#ifndef WIPE_CONFIG_ON_NEW_FW
#define WIPE_CONFIG_ON_NEW_FW 1
#endif

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

void trim_copy(const char* in, char* out, size_t out_len) {
  if (!out || out_len == 0) return;
  out[0] = '\0';
  if (!in) return;
  while (*in && isspace((unsigned char)*in)) in++;
  size_t n = strlen(in);
  while (n > 0 && isspace((unsigned char)in[n - 1])) n--;
  if (n >= out_len) n = out_len - 1;
  memcpy(out, in, n);
  out[n] = '\0';
}

void slot_keys(size_t i, char* name_k, char* mac_k, char* key_k, char* type_k, size_t klen) {
  snprintf(name_k, klen, "n%u", (unsigned)i);
  snprintf(mac_k, klen, "m%u", (unsigned)i);
  snprintf(key_k, klen, "k%u", (unsigned)i);
  snprintf(type_k, klen, "t%u", (unsigned)i);
}

void normalize_entry(DeviceEntry* e) {
  if (!e) return;
  uint8_t raw[16];
  e->configured = device_config_parse_key_hex(e->key_hex, raw);
  if (e->type > kDeviceTypeDcdc) e->type = kDeviceTypeAuto;
  if (e->configured && e->name[0] == '\0') {
    strncpy(e->name, "Device", kDeviceNameMax - 1);
  }
}

void prefs_remove_if(const char* key) {
  if (g_prefs.isKey(key)) {
    g_prefs.remove(key);
  }
}

String prefs_get_string(const char* key, const char* def = "") {
  if (!g_prefs.isKey(key)) {
    return String(def);
  }
  return g_prefs.getString(key, def);
}

bool migrate_legacy_if_needed(void) {
  if (g_prefs.isKey(kKeyCount)) return false;
  if (!g_prefs.isKey(kLegacyKey)) return false;
  String key = prefs_get_string(kLegacyKey);
  if (key.length() == 0) return false;

  DeviceList list = {};
  list.count = 1;
  String name = prefs_get_string(kLegacyName);
  String mac = prefs_get_string(kLegacyMac);
  strncpy(list.devices[0].name, name.c_str(), kDeviceNameMax - 1);
  strncpy(list.devices[0].mac, mac.c_str(), kMacInputBuf - 1);
  strncpy(list.devices[0].key_hex, key.c_str(), kKeyHexLen);
  list.devices[0].type = kDeviceTypeAuto;
  normalize_entry(&list.devices[0]);

  prefs_remove_if(kLegacyName);
  prefs_remove_if(kLegacyMac);
  prefs_remove_if(kLegacyKey);
  device_config_save_all(&list);
  Serial.println("NVS: migrated legacy single-device credentials");
  return true;
}

void clear_all_device_slots(void) {
  for (size_t i = 0; i < kMaxDevices; i++) {
    char nk[8], mk[8], kk[8], tk[8];
    slot_keys(i, nk, mk, kk, tk, sizeof(nk));
    prefs_remove_if(nk);
    prefs_remove_if(mk);
    prefs_remove_if(kk);
    prefs_remove_if(tk);
  }
  prefs_remove_if(kKeyCount);
  prefs_remove_if(kLegacyName);
  prefs_remove_if(kLegacyMac);
  prefs_remove_if(kLegacyKey);
}

}  // namespace

bool device_config_parse_key_hex(const char* hex, uint8_t out[16]) {
  if (!hex || !out) return false;
  char buf[kKeyHexBuf];
  trim_copy(hex, buf, sizeof(buf));
  char clean[kKeyHexBuf];
  size_t n = 0;
  for (size_t i = 0; buf[i] && n < kKeyHexLen; i++) {
    if (buf[i] == ' ' || buf[i] == ':' || buf[i] == '-') continue;
    clean[n++] = buf[i];
  }
  clean[n] = '\0';
  if (n != kKeyHexLen) return false;
  for (int i = 0; i < 16; i++) {
    int hi = hex_nibble(clean[i * 2]);
    int lo = hex_nibble(clean[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

bool device_config_parse_mac(const char* mac_str, uint8_t out[6]) {
  if (!mac_str || !out) return false;
  char buf[32];
  trim_copy(mac_str, buf, sizeof(buf));
  int wi = 0;
  for (int i = 0; buf[i] && wi < 6;) {
    if (buf[i] == ':' || buf[i] == '-' || buf[i] == ' ') {
      i++;
      continue;
    }
    int hi = hex_nibble(buf[i]);
    int lo = hex_nibble(buf[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[wi++] = (uint8_t)((hi << 4) | lo);
    i += 2;
  }
  return wi == 6;
}

void device_config_format_mac(const uint8_t mac[6], char* out, size_t out_len) {
  if (!out || out_len < kMacStrBuf) return;
  snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
}

bool device_config_parse_type(const char* s, uint8_t* out) {
  if (!out) return false;
  if (!s || !s[0]) {
    *out = kDeviceTypeAuto;
    return true;
  }
  if (strcasecmp(s, "auto") == 0 || strcmp(s, "0") == 0) {
    *out = kDeviceTypeAuto;
    return true;
  }
  if (strcasecmp(s, "battery") == 0 || strcasecmp(s, "shunt") == 0 ||
      strcmp(s, "1") == 0) {
    *out = kDeviceTypeBattery;
    return true;
  }
  if (strcasecmp(s, "solar") == 0 || strcmp(s, "2") == 0) {
    *out = kDeviceTypeSolar;
    return true;
  }
  if (strcasecmp(s, "dcdc") == 0 || strcasecmp(s, "dc-dc") == 0 || strcmp(s, "3") == 0) {
    *out = kDeviceTypeDcdc;
    return true;
  }
  return false;
}

const char* device_config_type_label(uint8_t type) {
  switch (type) {
    case kDeviceTypeBattery:
      return "Shunt";
    case kDeviceTypeSolar:
      return "Solar";
    case kDeviceTypeDcdc:
      return "DC-DC";
    default:
      return "Auto";
  }
}

DeviceType device_config_effective_type(const DeviceEntry* entry, uint8_t record_type_or_0) {
  if (!entry) return kDeviceTypeAuto;
  if (entry->type == kDeviceTypeBattery || entry->type == kDeviceTypeSolar ||
      entry->type == kDeviceTypeDcdc) {
    return (DeviceType)entry->type;
  }
  switch (record_type_or_0) {
    case 0x01:
      return kDeviceTypeSolar;
    case 0x02:
      return kDeviceTypeBattery;
    case 0x04:
      return kDeviceTypeDcdc;
    default:
      return kDeviceTypeBattery;
  }
}

bool device_config_begin(void) {
  if (!g_prefs.begin(kNs, false)) return false;

#if WIPE_CONFIG_ON_NEW_FW
  const String sketch_md5 = ESP.getSketchMD5();
  const String stored_md5 = prefs_get_string(kKeyFwMd5);
  if (stored_md5 != sketch_md5) {
    clear_all_device_slots();
    g_prefs.putString(kKeyFwMd5, sketch_md5);
    g_wiped_for_new_fw = true;
    Serial.println("NVS credentials cleared (new firmware upload)");
  }
#endif
  migrate_legacy_if_needed();
  return true;
}

bool device_config_load_all(DeviceList* out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));

  migrate_legacy_if_needed();

  const uint32_t n = g_prefs.isKey(kKeyCount) ? g_prefs.getUInt(kKeyCount, 0) : 0;
  size_t count = n > kMaxDevices ? kMaxDevices : (size_t)n;

  for (size_t i = 0; i < count; i++) {
    char nk[8], mk[8], kk[8], tk[8];
    slot_keys(i, nk, mk, kk, tk, sizeof(nk));
    String name = prefs_get_string(nk);
    String mac = prefs_get_string(mk);
    String key = prefs_get_string(kk);
    uint8_t type =
        g_prefs.isKey(tk) ? (uint8_t)g_prefs.getUChar(tk, kDeviceTypeAuto) : kDeviceTypeAuto;

    DeviceEntry& e = out->devices[out->count];
    strncpy(e.name, name.c_str(), kDeviceNameMax - 1);
    strncpy(e.mac, mac.c_str(), kMacInputBuf - 1);
    strncpy(e.key_hex, key.c_str(), kKeyHexLen);
    e.type = type;
    normalize_entry(&e);
    if (e.configured) {
      out->count++;
    }
  }
  return true;
}

bool device_config_save_all(const DeviceList* list) {
  if (!list) return false;

  clear_all_device_slots();

  size_t written = 0;
  for (size_t i = 0; i < list->count && written < kMaxDevices; i++) {
    DeviceEntry e = list->devices[i];
    uint8_t raw[16];
    if (!device_config_parse_key_hex(e.key_hex, raw)) continue;

    char key_norm[kKeyHexBuf];
    for (int b = 0; b < 16; b++) {
      snprintf(key_norm + b * 2, 3, "%02x", raw[b]);
    }

    char mac_norm[kMacStrBuf] = "";
    if (e.mac[0]) {
      uint8_t mac[6];
      if (!device_config_parse_mac(e.mac, mac)) return false;
      device_config_format_mac(mac, mac_norm, sizeof(mac_norm));
    }

    char name[kDeviceNameMax];
    trim_copy(e.name, name, sizeof(name));
    if (name[0] == '\0') {
      strncpy(name, "Device", sizeof(name) - 1);
    }
    if (e.type > kDeviceTypeDcdc) e.type = kDeviceTypeAuto;

    char nk[8], mk[8], kk[8], tk[8];
    slot_keys(written, nk, mk, kk, tk, sizeof(nk));
    g_prefs.putString(nk, name);
    g_prefs.putString(mk, mac_norm);
    g_prefs.putString(kk, key_norm);
    g_prefs.putUChar(tk, e.type);
    written++;
  }

  g_prefs.putUInt(kKeyCount, (uint32_t)written);
  return true;
}

bool device_config_upsert(const DeviceEntry* entry) {
  if (!entry) return false;
  DeviceList list = {};
  device_config_load_all(&list);

  uint8_t mac[6];
  bool have_mac = entry->mac[0] && device_config_parse_mac(entry->mac, mac);

  int found = -1;
  if (have_mac) {
    for (size_t i = 0; i < list.count; i++) {
      uint8_t existing[6];
      if (list.devices[i].mac[0] && device_config_parse_mac(list.devices[i].mac, existing) &&
          memcmp(existing, mac, 6) == 0) {
        found = (int)i;
        break;
      }
    }
  }

  DeviceEntry e = *entry;
  normalize_entry(&e);
  if (!e.configured) return false;

  if (found >= 0) {
    list.devices[found] = e;
  } else {
    if (list.count >= kMaxDevices) return false;
    list.devices[list.count++] = e;
  }
  return device_config_save_all(&list);
}

bool device_config_remove_at(size_t index) {
  DeviceList list = {};
  device_config_load_all(&list);
  if (index >= list.count) return false;
  for (size_t i = index; i + 1 < list.count; i++) {
    list.devices[i] = list.devices[i + 1];
  }
  list.count--;
  return device_config_save_all(&list);
}

bool device_config_clear(void) {
  DeviceList empty = {};
  bool ok = device_config_save_all(&empty);
#if WIPE_CONFIG_ON_NEW_FW
  g_prefs.putString(kKeyFwMd5, ESP.getSketchMD5());
#endif
  return ok;
}

bool device_config_load(DeviceConfig* out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  DeviceList list = {};
  device_config_load_all(&list);
  if (list.count == 0) return true;
  strncpy(out->name, list.devices[0].name, kDeviceNameMax - 1);
  strncpy(out->mac, list.devices[0].mac, kMacInputBuf - 1);
  strncpy(out->key_hex, list.devices[0].key_hex, kKeyHexLen);
  out->configured = list.devices[0].configured;
  return true;
}

bool device_config_save(const DeviceConfig* cfg) {
  if (!cfg) return false;
  DeviceEntry e = {};
  strncpy(e.name, cfg->name, kDeviceNameMax - 1);
  strncpy(e.mac, cfg->mac, kMacInputBuf - 1);
  strncpy(e.key_hex, cfg->key_hex, kKeyHexLen);
  e.type = kDeviceTypeAuto;
  return device_config_upsert(&e);
}

size_t device_config_battery_source_index(const DeviceList* list) {
  if (!list) return SIZE_MAX;
  for (size_t i = 0; i < list->count; i++) {
    if (!list->devices[i].configured) continue;
    if (list->devices[i].type == kDeviceTypeBattery ||
        list->devices[i].type == kDeviceTypeAuto) {
      return i;
    }
  }
  for (size_t i = 0; i < list->count; i++) {
    if (list->devices[i].configured) return i;
  }
  return SIZE_MAX;
}

bool device_config_wiped_for_new_fw(void) { return g_wiped_for_new_fw; }

uint8_t device_config_get_orient(void) {
  if (!g_prefs.isKey(kKeyOrient)) {
    return kOrientNormal;
  }
  const uint8_t v = g_prefs.getUChar(kKeyOrient, kOrientNormal);
  return (v == kOrientFlip) ? kOrientFlip : kOrientNormal;
}

bool device_config_set_orient(uint8_t orient) {
  const uint8_t v = (orient == kOrientFlip) ? kOrientFlip : kOrientNormal;
  g_prefs.putUChar(kKeyOrient, v);
  return device_config_get_orient() == v;
}

void device_config_reset_display_prefs(void) {
  prefs_remove_if(kKeyOrient);
}
