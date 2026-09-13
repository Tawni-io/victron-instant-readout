#include <Arduino.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>

#include "ble_scan.h"
#include "board_pins.h"
#include "config/device_config.h"
#include "diag.h"
#include "display.h"
#include "softap/softap.h"
#include "touch.h"
#include "ui/dashboard.h"
#include "ui/lvgl_port.h"
#include "ui/splash.h"
#include "victron/ir_battery.h"
#include "victron/ir_dcdc.h"
#include "victron/ir_decrypt.h"
#include "victron/ir_models.h"
#include "victron/ir_solar.h"

#if __has_include("victron_secrets.h")
#include "victron_secrets.h"
#endif

// Set to 1 to bisect: decode+serial only, skip cabin LVGL widget updates.
#ifndef VICTRONDASH_SKIP_CABIN_PAINT
#define VICTRONDASH_SKIP_CABIN_PAINT 0
#endif

#ifndef VICTRONDASH_UI_DEBUG
#define VICTRONDASH_UI_DEBUG 1
#endif

#ifndef VICTRONDASH_VERSION
#define VICTRONDASH_VERSION "0.3.8"
#endif

namespace {

enum PageKind : uint8_t { kPageBattery = 0, kPageSolar = 1, kPageOrion = 2, kPageInfo = 3 };

constexpr uint32_t kSoftOffHoldMs = 3000;
constexpr uint32_t kWakeConfirmMs = 1500;

// Board LiPo: sleep before pack UVLO. Field: fade/blink ~3.10 V, protection
// opened (USB showed "No LiPo"), then recovered ~3.19 V on charge.
constexpr int kLipoPresentMinMv = 2500;  // below = path open / no cell (not 3.00)
constexpr int kLipoEmptyMv = 3200;       // auto soft-off while discharging (3.20 V)
constexpr int kIbatThreshMa = 15;
constexpr uint32_t kLipoEmptyHoldMs = 4000;
constexpr uint32_t kLipoEmptyMsgMs = 1500;

struct PageSlot {
  PageKind kind;
  size_t device_index;
};

struct DeviceRuntime {
  bool have_batt;
  bool have_solar;
  bool have_dcdc;
  BatteryMonitorState batt;
  SolarChargerState solar;
  DcdcConverterState dcdc;
  uint32_t last_nonce;
  int8_t rssi;
  uint8_t last_record_type;
  uint16_t last_model_id;
  bool key_fail;
};

DeviceList g_devices = {};
DeviceRuntime g_rt[kMaxDevices] = {};
PageSlot g_pages[kMaxDevices + 2] = {};
size_t g_page_count = 0;
size_t g_page_index = 0;
size_t g_batt_source = SIZE_MAX;

bool g_setup_ui = false;
bool g_ui_ready = false;
bool g_ui_dirty = false;

struct BoardPower {
  bool present;
  bool ok;
  bool on_usb;
  bool voltage_valid;
  int mV;
  bool soc_valid;
  int soc_pct;
};

BoardPower g_board_power = {};

constexpr uint32_t kLongPressMs = 2000;

void enter_deep_sleep(void) {
  // GPIO0 active-low wake (LilyGO: deep-sleep wake pins IO0–IO6).
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  Serial.println("Soft power → deep sleep (wake: hold GPIO0)");
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
}

/** Soft power-off: radios down, panel sleep, deep sleep. Does not return. */
void soft_power_off(void) {
  Serial.println("Soft power off");
  if (softap_active()) {
    softap_stop();
    softap_clear_stop_request();
  }
  ble_scan_pause();
  display_enter_sleep();
  delay(100);
  enter_deep_sleep();
}

void poll_board_power(void) {
  static uint32_t last_read_ms = 0;
  const uint32_t now = millis();
  if (last_read_ms != 0 && (now - last_read_ms) < 2000u) {
    return;
  }
  last_read_ms = now;

  g_board_power.present = board_power_present();
  if (!g_board_power.present) {
    g_board_power.ok = false;
    g_board_power.on_usb = true;
    g_board_power.voltage_valid = false;
    g_board_power.mV = 0;
    g_board_power.soc_valid = false;
    g_board_power.soc_pct = 0;
    return;
  }

  int mV = 0;
  int soc = -1;
  int i_mA = 0;
  g_board_power.ok = board_power_read(&mV, &soc, &i_mA);
  if (!g_board_power.ok) {
    return;
  }
  g_board_power.mV = mV;
  g_board_power.voltage_valid = mV >= kLipoPresentMinMv;
  g_board_power.soc_valid = (soc >= 0 && soc <= 100);
  g_board_power.soc_pct = g_board_power.soc_valid ? soc : 0;
  // Negative current = discharging from the cell. USB charge/idle stays awake
  // even when the pack is empty so plugging in recovers it.
  g_board_power.on_usb = (i_mA >= -kIbatThreshMa);
}

/** Deep sleep before pack UVLO / backlight brownout. Same path as dual-button. */
void maybe_lipo_empty_sleep(void) {
  static uint32_t below_since_ms = 0;
  if (softap_ota_active()) {
    below_since_ms = 0;
    return;
  }
  poll_board_power();
  const bool empty = g_board_power.ok && !g_board_power.on_usb &&
                     g_board_power.voltage_valid && g_board_power.mV <= kLipoEmptyMv;
  if (!empty) {
    below_since_ms = 0;
    return;
  }
  const uint32_t now = millis();
  if (below_since_ms == 0) {
    below_since_ms = now;
    Serial.printf("LiPo low %d mV — soft-off if still discharging\n", g_board_power.mV);
  }
  if ((now - below_since_ms) < kLipoEmptyHoldMs) {
    return;
  }

  Serial.printf("LiPo empty %d mV → soft-off\n", g_board_power.mV);
  if (!softap_active()) {
    dashboard_show_message("BATTERY EMPTY", 0xFF1744);
    const uint32_t until = millis() + kLipoEmptyMsgMs;
    while (millis() < until) {
      diag_wdt_feed();
      lvgl_port_handler();
      delay(10);
    }
  }
  soft_power_off();
}

bool woke_from_gpio(void) {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
}

/** Pocket bump: sleep again if they already let go. */
void abort_gpio_wake_if_released(bool panel_on) {
  if (!woke_from_gpio()) {
    return;
  }
  if (digitalRead(BUTTON_PIN) != LOW) {
    Serial.println("Wake confirm released → sleep again");
    if (panel_on) {
      display_enter_sleep();
    }
    enter_deep_sleep();
  }
}

const char* orion_state_label(uint8_t state) {
  // Reuse solar charge-state labels where they overlap; else show hex.
  const char* s = victron_charge_state_label(state);
  if (s && strcmp(s, "----") != 0) return s;
  return "----";
}

void apply_credentials(void) {
  BleDeviceCred creds[kMaxDevices] = {};
  size_t n = 0;

  for (size_t i = 0; i < g_devices.count && n < kMaxDevices; i++) {
    if (!g_devices.devices[i].configured) continue;
    BleDeviceCred& c = creds[n];
    c.have_mac = g_devices.devices[i].mac[0] &&
                 device_config_parse_mac(g_devices.devices[i].mac, c.mac);
    c.have_key = device_config_parse_key_hex(g_devices.devices[i].key_hex, c.key);
    if (c.have_key) n++;
  }

  if (n == 0) {
#ifdef VICTRON_IR_KEY_HEX
    BleDeviceCred c = {};
    if (device_config_parse_key_hex(VICTRON_IR_KEY_HEX, c.key)) {
      c.have_key = true;
#ifdef VICTRON_IR_MAC
      c.have_mac = device_config_parse_mac(VICTRON_IR_MAC, c.mac);
#endif
      creds[0] = c;
      n = 1;
    }
#endif
  }

  ble_scan_set_credentials_list(creds, n);
}

void rebuild_pages(void) {
  g_page_count = 0;
  g_batt_source = device_config_battery_source_index(&g_devices);

  if (g_batt_source != SIZE_MAX) {
    g_pages[g_page_count++] = {kPageBattery, g_batt_source};
  }

  for (size_t i = 0; i < g_devices.count; i++) {
    if (!g_devices.devices[i].configured) continue;
    if (i == g_batt_source) continue;

    const DeviceType t =
        device_config_effective_type(&g_devices.devices[i], g_rt[i].last_record_type);
    if (t == kDeviceTypeSolar) {
      g_pages[g_page_count++] = {kPageSolar, i};
    } else if (t == kDeviceTypeDcdc) {
      g_pages[g_page_count++] = {kPageOrion, i};
    } else if (t == kDeviceTypeBattery) {
      // Extra battery monitor — show as its own battery-style page via solar? Keep solar/orion
      // only for non-primary. Extra shunt gets a battery page.
      g_pages[g_page_count++] = {kPageBattery, i};
    } else {
      // Auto without IR yet: skip until we see a record type (except primary already added).
      if (g_rt[i].last_record_type == 0x01) {
        g_pages[g_page_count++] = {kPageSolar, i};
      } else if (g_rt[i].last_record_type == 0x04) {
        g_pages[g_page_count++] = {kPageOrion, i};
      } else if (g_rt[i].last_record_type == 0x02) {
        g_pages[g_page_count++] = {kPageBattery, i};
      }
    }
  }

  if (g_page_count == 0 && g_devices.count > 0) {
    g_pages[g_page_count++] = {kPageBattery, 0};
    g_batt_source = 0;
  }

  // Always last: read-only cabin Info (devices / board / SoftAP / version).
  g_pages[g_page_count++] = {kPageInfo, SIZE_MAX};

  if (g_page_index >= g_page_count) g_page_index = 0;
  Serial.printf("Pages: %u (batt source=%u)\n", (unsigned)g_page_count,
                g_batt_source == SIZE_MAX ? 999u : (unsigned)g_batt_source);
}

void reload_device_config(void) {
  device_config_load_all(&g_devices);
  memset(g_rt, 0, sizeof(g_rt));
  for (size_t i = 0; i < kMaxDevices; i++) {
    g_rt[i].last_nonce = 0xFFFFFFFF;
  }
  apply_credentials();
  rebuild_pages();
  g_ui_ready = false;
  g_ui_dirty = true;
}

uint8_t status_for_device(size_t di, bool valid, uint32_t age_ms) {
  if (!ble_scan_have_key()) return 3;
  if (di < kMaxDevices && g_rt[di].key_fail) return 4;
  if (valid && age_ms < 5000) return 1;
  if (valid) return 2;
  return 0;
}

void fill_name(char* out, size_t out_len, size_t di) {
  if (!out || out_len == 0) return;
  out[0] = '\0';
  if (di < g_devices.count && g_devices.devices[di].name[0]) {
    strncpy(out, g_devices.devices[di].name, out_len - 1);
    out[out_len - 1] = '\0';
  }
}

DashSnapshot snapshot_battery(size_t di) {
  DashSnapshot s = {};
  fill_name(s.name, sizeof(s.name), di);
  if (di >= kMaxDevices) return s;

  const DeviceRuntime& rt = g_rt[di];
  const uint32_t now = millis();
  const uint32_t age = rt.have_batt ? (now - rt.batt.updated_ms) : 0xFFFFFFFFu;
  s.sense_mode = victron_model_is_battery_sense(rt.last_model_id);
  s.valid = rt.have_batt && rt.batt.valid;
  s.voltage_valid = rt.have_batt && rt.batt.voltage_valid;
  s.current_valid = rt.have_batt && rt.batt.current_valid && !s.sense_mode;
  s.soc_valid = rt.have_batt && rt.batt.soc_valid && !s.sense_mode;
  s.temp_valid = rt.have_batt && rt.batt.temp_valid;
  s.voltage_cV = s.voltage_valid ? (int)lroundf(rt.batt.voltage_v * 100.0f) : 0;
  s.current_dA = s.current_valid ? (int)lroundf(rt.batt.current_a * 10.0f) : 0;
  s.soc_pct = s.soc_valid ? (int)lroundf(rt.batt.soc_pct) : 0;
  s.temp_dC = s.temp_valid ? (int)lroundf(rt.batt.temp_c * 10.0f) : 0;
  s.power_w = (s.valid && s.current_valid) ? (int)lroundf(rt.batt.power_w) : 0;
  s.alarm = s.valid ? rt.batt.alarm : 0;
  s.age_s = s.valid ? (int)(age / 1000u) : -1;
  s.rssi = rt.rssi;
  s.status = status_for_device(di, s.valid, age);

  // Fallback: show solar/dcdc battery voltage on battery page if no shunt decode yet.
  if (!s.voltage_valid && rt.have_solar && rt.solar.voltage_valid) {
    s.voltage_valid = true;
    s.voltage_cV = (int)lroundf(rt.solar.battery_v * 100.0f);
    s.valid = true;
    s.age_s = (int)((now - rt.solar.updated_ms) / 1000u);
    s.status = status_for_device(di, true, now - rt.solar.updated_ms);
  } else if (!s.voltage_valid && rt.have_dcdc && rt.dcdc.output_valid) {
    s.voltage_valid = true;
    s.voltage_cV = (int)lroundf(rt.dcdc.output_v * 100.0f);
    s.valid = true;
    s.age_s = (int)((now - rt.dcdc.updated_ms) / 1000u);
    s.status = status_for_device(di, true, now - rt.dcdc.updated_ms);
  }
  return s;
}

SolarSnapshot snapshot_solar(size_t di) {
  SolarSnapshot s = {};
  fill_name(s.name, sizeof(s.name), di);
  if (di >= kMaxDevices) return s;
  const DeviceRuntime& rt = g_rt[di];
  const uint32_t now = millis();
  const uint32_t age = rt.have_solar ? (now - rt.solar.updated_ms) : 0xFFFFFFFFu;
  s.valid = rt.have_solar && rt.solar.valid;
  s.voltage_valid = rt.have_solar && rt.solar.voltage_valid;
  s.current_valid = rt.have_solar && rt.solar.current_valid;
  s.power_valid = rt.have_solar && rt.solar.power_valid;
  s.state_valid = rt.have_solar && rt.solar.state_valid;
  s.battery_cV = s.voltage_valid ? (int)lroundf(rt.solar.battery_v * 100.0f) : 0;
  s.battery_dA = s.current_valid ? (int)lroundf(rt.solar.battery_a * 10.0f) : 0;
  s.solar_w = s.power_valid ? (int)lroundf(rt.solar.solar_w) : 0;
  s.charge_state = s.state_valid ? rt.solar.charge_state : 0xFF;
  s.age_s = s.valid ? (int)(age / 1000u) : -1;
  s.rssi = rt.rssi;
  s.status = status_for_device(di, s.valid, age);
  const char* lbl = victron_charge_state_label(s.charge_state);
  strncpy(s.state_label, lbl ? lbl : "----", sizeof(s.state_label) - 1);
  return s;
}

OrionSnapshot snapshot_orion(size_t di) {
  OrionSnapshot s = {};
  fill_name(s.name, sizeof(s.name), di);
  if (di >= kMaxDevices) return s;
  const DeviceRuntime& rt = g_rt[di];
  const uint32_t now = millis();
  const uint32_t age = rt.have_dcdc ? (now - rt.dcdc.updated_ms) : 0xFFFFFFFFu;
  s.valid = rt.have_dcdc && rt.dcdc.valid;
  s.input_valid = rt.have_dcdc && rt.dcdc.input_valid;
  s.output_valid = rt.have_dcdc && rt.dcdc.output_valid;
  s.state_valid = rt.have_dcdc && rt.dcdc.state_valid;
  s.input_cV = s.input_valid ? (int)lroundf(rt.dcdc.input_v * 100.0f) : 0;
  s.output_cV = s.output_valid ? (int)lroundf(rt.dcdc.output_v * 100.0f) : 0;
  s.device_state = s.state_valid ? rt.dcdc.device_state : 0xFF;
  s.age_s = s.valid ? (int)(age / 1000u) : -1;
  s.rssi = rt.rssi;
  s.status = status_for_device(di, s.valid, age);
  const char* lbl = orion_state_label(s.device_state);
  strncpy(s.state_label, lbl ? lbl : "----", sizeof(s.state_label) - 1);
  return s;
}

InfoSnapshot snapshot_info(void) {
  InfoSnapshot s = {};

  uint8_t n_cfg = 0;
  for (size_t i = 0; i < g_devices.count; i++) {
    if (g_devices.devices[i].configured) n_cfg++;
  }
  s.device_count = n_cfg;

  s.softap_active = softap_active();
  const char* ip = softap_ip();
  if (s.softap_active && ip && ip[0]) {
    strncpy(s.softap_ip, ip, sizeof(s.softap_ip) - 1);
  } else {
    strncpy(s.softap_ip, "192.168.4.1", sizeof(s.softap_ip) - 1);
  }
  s.softap_ip[sizeof(s.softap_ip) - 1] = '\0';

  poll_board_power();
  s.board_present = g_board_power.present;
  s.power_on_usb = true;
  s.board_voltage_valid = false;
  s.board_mV = 0;
  s.board_soc_valid = false;
  s.board_soc_pct = 0;
  if (g_board_power.present && g_board_power.ok) {
    s.board_voltage_valid = g_board_power.voltage_valid;
    s.board_mV = g_board_power.mV;
    s.board_soc_valid = g_board_power.soc_valid;
    s.board_soc_pct = g_board_power.soc_pct;
    s.power_on_usb = g_board_power.on_usb;
  }

  strncpy(s.version, VICTRONDASH_VERSION, sizeof(s.version) - 1);
  s.version[sizeof(s.version) - 1] = '\0';
  return s;
}

uint32_t page_nav_accent(const PageSlot& page) {
  // Match g_page_accent[] in dashboard.cpp.
  switch (page.kind) {
    case kPageSolar:
      return 0xFFB300;
    case kPageOrion:
      return 0xFF9100;
    case kPageInfo:
      return 0xB388FF;
    case kPageBattery:
    default:
      if (page.device_index < kMaxDevices &&
          victron_model_is_battery_sense(g_rt[page.device_index].last_model_id)) {
        return 0x1DE9B6;  // Sense mint
      }
      return 0x00BCD4;  // Battery cyan
  }
}

void update_cabin_ui(bool force) {
  if (softap_active()) return;

#if VICTRONDASH_SKIP_CABIN_PAINT
  (void)force;
  g_ui_dirty = false;
  g_ui_ready = true;
  return;
#endif

  diag_set_stage(kDiagUiEnter);
  static uint32_t s_ui_mem = 0;
  const bool mem_log = s_ui_mem < 24;
  if (mem_log) {
    s_ui_mem++;
    diag_print_mem("UI_IN");
  }

  if (!ble_scan_have_key() || g_page_count == 0) {
    if (!g_ui_ready || force) {
      dashboard_show_needs_setup();
      g_ui_ready = true;
      g_setup_ui = false;
    }
    diag_set_stage(kDiagUiLeave);
    if (mem_log) diag_print_mem("UI_OUT");
    return;
  }

  const PageSlot& page = g_pages[g_page_index];
  const bool do_force = force || !g_ui_ready || g_ui_dirty;
  if (page.kind == kPageBattery) {
    dashboard_update_battery(snapshot_battery(page.device_index), do_force);
  } else if (page.kind == kPageSolar) {
    dashboard_update_solar(snapshot_solar(page.device_index), do_force);
  } else if (page.kind == kPageOrion) {
    dashboard_update_orion(snapshot_orion(page.device_index), do_force);
  } else {
    dashboard_update_info(snapshot_info(), do_force);
  }
  dashboard_set_page_nav((uint8_t)g_page_index, (uint8_t)g_page_count, page_nav_accent(page));
  g_ui_ready = true;
  g_ui_dirty = false;
  g_setup_ui = false;
  diag_set_stage(kDiagUiLeave);
  if (mem_log) diag_print_mem("UI_OUT");
}

void try_decode_device(size_t di) {
  VictronAdvert adv;
  if (!ble_scan_latest_for(di, &adv)) return;

  DeviceRuntime& rt = g_rt[di];
  rt.rssi = adv.rssi;
  rt.last_record_type = adv.record_type;
  rt.last_model_id = adv.model_id;

  if (!adv.key_configured) return;
  if (!adv.key_check_ok) {
    rt.key_fail = true;
    return;
  }
  rt.key_fail = false;

  if ((uint32_t)adv.nonce == rt.last_nonce &&
      (rt.have_batt || rt.have_solar || rt.have_dcdc)) {
    return;
  }

  uint8_t key[16];
  if (!ble_scan_get_key_for(di, key)) return;

  uint8_t plain[32];
  size_t plain_len = 0;
  if (!victron_ir_decrypt(adv.mfr, adv.mfr_len, key, plain, sizeof(plain), &plain_len)) {
    return;
  }

  bool decoded = false;
  if (adv.record_type == 0x02) {
    BatteryMonitorState batt = {};
    if (victron_parse_battery_monitor(plain, plain_len, &batt)) {
      // Battery Sense reuses record 0x02 but has no shunt current / SoC — BMV bit
      // layout leaves garbage in those fields (e.g. +2097 A). Keep V + aux temp.
      if (victron_model_is_battery_sense(adv.model_id)) {
        batt.current_valid = false;
        batt.soc_valid = false;
        batt.current_a = 0;
        batt.soc_pct = 0;
        batt.power_w = 0;
      }
      batt.updated_ms = adv.seen_ms;
      rt.batt = batt;
      rt.have_batt = true;
      decoded = true;
      if (Serial) {
        if (victron_model_is_battery_sense(adv.model_id)) {
          if (batt.temp_valid) {
            Serial.printf("DECODE[%u] SENSE V=%0.2f T=%0.1fC\n", (unsigned)di,
                          (double)batt.voltage_v, (double)batt.temp_c);
          } else {
            Serial.printf("DECODE[%u] SENSE V=%0.2f\n", (unsigned)di, (double)batt.voltage_v);
          }
        } else {
          Serial.printf("DECODE[%u] BATT V=%0.2f I=%+0.2f SoC=%0.1f%%\n", (unsigned)di,
                        (double)batt.voltage_v, (double)batt.current_a, (double)batt.soc_pct);
        }
      }
    }
  } else if (adv.record_type == 0x01) {
    SolarChargerState solar = {};
    if (victron_parse_solar_charger(plain, plain_len, &solar)) {
      solar.updated_ms = adv.seen_ms;
      rt.solar = solar;
      rt.have_solar = true;
      decoded = true;
      if (Serial) {
        Serial.printf("DECODE[%u] SOLAR W=%0.0f V=%0.2f\n", (unsigned)di, (double)solar.solar_w,
                      (double)solar.battery_v);
      }
    }
  } else if (adv.record_type == 0x04) {
    DcdcConverterState dcdc = {};
    if (victron_parse_dcdc_converter(plain, plain_len, &dcdc)) {
      dcdc.updated_ms = adv.seen_ms;
      rt.dcdc = dcdc;
      rt.have_dcdc = true;
      decoded = true;
      if (Serial) {
        Serial.printf("DECODE[%u] DCDC out=%0.2f in=%0.2f\n", (unsigned)di,
                      (double)dcdc.output_v, (double)dcdc.input_v);
      }
    }
  }

  if (!decoded) return;
  rt.last_nonce = adv.nonce;
  g_ui_dirty = true;
  diag_set_stage(kDiagDecode);
  diag_print_mem("DECODE");

  // Auto-typed devices may gain a page once record_type is known.
  if (di < g_devices.count && g_devices.devices[di].type == kDeviceTypeAuto) {
    rebuild_pages();
  }
}

void try_decode_all(void) {
  for (size_t i = 0; i < g_devices.count; i++) {
    if (g_devices.devices[i].configured) {
      try_decode_device(i);
    }
  }
}

void enter_softap(void) {
  if (softap_active()) return;

  // SoftAP pauses BLE — fill the nearby cache first so the shunt can appear.
  dashboard_show_message("SCANNING BLE", 0x00E676);
  Serial.printf("SoftAP pre-listen (heap %u maxblk %u)\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());
  const uint32_t listen_until = millis() + 7000;
  while (millis() < listen_until) {
    diag_wdt_feed();
    ble_scan_loop();
    lvgl_port_handler();
    delay(10);
  }
  {
    BleNearbyDevice near[kBleNearbyMax];
    const size_t n = ble_scan_nearby(near, kBleNearbyMax);
    Serial.printf("SoftAP nearby cached: %u device(s)\n", (unsigned)n);
  }

  Serial.printf("Entering SoftAP setup (free heap %u)\n", (unsigned)ESP.getFreeHeap());
  if (!softap_start()) {
    dashboard_show_message("WIFI FAILED", 0xFF1744);
    g_setup_ui = false;
    g_ui_ready = true;
    return;
  }
  dashboard_show_setup(softap_ip());
  g_setup_ui = true;
  g_ui_ready = false;
  // SoftAP loop skips LVGL — flush the setup screen once now.
  for (int i = 0; i < 10; i++) {
    diag_wdt_feed();
    lvgl_port_handler();
    delay(5);
  }
  // Free DMA draw buffer for the SoftAP session (HTTP/Wi‑Fi need contiguous heap).
  lvgl_port_suspend_draw_buf();
}

void leave_softap(void) {
  if (!softap_active()) return;

  Serial.println("SoftAP leave");
  // Tear down Wi‑Fi / resume BLE before a full cabin redraw.
  softap_stop();
  softap_clear_stop_request();
  if (softap_config_changed()) {
    softap_clear_config_changed();
  }
  reload_device_config();

  if (!lvgl_port_resume_draw_buf()) {
    Serial.println("LVGL resume failed — restarting to reclaim heap");
    delay(200);
    ESP.restart();
  }

  g_setup_ui = false;
  g_ui_ready = false;
  dashboard_show_message("LEAVING SETUP", 0x00E676);
  Serial.printf("SoftAP leave settle (heap %u maxblk %u)\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());

  // Let Wi‑Fi teardown + BLE resume finish before a full cabin invalidate.
  const uint32_t settle_until = millis() + 800;
  while (millis() < settle_until) {
    diag_wdt_feed();
    lvgl_port_handler();
    ble_scan_loop();
    delay(10);
  }

  update_cabin_ui(true);
  Serial.println("SoftAP leave → cabin UI armed");
}

void page_next(void) {
  if (g_page_count <= 1) return;
  g_page_index = (g_page_index + 1) % g_page_count;
  g_ui_dirty = true;
  Serial.printf("Page %u/%u kind=%u dev=%u\n", (unsigned)(g_page_index + 1),
                (unsigned)g_page_count, (unsigned)g_pages[g_page_index].kind,
                (unsigned)g_pages[g_page_index].device_index);
  update_cabin_ui(true);
}

void page_prev(void) {
  if (g_page_count <= 1) return;
  g_page_index = (g_page_index + g_page_count - 1) % g_page_count;
  g_ui_dirty = true;
  Serial.printf("Page %u/%u kind=%u dev=%u\n", (unsigned)(g_page_index + 1),
                (unsigned)g_page_count, (unsigned)g_pages[g_page_index].kind,
                (unsigned)g_pages[g_page_index].device_index);
  update_cabin_ui(true);
}

void poll_buttons(void) {
  static bool was0 = false;
  static bool was28 = false;
  static uint32_t down0_ms = 0;
  static uint32_t both_ms = 0;
  static bool long0_fired = false;
  static bool soft_off_fired = false;

  const bool down0 = digitalRead(BUTTON_PIN) == LOW;
  const bool down28 = digitalRead(BUTTON_BOOT) == LOW;
  const uint32_t now = millis();

  // Dual-hold soft power-off (both buttons). Suppresses SoftAP long-press.
  if (down0 && down28) {
    if (both_ms == 0) {
      both_ms = now;
      soft_off_fired = false;
    }
    if (!soft_off_fired && (now - both_ms) >= kSoftOffHoldMs) {
      soft_off_fired = true;
      long0_fired = true;  // consume GPIO0 long so SoftAP does not fire on release
      if (softap_ota_active()) {
        Serial.println("Soft power ignored — OTA active");
      } else {
        soft_power_off();
      }
    }
  } else {
    both_ms = 0;
  }

  if (down0 && !was0) {
    down0_ms = now;
    long0_fired = false;
  }

  // SoftAP long-press: GPIO0 alone only (not while BOOT is also held).
  if (down0 && !down28 && !long0_fired && (now - down0_ms) >= kLongPressMs) {
    long0_fired = true;
    if (softap_active()) {
      Serial.println("GPIO0 long → leave SoftAP");
      leave_softap();
    } else {
      Serial.println("GPIO0 long → enter SoftAP");
      enter_softap();
    }
  }

  if (!down0 && was0 && !long0_fired) {
    if (!softap_active()) {
      page_next();
    }
  }

  if (down28 && !was28 && !softap_active()) {
    page_prev();
  }

  was0 = down0;
  was28 = down28;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(300);
  if (Serial) {
    Serial.println();
    Serial.println("VictronDash — multi-device Instant Readout (LVGL)");
#if VICTRONDASH_SKIP_CABIN_PAINT
    Serial.println("NOTE: VICTRONDASH_SKIP_CABIN_PAINT=1 (decode only, no cabin paint)");
#endif
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_BOOT, INPUT_PULLUP);

  // GPIO wake: drop out before lighting the panel if they already let go.
  abort_gpio_wake_if_released(false);

  if (!display_init()) {
    while (true) delay(1000);
  }

  device_config_begin();
  if (device_config_wiped_for_new_fw() && Serial) {
    Serial.println("First-boot style: paste MAC + key via SoftAP");
  }
  {
    const uint8_t orient = device_config_get_orient();
    display_reconfigure(orient);
  }

  if (!lvgl_port_init()) {
    Serial.println("LVGL init failed — halt");
    while (true) delay(1000);
  }

  diag_begin();

  // Still holding? Pocket bump ends here. Once the owl is up, let-go stays awake.
  abort_gpio_wake_if_released(true);
  splash_show();
  Serial.printf("Splash (wake cause %d)\n", (int)esp_sleep_get_wakeup_cause());
  {
    const uint32_t start = millis();
    while ((millis() - start) < kWakeConfirmMs) {
      lvgl_port_handler();
      delay(10);
    }
  }

  if (!dashboard_init()) {
    Serial.println("Dashboard init failed — halt");
    while (true) delay(1000);
  }

#if USE_TOUCH_SWIPE
  touch_init();
  touch_set_nav_flip(device_config_get_orient() == kOrientFlip);
#endif

  reload_device_config();

  if (!ble_scan_start()) {
    dashboard_show_message("BLE FAILED", 0xFF1744);
    return;
  }

  // SoftAP OTA dual-slot: confirm this image after a healthy boot so rollback
  // can reclaim the previous slot if a bad update never reaches here.
  {
    const esp_err_t ota_ok = esp_ota_mark_app_valid_cancel_rollback();
    if (ota_ok != ESP_OK && Serial) {
      Serial.printf("OTA mark valid: %s\n", esp_err_to_name(ota_ok));
    }
  }

  if (g_devices.count == 0 || !ble_scan_have_key()) {
    // enter_softap() does its own BLE pre-listen before starting Wi‑Fi.
    enter_softap();
    return;
  }

  g_ui_ready = false;
  update_cabin_ui(true);
}

void loop() {
  static uint32_t last_ui_ms = 0;
  static uint32_t last_hb_ms = 0;

  diag_wdt_feed();
  diag_set_stage(kDiagBtn);
  poll_buttons();
  maybe_lipo_empty_sleep();
#if USE_TOUCH_SWIPE
  {
    const TouchNav nav = touch_poll();
    if (!softap_active()) {
      if (nav == kTouchNavNext) {
        page_next();
      } else if (nav == kTouchNavPrev) {
        page_prev();
      }
    }
  }
#endif

  if (softap_active()) {
    // Skip LVGL while SoftAP is up — frees CPU/heap for HTTP/DNS.
    diag_set_stage(kDiagSoftAp);
    softap_loop();
    if (softap_stop_requested()) {
      Serial.println("SoftAP /stop requested");
      leave_softap();
    } else if (softap_config_changed()) {
      softap_clear_config_changed();
      reload_device_config();
      // Setup overlay already on-screen; draw buf is suspended during SoftAP.
      if (!g_setup_ui) {
        g_setup_ui = true;
      }
    }
    ble_scan_loop();
    delay(2);
    return;
  }

  lvgl_port_handler();
  diag_wdt_feed();

  diag_set_stage(kDiagBle);
  ble_scan_loop();
  try_decode_all();

  const uint32_t now = millis();
  if ((now - last_ui_ms) >= 250 || g_ui_dirty) {
    last_ui_ms = now;
    update_cabin_ui(false);
  }

  if ((now - last_hb_ms) >= 2000) {
    last_hb_ms = now;
    Serial.printf("hb hits=%u pages=%u page=%u\n", (unsigned)ble_scan_hit_count(),
                  (unsigned)g_page_count, (unsigned)g_page_index);
#if USE_TOUCH_SWIPE
    touch_print_status();
#endif
    diag_print_mem("hb");
  }

  diag_set_stage(kDiagIdle);
  delay(5);
}
