#include "ble_scan.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "config/device_config.h"

namespace {

constexpr size_t kMaxTracked = 8;
constexpr uint32_t kPrintMinMs = 900;
constexpr uint32_t kNearbyMaxAgeMs = 30000;

struct TrackedMac {
  bool used;
  uint8_t mac[6];
  uint16_t last_nonce;
  uint32_t last_print_ms;
};

struct NearbySlot {
  bool used;
  uint8_t mac[6];
  int8_t rssi;
  uint8_t record_type;
  uint16_t model_id;
  char gap_name[kGapNameMax];
  uint32_t seen_ms;
};

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
VictronAdvert g_queue[4];
uint8_t g_q_head = 0;
uint8_t g_q_tail = 0;
VictronAdvert g_latest = {};
bool g_latest_valid = false;
VictronAdvert g_latest_by_dev[kBleMaxCreds];
bool g_latest_by_dev_valid[kBleMaxCreds];
uint32_t g_hit_count = 0;
TrackedMac g_tracked[kMaxTracked];
NearbySlot g_nearby[kBleNearbyMax];

BleDeviceCred g_creds[kBleMaxCreds];
size_t g_cred_count = 0;
bool g_scan_running = false;
bool g_scan_paused = false;

void mac_to_str(const uint8_t mac[6], char* out, size_t out_len) {
  snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
}

TrackedMac* find_or_add(const uint8_t mac[6]) {
  for (size_t i = 0; i < kMaxTracked; i++) {
    if (g_tracked[i].used && memcmp(g_tracked[i].mac, mac, 6) == 0) {
      return &g_tracked[i];
    }
  }
  for (size_t i = 0; i < kMaxTracked; i++) {
    if (!g_tracked[i].used) {
      g_tracked[i].used = true;
      memcpy(g_tracked[i].mac, mac, 6);
      g_tracked[i].last_nonce = 0xFFFF;
      g_tracked[i].last_print_ms = 0;
      return &g_tracked[i];
    }
  }
  return nullptr;
}

int match_cred_index(const uint8_t mac[6]) {
  for (size_t i = 0; i < g_cred_count; i++) {
    if (g_creds[i].have_mac && memcmp(g_creds[i].mac, mac, 6) == 0) {
      return (int)i;
    }
  }
  // Single-cred mode without MAC filter: accept any Victron ad.
  if (g_cred_count == 1 && g_creds[0].have_key && !g_creds[0].have_mac) {
    return 0;
  }
  return -1;
}

bool queue_push(const VictronAdvert& adv) {
  portENTER_CRITICAL(&g_mux);
  g_latest = adv;
  g_latest_valid = true;
  if (adv.device_index < kBleMaxCreds) {
    g_latest_by_dev[adv.device_index] = adv;
    g_latest_by_dev_valid[adv.device_index] = true;
  }
  g_hit_count++;

  uint8_t next = (uint8_t)((g_q_head + 1) % 4);
  if (next == g_q_tail) {
    portEXIT_CRITICAL(&g_mux);
    return false;
  }
  g_queue[g_q_head] = adv;
  g_q_head = next;
  portEXIT_CRITICAL(&g_mux);
  return true;
}

bool queue_pop(VictronAdvert* out) {
  portENTER_CRITICAL(&g_mux);
  if (g_q_tail == g_q_head) {
    portEXIT_CRITICAL(&g_mux);
    return false;
  }
  *out = g_queue[g_q_tail];
  g_q_tail = (uint8_t)((g_q_tail + 1) % 4);
  portEXIT_CRITICAL(&g_mux);
  return true;
}

bool parse_victron_mfr(const uint8_t* mfr, size_t len, VictronAdvert* adv) {
  if (len < 2 + 9) return false;
  const uint16_t company = (uint16_t)mfr[0] | ((uint16_t)mfr[1] << 8);
  if (company != kVictronCompanyId) return false;
  const uint8_t* p = mfr + 2;
  // Instant Readout product ads use prefix 0x10 (sometimes 0x11).
  // VE.Smart Networking uses 0x01/0x02/0x03 — different layout; ignoring them
  // avoids false key=FAIL and bogus model_ids (e.g. Battery Sense 0x685A).
  if (p[0] != 0x10 && p[0] != 0x11) return false;
  adv->model_id = (uint16_t)p[2] | ((uint16_t)p[3] << 8);
  adv->record_type = p[4];
  adv->nonce = (uint16_t)p[5] | ((uint16_t)p[6] << 8);
  adv->key_check = p[7];
  return true;
}

void copy_gap_name(const NimBLEAdvertisedDevice* device, char* out, size_t out_len) {
  if (!out || out_len == 0) return;
  out[0] = '\0';
  if (!device || !device->haveName()) return;
  std::string name = device->getName();
  size_t n = name.size();
  if (n >= out_len) n = out_len - 1;
  memcpy(out, name.data(), n);
  out[n] = '\0';
}

void nearby_touch(const uint8_t mac[6], int8_t rssi, uint8_t record_type, uint16_t model_id,
                  const char* gap_name, uint32_t seen_ms) {
  portENTER_CRITICAL(&g_mux);
  NearbySlot* slot = nullptr;
  for (size_t i = 0; i < kBleNearbyMax; i++) {
    if (g_nearby[i].used && memcmp(g_nearby[i].mac, mac, 6) == 0) {
      slot = &g_nearby[i];
      break;
    }
  }
  if (!slot) {
    for (size_t i = 0; i < kBleNearbyMax; i++) {
      if (!g_nearby[i].used) {
        slot = &g_nearby[i];
        slot->used = true;
        memcpy(slot->mac, mac, 6);
        slot->gap_name[0] = '\0';
        break;
      }
    }
  }
  if (!slot) {
    size_t victim = 0;
    for (size_t i = 1; i < kBleNearbyMax; i++) {
      if (g_nearby[i].rssi < g_nearby[victim].rssi) victim = i;
    }
    slot = &g_nearby[victim];
    slot->used = true;
    memcpy(slot->mac, mac, 6);
    slot->gap_name[0] = '\0';
  }
  slot->rssi = rssi;
  slot->record_type = record_type;
  slot->model_id = model_id;
  slot->seen_ms = seen_ms;
  if (gap_name && gap_name[0]) {
    strncpy(slot->gap_name, gap_name, kGapNameMax - 1);
    slot->gap_name[kGapNameMax - 1] = '\0';
  }
  portEXIT_CRITICAL(&g_mux);
}

void print_sighting(const VictronAdvert& adv) {
  if (!Serial) return;
  char macstr[20];
  mac_to_str(adv.mac, macstr, sizeof(macstr));
  Serial.printf("IR %s RSSI=%d rec=0x%02X model=0x%04X dev=%u key=%s\n", macstr,
                (int)adv.rssi, adv.record_type, adv.model_id, (unsigned)adv.device_index,
                !adv.key_configured ? "-" : (adv.key_check_ok ? "OK" : "FAIL"));
}

class VictronScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    if (!device || !device->haveManufacturerData()) return;
    std::string md = device->getManufacturerData();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(md.data());
    const size_t len = md.size();
    if (len < 2 || len > kMaxMfrLen) return;

    const uint16_t company = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    if (company != kVictronCompanyId) return;

    VictronAdvert adv = {};
    NimBLEAddress addr = device->getAddress();
    const uint8_t* native = addr.getVal();
    for (int i = 0; i < 6; i++) {
      adv.mac[i] = native[5 - i];
    }
    adv.rssi = device->getRSSI();
    adv.mfr_len = (uint8_t)len;
    memcpy(adv.mfr, data, len);
    adv.seen_ms = millis();
    adv.device_index = 0xFF;
    copy_gap_name(device, adv.gap_name, sizeof(adv.gap_name));

    if (!parse_victron_mfr(adv.mfr, adv.mfr_len, &adv)) return;

    nearby_touch(adv.mac, adv.rssi, adv.record_type, adv.model_id, adv.gap_name, adv.seen_ms);

    const int idx = match_cred_index(adv.mac);
    if (idx < 0) return;

    adv.device_index = (uint8_t)idx;
    adv.key_configured = g_creds[idx].have_key;
    adv.key_check_ok =
        g_creds[idx].have_key ? (adv.key_check == g_creds[idx].key[0]) : false;
    queue_push(adv);
  }
};

VictronScanCallbacks g_callbacks;

}  // namespace

bool ble_scan_set_credentials_list(const BleDeviceCred* creds, size_t count) {
  portENTER_CRITICAL(&g_mux);
  memset(g_creds, 0, sizeof(g_creds));
  memset(g_latest_by_dev_valid, 0, sizeof(g_latest_by_dev_valid));
  g_cred_count = 0;
  g_latest_valid = false;
  if (creds && count > 0) {
    if (count > kBleMaxCreds) count = kBleMaxCreds;
    memcpy(g_creds, creds, count * sizeof(BleDeviceCred));
    g_cred_count = count;
  }
  portEXIT_CRITICAL(&g_mux);

  if (Serial) {
    Serial.printf("IR credentials: %u device(s)\n", (unsigned)g_cred_count);
  }
  return g_cred_count > 0;
}

bool ble_scan_set_credentials(const char* key_hex, const char* mac_or_null) {
  BleDeviceCred c = {};
  if (key_hex && key_hex[0] && device_config_parse_key_hex(key_hex, c.key)) {
    c.have_key = true;
  }
  if (mac_or_null && mac_or_null[0] && device_config_parse_mac(mac_or_null, c.mac)) {
    c.have_mac = true;
  }
  if (!c.have_key) {
    return ble_scan_set_credentials_list(nullptr, 0);
  }
  return ble_scan_set_credentials_list(&c, 1);
}

bool ble_scan_start(void) {
  NimBLEDevice::init("");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&g_callbacks, false);
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(80);
  scan->setMaxResults(0);
  scan->setDuplicateFilter(false);

  bool ok = scan->start(0, false, true);
  g_scan_running = ok;
  g_scan_paused = false;
  Serial.printf("NimBLE scan %s (Victron company 0x%04X)\n", ok ? "STARTED" : "FAILED",
                kVictronCompanyId);
  Serial.println("Tip: Instant Readout ON; disconnect VictronConnect if ads seem missing.");
  return ok;
}

bool ble_scan_pause(void) {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (!scan) return false;
  if (scan->isScanning()) scan->stop();
  g_scan_running = false;
  g_scan_paused = true;
  Serial.println("NimBLE scan paused (SoftAP)");
  return true;
}

bool ble_scan_resume(void) {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (!scan) return false;
  if (scan->isScanning()) {
    g_scan_running = true;
    g_scan_paused = false;
    return true;
  }
  bool ok = scan->start(0, false, true);
  g_scan_running = ok;
  g_scan_paused = !ok;
  Serial.printf("NimBLE scan %s\n", ok ? "resumed" : "resume FAILED");
  return ok;
}

bool ble_scan_is_running(void) { return g_scan_running; }

void ble_scan_loop(void) {
  VictronAdvert adv;
  while (queue_pop(&adv)) {
    TrackedMac* t = find_or_add(adv.mac);
    const uint32_t now = millis();
    bool should_print = true;
    if (t) {
      if (adv.nonce == t->last_nonce && (now - t->last_print_ms) < kPrintMinMs) {
        should_print = false;
      }
      if ((now - t->last_print_ms) < 400) should_print = false;
      if (should_print) {
        t->last_nonce = adv.nonce;
        t->last_print_ms = now;
      }
    }
    if (should_print) print_sighting(adv);
  }
}

bool ble_scan_latest(VictronAdvert* out) {
  if (!out) return false;
  portENTER_CRITICAL(&g_mux);
  bool ok = g_latest_valid;
  if (ok) *out = g_latest;
  portEXIT_CRITICAL(&g_mux);
  return ok;
}

bool ble_scan_latest_for(size_t device_index, VictronAdvert* out) {
  if (!out || device_index >= kBleMaxCreds) return false;
  portENTER_CRITICAL(&g_mux);
  bool ok = g_latest_by_dev_valid[device_index];
  if (ok) *out = g_latest_by_dev[device_index];
  portEXIT_CRITICAL(&g_mux);
  return ok;
}

uint32_t ble_scan_unique_count(void) {
  uint32_t n = 0;
  for (size_t i = 0; i < kMaxTracked; i++) {
    if (g_tracked[i].used) n++;
  }
  return n;
}

uint32_t ble_scan_hit_count(void) {
  portENTER_CRITICAL(&g_mux);
  uint32_t n = g_hit_count;
  portEXIT_CRITICAL(&g_mux);
  return n;
}

bool ble_scan_have_key(void) {
  for (size_t i = 0; i < g_cred_count; i++) {
    if (g_creds[i].have_key) return true;
  }
  return false;
}

bool ble_scan_get_key(uint8_t key_out[16]) {
  return ble_scan_get_key_for(0, key_out);
}

bool ble_scan_get_key_for(size_t device_index, uint8_t key_out[16]) {
  if (!key_out || device_index >= g_cred_count) return false;
  if (!g_creds[device_index].have_key) return false;
  memcpy(key_out, g_creds[device_index].key, 16);
  return true;
}

size_t ble_scan_nearby(BleNearbyDevice* out, size_t max_out) {
  if (!out || max_out == 0) return 0;
  const uint32_t now = millis();
  BleNearbyDevice tmp[kBleNearbyMax];
  size_t n = 0;
  portENTER_CRITICAL(&g_mux);
  for (size_t i = 0; i < kBleNearbyMax && n < kBleNearbyMax; i++) {
    if (!g_nearby[i].used) continue;
    const uint32_t age = now - g_nearby[i].seen_ms;
    // While SoftAP has paused scanning, keep the frozen snapshot (no age-out).
    if (!g_scan_paused && age > kNearbyMaxAgeMs) {
      g_nearby[i].used = false;
      continue;
    }
    tmp[n].rssi = g_nearby[i].rssi;
    tmp[n].record_type = g_nearby[i].record_type;
    tmp[n].model_id = g_nearby[i].model_id;
    tmp[n].age_ms = age;
    memcpy(tmp[n].mac, g_nearby[i].mac, 6);
    strncpy(tmp[n].gap_name, g_nearby[i].gap_name, kGapNameMax - 1);
    tmp[n].gap_name[kGapNameMax - 1] = '\0';
    n++;
  }
  portEXIT_CRITICAL(&g_mux);

  // Strongest RSSI first.
  for (size_t i = 0; i + 1 < n; i++) {
    for (size_t j = i + 1; j < n; j++) {
      if (tmp[j].rssi > tmp[i].rssi) {
        BleNearbyDevice t = tmp[i];
        tmp[i] = tmp[j];
        tmp[j] = t;
      }
    }
  }

  size_t copy_n = n < max_out ? n : max_out;
  memcpy(out, tmp, copy_n * sizeof(BleNearbyDevice));
  return copy_n;
}
