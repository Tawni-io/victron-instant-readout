#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

constexpr size_t kDashNameMax = 32;

// Cabin Battery / Solar / Orion / Info / Setup screens on LVGL (Montserrat).
struct DashSnapshot {
  bool valid;
  bool voltage_valid;
  bool current_valid;
  bool soc_valid;
  bool temp_valid;
  bool sense_mode;  // Smart Battery Sense: V + temp only
  int voltage_cV;   // centivolts
  int current_dA;   // deciamps
  int soc_pct;
  int temp_dC;  // decidegrees Celsius (×10)
  int power_w;
  uint16_t alarm;
  uint8_t status;  // 0 scan, 1 ble, 2 stale, 3 no config, 4 bad key
  int age_s;
  int rssi;
  char name[kDashNameMax];
};

struct SolarSnapshot {
  bool valid;
  bool voltage_valid;
  bool current_valid;
  bool power_valid;
  bool state_valid;
  int battery_cV;
  int battery_dA;
  int solar_w;
  uint8_t charge_state;
  uint8_t status;  // same codes as DashSnapshot
  int age_s;
  int rssi;
  char name[kDashNameMax];
  char state_label[16];
};

struct OrionSnapshot {
  bool valid;
  bool input_valid;
  bool output_valid;
  bool state_valid;
  int input_cV;
  int output_cV;
  uint8_t device_state;
  uint8_t status;
  int age_s;
  int rssi;
  char name[kDashNameMax];
  char state_label[16];
};

struct InfoSnapshot {
  uint8_t device_count;
  bool softap_active;
  char softap_ip[16];  // always filled (live or 192.168.4.1)
  bool board_present;
  bool power_on_usb;  // true = operating from USB; false = from LiPo
  bool board_voltage_valid;
  int board_mV;
  bool board_soc_valid;
  int board_soc_pct;
  char version[16];
};

bool dashboard_init(void);
void dashboard_show_message(const char* title, uint32_t color_hex);
void dashboard_show_needs_setup(void);
void dashboard_show_setup(const char* ip_or_null);
// Footer carousel dots on cabin screens. Hidden when count <= 1.
void dashboard_set_page_nav(uint8_t index, uint8_t count, uint32_t accent_hex);
void dashboard_update_battery(const DashSnapshot& s, bool force);
void dashboard_update_solar(const SolarSnapshot& s, bool force);
void dashboard_update_orion(const OrionSnapshot& s, bool force);
void dashboard_update_info(const InfoSnapshot& s, bool force);
