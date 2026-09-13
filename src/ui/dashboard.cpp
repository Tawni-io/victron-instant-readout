#include "ui/dashboard.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#include "board_pins.h"
#include "ui/lvgl_port.h"

namespace {

constexpr uint32_t kBg = 0x0D1117;
constexpr uint32_t kCard = 0x161B22;
constexpr uint32_t kDim = 0x30363D;
constexpr uint32_t kFg = 0xFFFFFF;
constexpr uint32_t kMuted = 0x8B949E;
constexpr uint32_t kOk = 0x00E676;
constexpr uint32_t kWarn = 0xFFD600;
constexpr uint32_t kAlarm = 0xFF1744;
constexpr uint32_t kCyan = 0x00BCD4;
constexpr uint32_t kOrange = 0xFF9100;

// Page identity accents (not status). SoftAP may overwrite later.
enum PageAccentId : uint8_t {
  kAccentBattery = 0,
  kAccentSense,
  kAccentSolar,
  kAccentOrion,
  kAccentInfo,
  kAccentCount
};

uint32_t g_page_accent[kAccentCount] = {
    0x00BCD4,  // Battery cyan
    0x1DE9B6,  // Sense mint
    0xFFB300,  // Solar amber
    0xFF9100,  // Orion orange
    0xB388FF,  // Info violet
};

uint32_t page_accent(PageAccentId id) {
  if (id >= kAccentCount) return kCyan;
  return g_page_accent[id];
}

lv_obj_t* scr_msg = nullptr;
lv_obj_t* scr_setup = nullptr;
lv_obj_t* scr_needs = nullptr;
lv_obj_t* scr_batt = nullptr;
lv_obj_t* scr_solar = nullptr;
lv_obj_t* scr_orion = nullptr;
lv_obj_t* scr_info = nullptr;

lv_obj_t* msg_title = nullptr;
lv_obj_t* setup_ip = nullptr;

lv_obj_t* batt_top_strip = nullptr;
lv_obj_t* batt_family = nullptr;
lv_obj_t* hdr_name = nullptr;
lv_obj_t* hdr_status_pill = nullptr;
lv_obj_t* hdr_status_dot = nullptr;
lv_obj_t* hdr_status_lbl = nullptr;
lv_obj_t* alarm_banner = nullptr;
lv_obj_t* volt_lbl = nullptr;
lv_obj_t* volt_unit = nullptr;
lv_obj_t* card_status = nullptr;
lv_obj_t* card_amps = nullptr;
lv_obj_t* card_watts = nullptr;
lv_obj_t* card_status_val = nullptr;
lv_obj_t* card_status_accent = nullptr;
lv_obj_t* card_amps_accent = nullptr;
lv_obj_t* card_amps_val = nullptr;
lv_obj_t* card_amps_unit = nullptr;
lv_obj_t* card_watts_val = nullptr;
lv_obj_t* soc_bar = nullptr;
lv_obj_t* soc_pct = nullptr;
lv_obj_t* foot_left = nullptr;
lv_obj_t* foot_right = nullptr;

lv_obj_t* sol_family = nullptr;
lv_obj_t* sol_name = nullptr;
lv_obj_t* sol_status_dot = nullptr;
lv_obj_t* sol_status_lbl = nullptr;
lv_obj_t* sol_status_pill = nullptr;
lv_obj_t* sol_power = nullptr;
lv_obj_t* sol_power_unit = nullptr;
lv_obj_t* sol_state = nullptr;
lv_obj_t* sol_batt_v = nullptr;
lv_obj_t* sol_batt_a = nullptr;
lv_obj_t* sol_foot_l = nullptr;
lv_obj_t* sol_foot_r = nullptr;

lv_obj_t* ori_family = nullptr;
lv_obj_t* ori_name = nullptr;
lv_obj_t* ori_status_dot = nullptr;
lv_obj_t* ori_status_lbl = nullptr;
lv_obj_t* ori_status_pill = nullptr;
lv_obj_t* ori_mode = nullptr;
lv_obj_t* ori_out_v = nullptr;
lv_obj_t* ori_in_v = nullptr;
lv_obj_t* ori_foot_l = nullptr;
lv_obj_t* ori_foot_r = nullptr;

lv_obj_t* info_title = nullptr;
lv_obj_t* info_version = nullptr;
lv_obj_t* info_dev_title = nullptr;
lv_obj_t* info_dev_line = nullptr;
lv_obj_t* info_power_status = nullptr;
lv_obj_t* info_power_detail = nullptr;
lv_obj_t* info_setup_status = nullptr;
lv_obj_t* info_setup_detail = nullptr;

// Footer page dots (matches main g_pages capacity: kMaxDevices + 2).
constexpr size_t kPageNavMax = 10;
constexpr int kPageDotH = 5;
constexpr int kPageDotInactiveW = 5;
constexpr int kPageDotActiveW = 10;
constexpr int kPageDotGap = 5;

struct PageNav {
  lv_obj_t* root = nullptr;
  lv_obj_t* dots[kPageNavMax] = {};
};

PageNav nav_batt = {};
PageNav nav_solar = {};
PageNav nav_orion = {};
PageNav nav_info = {};

DashSnapshot g_drawn = {};
SolarSnapshot g_drawn_solar = {};
OrionSnapshot g_drawn_orion = {};
InfoSnapshot g_drawn_info = {};
bool g_batt_built = false;
bool g_solar_built = false;
bool g_orion_built = false;
bool g_info_built = false;

lv_obj_t* make_screen(void) {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, lv_color_hex(kBg), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  return scr;
}

lv_obj_t* make_top_strip(lv_obj_t* parent, uint32_t accent) {
  lv_obj_t* strip = lv_obj_create(parent);
  lv_obj_set_pos(strip, 0, 0);
  lv_obj_set_size(strip, DISPLAY_WIDTH, 3);
  lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(strip, 0, 0);
  lv_obj_set_style_bg_color(strip, lv_color_hex(accent), 0);
  lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(strip, 0, 0);
  lv_obj_set_style_pad_all(strip, 0, 0);
  return strip;
}

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  return lbl;
}

lv_obj_t* make_card(lv_obj_t* parent, int x, int y, int w, int h, uint32_t accent,
                    lv_obj_t** accent_out, lv_obj_t** value_out, lv_obj_t** unit_out,
                    const char* unit, uint32_t unit_color) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  // Radius 0: rounded corners allocate LVGL layers and can wedge on 32–64KB pools.
  lv_obj_set_style_radius(card, 0, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 0, 0);

  lv_obj_t* accent_bar = lv_obj_create(card);
  lv_obj_set_pos(accent_bar, 0, 0);
  lv_obj_set_size(accent_bar, w, 3);
  lv_obj_remove_flag(accent_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(accent_bar, 0, 0);
  lv_obj_set_style_bg_color(accent_bar, lv_color_hex(accent), 0);
  lv_obj_set_style_bg_opa(accent_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(accent_bar, 0, 0);
  lv_obj_set_style_pad_all(accent_bar, 0, 0);

  lv_obj_t* value = make_label(card, &lv_font_montserrat_16, kFg);
  lv_obj_align(value, LV_ALIGN_TOP_LEFT, 6, 8);
  lv_label_set_text(value, "--");

  lv_obj_t* unit_lbl = make_label(card, &lv_font_montserrat_12, unit_color);
  lv_obj_align(unit_lbl, LV_ALIGN_BOTTOM_LEFT, 6, -4);
  lv_label_set_text(unit_lbl, unit);

  if (accent_out) *accent_out = accent_bar;
  if (value_out) *value_out = value;
  if (unit_out) *unit_out = unit_lbl;
  return card;
}

void make_status_pill(lv_obj_t* parent, lv_obj_t** pill, lv_obj_t** dot, lv_obj_t** lbl) {
  *pill = lv_obj_create(parent);
  lv_obj_set_size(*pill, 70, 20);
  lv_obj_align(*pill, LV_ALIGN_TOP_RIGHT, -8, 5);
  lv_obj_remove_flag(*pill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(*pill, 0, 0);
  lv_obj_set_style_bg_color(*pill, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(*pill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(*pill, 0, 0);
  lv_obj_set_style_pad_hor(*pill, 6, 0);
  lv_obj_set_style_pad_ver(*pill, 2, 0);

  // Absolute layout — avoid flex (extra layout/draw cost on cabin refresh).
  *dot = lv_obj_create(*pill);
  lv_obj_set_size(*dot, 6, 6);
  lv_obj_set_pos(*dot, 4, 7);
  lv_obj_remove_flag(*dot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(*dot, 0, 0);
  lv_obj_set_style_bg_color(*dot, lv_color_hex(kWarn), 0);
  lv_obj_set_style_bg_opa(*dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(*dot, 0, 0);
  lv_obj_set_style_pad_all(*dot, 0, 0);

  *lbl = make_label(*pill, &lv_font_montserrat_12, kFg);
  lv_obj_set_pos(*lbl, 14, 2);
  lv_label_set_text(*lbl, "SCAN");
}

void make_page_nav(lv_obj_t* parent, PageNav* nav) {
  if (!parent || !nav) return;
  constexpr int max_w =
      kPageNavMax * kPageDotActiveW + (int)(kPageNavMax - 1) * kPageDotGap;
  nav->root = lv_obj_create(parent);
  lv_obj_set_size(nav->root, max_w, kPageDotH);
  lv_obj_align(nav->root, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_remove_flag(nav->root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(nav->root, 0, 0);
  lv_obj_set_style_bg_opa(nav->root, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(nav->root, 0, 0);
  lv_obj_set_style_pad_all(nav->root, 0, 0);
  lv_obj_add_flag(nav->root, LV_OBJ_FLAG_HIDDEN);

  for (size_t i = 0; i < kPageNavMax; i++) {
    nav->dots[i] = lv_obj_create(nav->root);
    lv_obj_set_size(nav->dots[i], kPageDotInactiveW, kPageDotH);
    lv_obj_set_pos(nav->dots[i], 0, 0);
    lv_obj_remove_flag(nav->dots[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(nav->dots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(nav->dots[i], lv_color_hex(kDim), 0);
    lv_obj_set_style_bg_opa(nav->dots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nav->dots[i], 0, 0);
    lv_obj_set_style_pad_all(nav->dots[i], 0, 0);
    lv_obj_add_flag(nav->dots[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void apply_page_nav(PageNav* nav, uint8_t index, uint8_t count, uint32_t accent_hex) {
  if (!nav || !nav->root) return;
  if (count <= 1 || count > kPageNavMax) {
    lv_obj_add_flag(nav->root, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (index >= count) index = 0;

  int total_w = 0;
  for (uint8_t i = 0; i < count; i++) {
    total_w += (i == index) ? kPageDotActiveW : kPageDotInactiveW;
    if (i + 1 < count) total_w += kPageDotGap;
  }
  lv_obj_set_width(nav->root, total_w);
  lv_obj_align(nav->root, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_remove_flag(nav->root, LV_OBJ_FLAG_HIDDEN);

  int x = 0;
  for (size_t i = 0; i < kPageNavMax; i++) {
    lv_obj_t* dot = nav->dots[i];
    if (!dot) continue;
    if (i >= count) {
      lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    const bool active = ((uint8_t)i == index);
    const int w = active ? kPageDotActiveW : kPageDotInactiveW;
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(dot, w, kPageDotH);
    lv_obj_set_pos(dot, x, 0);
    lv_obj_set_style_radius(dot, active ? (kPageDotH / 2) : LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(active ? accent_hex : kDim), 0);
    x += w + kPageDotGap;
  }
}

void apply_status_pill(lv_obj_t* pill, lv_obj_t* dot, lv_obj_t* lbl, uint8_t status) {
  const char* st = "SCAN";
  uint32_t st_col = kWarn;
  if (status == 1) {
    st = "BLE";
    st_col = kOk;
  } else if (status == 2) {
    st = "STALE";
    st_col = kWarn;
  } else if (status == 4) {
    st = "BAD KEY";
    st_col = kAlarm;
  }
  lv_label_set_text(lbl, st);
  lv_obj_set_style_bg_color(dot, lv_color_hex(st_col), 0);
  lv_obj_set_width(pill, (status == 4) ? 86 : 70);
}

uint32_t soc_color(int pct) {
  if (pct >= 60) return kOk;
  if (pct >= 20) return kWarn;
  return kAlarm;
}

const char* direction_label(float amps) {
  if (!isfinite(amps) || fabsf(amps) < 0.1f) return "IDLE";
  return (amps > 0.0f) ? "CHARGE" : "DISCH.";
}

uint32_t direction_color(float amps) {
  if (!isfinite(amps) || fabsf(amps) < 0.1f) return kFg;
  return (amps > 0.0f) ? kOk : kWarn;
}

void set_name_label(lv_obj_t* lbl, const char* name, const char* fallback) {
  if (!lbl) return;
  if (name && name[0]) {
    lv_label_set_text(lbl, name);
  } else {
    lv_label_set_text(lbl, fallback ? fallback : "DEVICE");
  }
}

void load_screen(lv_obj_t* scr) {
  if (!scr) return;
  if (lv_screen_active() != scr) {
    lv_screen_load(scr);
  }
  lv_obj_invalidate(scr);
}

void build_message_screen(void) {
  scr_msg = make_screen();
  msg_title = make_label(scr_msg, &lv_font_montserrat_20, kFg);
  lv_obj_align(msg_title, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(msg_title, "");
}

void build_needs_screen(void) {
  scr_needs = make_screen();
  lv_obj_t* card = lv_obj_create(scr_needs);
  lv_obj_set_pos(card, 8, 8);
  lv_obj_set_size(card, DISPLAY_WIDTH - 16, DISPLAY_HEIGHT - 16);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(card, 0, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 12, 0);

  lv_obj_t* t = make_label(card, &lv_font_montserrat_20, kWarn);
  lv_label_set_text(t, "SETUP NEEDED");
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 8);

  const char* lines[] = {
      "Wi-Fi starts automatically",
      "Join VictronDash",
      "Paste Instant Readout key",
      "from VictronConnect",
  };
  for (int i = 0; i < 4; i++) {
    lv_obj_t* l = make_label(card, &lv_font_montserrat_14, i == 0 ? kFg : kMuted);
    lv_label_set_text(l, lines[i]);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 48 + i * 22);
  }
}

void build_setup_screen(void) {
  scr_setup = make_screen();
  lv_obj_t* card = lv_obj_create(scr_setup);
  lv_obj_set_pos(card, 8, 8);
  lv_obj_set_size(card, DISPLAY_WIDTH - 16, DISPLAY_HEIGHT - 16);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(card, 0, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 12, 0);

  lv_obj_t* t = make_label(card, &lv_font_montserrat_20, kOk);
  lv_label_set_text(t, "SETUP MODE");
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 4);

  lv_obj_t* wifi = make_label(card, &lv_font_montserrat_14, kFg);
  lv_label_set_text(wifi, "Wi-Fi: VictronDash");
  lv_obj_align(wifi, LV_ALIGN_TOP_LEFT, 0, 36);

  setup_ip = make_label(card, &lv_font_montserrat_14, kCyan);
  lv_label_set_text(setup_ip, "http://192.168.4.1");
  lv_obj_align(setup_ip, LV_ALIGN_TOP_LEFT, 0, 56);

  const char* tips[] = {
      "Join on your phone",
      "Open that address",
      "Hold btn / Stop to finish",
  };
  for (int i = 0; i < 3; i++) {
    lv_obj_t* l = make_label(card, &lv_font_montserrat_12, kMuted);
    lv_label_set_text(l, tips[i]);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 88 + i * 18);
  }
}

void build_battery_screen(void) {
  scr_batt = make_screen();
  batt_top_strip = make_top_strip(scr_batt, page_accent(kAccentBattery));

  const uint32_t batt_acc = page_accent(kAccentBattery);
  batt_family = make_label(scr_batt, &lv_font_montserrat_12, batt_acc);
  lv_obj_align(batt_family, LV_ALIGN_TOP_LEFT, 10, 6);
  lv_label_set_text(batt_family, "SHUNT");

  hdr_name = make_label(scr_batt, &lv_font_montserrat_12, kMuted);
  lv_obj_align(hdr_name, LV_ALIGN_TOP_LEFT, 70, 6);
  lv_label_set_text(hdr_name, "BATTERY");
  lv_label_set_long_mode(hdr_name, LV_LABEL_LONG_DOT);
  lv_obj_set_width(hdr_name, 150);

  make_status_pill(scr_batt, &hdr_status_pill, &hdr_status_dot, &hdr_status_lbl);

  alarm_banner = lv_obj_create(scr_batt);
  lv_obj_set_pos(alarm_banner, 8, 28);
  lv_obj_set_size(alarm_banner, DISPLAY_WIDTH - 16, 16);
  lv_obj_remove_flag(alarm_banner, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(alarm_banner, 0, 0);
  lv_obj_set_style_bg_color(alarm_banner, lv_color_hex(kAlarm), 0);
  lv_obj_set_style_bg_opa(alarm_banner, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(alarm_banner, 0, 0);
  lv_obj_set_style_pad_all(alarm_banner, 0, 0);
  lv_obj_add_flag(alarm_banner, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t* alarm_lbl = make_label(alarm_banner, &lv_font_montserrat_12, kFg);
  lv_label_set_text(alarm_lbl, "ALARM");
  lv_obj_align(alarm_lbl, LV_ALIGN_LEFT_MID, 8, 0);

  volt_lbl = make_label(scr_batt, &lv_font_montserrat_40, kFg);
  lv_obj_align(volt_lbl, LV_ALIGN_TOP_LEFT, 12, 32);
  lv_label_set_text(volt_lbl, "--.--");

  volt_unit = make_label(scr_batt, &lv_font_montserrat_20, page_accent(kAccentBattery));
  lv_label_set_text(volt_unit, "V");
  lv_obj_align_to(volt_unit, volt_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -4);

  constexpr int card_y = 86;
  constexpr int card_h = 44;
  constexpr int card_w = 98;
  constexpr int gap = 6;
  constexpr int x0 = 10;

  card_status = make_card(scr_batt, x0, card_y, card_w, card_h, kDim, &card_status_accent,
                          &card_status_val, nullptr, "STATUS", kDim);
  card_amps = make_card(scr_batt, x0 + card_w + gap, card_y, card_w, card_h, kOrange,
                        &card_amps_accent, &card_amps_val, &card_amps_unit, "AMPS", kOrange);
  card_watts = make_card(scr_batt, x0 + 2 * (card_w + gap), card_y, card_w, card_h, kMuted, nullptr,
                         &card_watts_val, nullptr, "WATTS", kMuted);
  lv_obj_set_style_text_font(card_status_val, &lv_font_montserrat_14, 0);

  soc_bar = lv_bar_create(scr_batt);
  lv_obj_set_pos(soc_bar, 10, 138);
  lv_obj_set_size(soc_bar, 250, 14);
  lv_bar_set_range(soc_bar, 0, 100);
  lv_bar_set_value(soc_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_radius(soc_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(soc_bar, lv_color_hex(kDim), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(soc_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(soc_bar, 0, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(soc_bar, lv_color_hex(kOk), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(soc_bar, LV_OPA_COVER, LV_PART_INDICATOR);

  soc_pct = make_label(scr_batt, &lv_font_montserrat_14, kOk);
  lv_obj_align(soc_pct, LV_ALIGN_TOP_RIGHT, -10, 136);
  lv_label_set_text(soc_pct, "N/A");

  foot_left = make_label(scr_batt, &lv_font_montserrat_12, kMuted);
  lv_obj_align(foot_left, LV_ALIGN_BOTTOM_LEFT, 12, -4);
  lv_label_set_text(foot_left, "Waiting for shunt");

  foot_right = make_label(scr_batt, &lv_font_montserrat_12, kMuted);
  lv_obj_align(foot_right, LV_ALIGN_BOTTOM_RIGHT, -12, -4);
  lv_label_set_text(foot_right, "");

  make_page_nav(scr_batt, &nav_batt);

  g_batt_built = true;
}

void build_solar_screen(void) {
  scr_solar = make_screen();
  make_top_strip(scr_solar, page_accent(kAccentSolar));

  const uint32_t sol_acc = page_accent(kAccentSolar);
  sol_family = make_label(scr_solar, &lv_font_montserrat_12, sol_acc);
  lv_obj_align(sol_family, LV_ALIGN_TOP_LEFT, 10, 6);
  lv_label_set_text(sol_family, "SOLAR");

  sol_name = make_label(scr_solar, &lv_font_montserrat_12, kMuted);
  lv_obj_align(sol_name, LV_ALIGN_TOP_LEFT, 70, 6);
  lv_label_set_text(sol_name, "MPPT");
  lv_label_set_long_mode(sol_name, LV_LABEL_LONG_DOT);
  lv_obj_set_width(sol_name, 150);

  make_status_pill(scr_solar, &sol_status_pill, &sol_status_dot, &sol_status_lbl);

  sol_power = make_label(scr_solar, &lv_font_montserrat_40, kFg);
  lv_obj_align(sol_power, LV_ALIGN_TOP_LEFT, 12, 36);
  lv_label_set_text(sol_power, "---");

  sol_power_unit = make_label(scr_solar, &lv_font_montserrat_20, sol_acc);
  lv_label_set_text(sol_power_unit, "W");
  lv_obj_align_to(sol_power_unit, sol_power, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -4);

  sol_state = make_label(scr_solar, &lv_font_montserrat_16, kOk);
  lv_obj_align(sol_state, LV_ALIGN_TOP_LEFT, 12, 90);
  lv_label_set_text(sol_state, "----");

  sol_batt_v = make_label(scr_solar, &lv_font_montserrat_14, kMuted);
  lv_obj_align(sol_batt_v, LV_ALIGN_TOP_LEFT, 12, 118);
  lv_label_set_text(sol_batt_v, "Batt --.- V");

  sol_batt_a = make_label(scr_solar, &lv_font_montserrat_14, kMuted);
  lv_obj_align(sol_batt_a, LV_ALIGN_TOP_LEFT, 160, 118);
  lv_label_set_text(sol_batt_a, "--.- A");

  sol_foot_l = make_label(scr_solar, &lv_font_montserrat_12, kMuted);
  lv_obj_align(sol_foot_l, LV_ALIGN_BOTTOM_LEFT, 12, -4);
  lv_label_set_text(sol_foot_l, "Waiting for MPPT");

  sol_foot_r = make_label(scr_solar, &lv_font_montserrat_12, kMuted);
  lv_obj_align(sol_foot_r, LV_ALIGN_BOTTOM_RIGHT, -12, -4);
  lv_label_set_text(sol_foot_r, "");

  make_page_nav(scr_solar, &nav_solar);

  g_solar_built = true;
}

void build_orion_screen(void) {
  scr_orion = make_screen();
  make_top_strip(scr_orion, page_accent(kAccentOrion));

  const uint32_t ori_acc = page_accent(kAccentOrion);
  ori_family = make_label(scr_orion, &lv_font_montserrat_12, ori_acc);
  lv_obj_align(ori_family, LV_ALIGN_TOP_LEFT, 10, 6);
  lv_label_set_text(ori_family, "ORION");

  ori_name = make_label(scr_orion, &lv_font_montserrat_12, kMuted);
  lv_obj_align(ori_name, LV_ALIGN_TOP_LEFT, 70, 6);
  lv_label_set_text(ori_name, "DC-DC");
  lv_label_set_long_mode(ori_name, LV_LABEL_LONG_DOT);
  lv_obj_set_width(ori_name, 150);

  make_status_pill(scr_orion, &ori_status_pill, &ori_status_dot, &ori_status_lbl);

  ori_mode = make_label(scr_orion, &lv_font_montserrat_28, kFg);
  lv_obj_align(ori_mode, LV_ALIGN_TOP_LEFT, 12, 40);
  lv_label_set_text(ori_mode, "----");

  ori_out_v = make_label(scr_orion, &lv_font_montserrat_20, ori_acc);
  lv_obj_align(ori_out_v, LV_ALIGN_TOP_LEFT, 12, 90);
  lv_label_set_text(ori_out_v, "Out --.-- V");

  ori_in_v = make_label(scr_orion, &lv_font_montserrat_14, kMuted);
  lv_obj_align(ori_in_v, LV_ALIGN_TOP_LEFT, 12, 120);
  lv_label_set_text(ori_in_v, "In --.-- V");

  ori_foot_l = make_label(scr_orion, &lv_font_montserrat_12, kMuted);
  lv_obj_align(ori_foot_l, LV_ALIGN_BOTTOM_LEFT, 12, -4);
  lv_label_set_text(ori_foot_l, "Waiting for Orion");

  ori_foot_r = make_label(scr_orion, &lv_font_montserrat_12, kMuted);
  lv_obj_align(ori_foot_r, LV_ALIGN_BOTTOM_RIGHT, -12, -4);
  lv_label_set_text(ori_foot_r, "");

  make_page_nav(scr_orion, &nav_orion);

  g_orion_built = true;
}

lv_obj_t* make_info_panel(lv_obj_t* parent, int x, int y, int w, int h, uint32_t accent) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(card, 0, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 0, 0);

  lv_obj_t* bar = lv_obj_create(card);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_size(bar, w, 3);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(accent), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  return card;
}

void build_info_screen(void) {
  scr_info = make_screen();
  const uint32_t info_acc = page_accent(kAccentInfo);
  make_top_strip(scr_info, info_acc);

  info_title = make_label(scr_info, &lv_font_montserrat_16, info_acc);
  lv_label_set_text(info_title, "INFO");
  lv_obj_align(info_title, LV_ALIGN_TOP_LEFT, 12, 6);

  info_version = make_label(scr_info, &lv_font_montserrat_14, kMuted);
  lv_label_set_text(info_version, "v--");
  lv_obj_align(info_version, LV_ALIGN_TOP_RIGHT, -12, 8);

  constexpr int gap = 6;
  constexpr int x0 = 10;
  const int full_w = DISPLAY_WIDTH - 20;
  const int half_w = (full_w - gap) / 2;

  lv_obj_t* dev_card = make_info_panel(scr_info, x0, 28, full_w, 44, info_acc);
  info_dev_title = make_label(dev_card, &lv_font_montserrat_12, kMuted);
  lv_label_set_text(info_dev_title, "DEVICES");
  lv_obj_align(info_dev_title, LV_ALIGN_TOP_LEFT, 8, 8);
  info_dev_line = make_label(dev_card, &lv_font_montserrat_14, kFg);
  lv_label_set_text(info_dev_line, "None configured");
  lv_obj_align(info_dev_line, LV_ALIGN_TOP_LEFT, 8, 24);

  lv_obj_t* power_card = make_info_panel(scr_info, x0, 82, half_w, 70, info_acc);
  lv_obj_t* power_lbl = make_label(power_card, &lv_font_montserrat_12, kMuted);
  lv_label_set_text(power_lbl, "POWER");
  lv_obj_align(power_lbl, LV_ALIGN_TOP_LEFT, 8, 8);
  info_power_status = make_label(power_card, &lv_font_montserrat_16, kMuted);
  lv_label_set_text(info_power_status, "USB");
  lv_obj_align(info_power_status, LV_ALIGN_TOP_LEFT, 8, 26);
  info_power_detail = make_label(power_card, &lv_font_montserrat_12, kMuted);
  lv_label_set_text(info_power_detail, "");
  lv_label_set_long_mode(info_power_detail, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(info_power_detail, half_w - 16);
  lv_obj_align(info_power_detail, LV_ALIGN_TOP_LEFT, 8, 48);

  lv_obj_t* setup_card = make_info_panel(scr_info, x0 + half_w + gap, 82, half_w, 70, info_acc);
  lv_obj_t* setup_lbl = make_label(setup_card, &lv_font_montserrat_12, kMuted);
  lv_label_set_text(setup_lbl, "SETUP");
  lv_obj_align(setup_lbl, LV_ALIGN_TOP_LEFT, 8, 8);
  info_setup_status = make_label(setup_card, &lv_font_montserrat_16, kMuted);
  lv_label_set_text(info_setup_status, "Not active");
  lv_obj_align(info_setup_status, LV_ALIGN_TOP_LEFT, 8, 26);
  info_setup_detail = make_label(setup_card, &lv_font_montserrat_12, kMuted);
  lv_label_set_text(info_setup_detail, "192.168.4.1");
  lv_label_set_long_mode(info_setup_detail, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(info_setup_detail, half_w - 16);
  lv_obj_align(info_setup_detail, LV_ALIGN_TOP_LEFT, 8, 48);

  make_page_nav(scr_info, &nav_info);

  g_info_built = true;
}

}  // namespace

bool dashboard_init(void) {
  if (!lvgl_port_ready()) {
    return false;
  }
  build_message_screen();
  build_needs_screen();
  build_setup_screen();
  build_battery_screen();
  build_solar_screen();
  build_orion_screen();
  build_info_screen();
  return true;
}

void dashboard_show_message(const char* title, uint32_t color_hex) {
  if (!scr_msg) return;
  lv_label_set_text(msg_title, title ? title : "");
  lv_obj_set_style_text_color(msg_title, lv_color_hex(color_hex), 0);
  load_screen(scr_msg);
}

void dashboard_show_needs_setup(void) { load_screen(scr_needs); }

void dashboard_set_page_nav(uint8_t index, uint8_t count, uint32_t accent_hex) {
  apply_page_nav(&nav_batt, index, count, accent_hex);
  apply_page_nav(&nav_solar, index, count, accent_hex);
  apply_page_nav(&nav_orion, index, count, accent_hex);
  apply_page_nav(&nav_info, index, count, accent_hex);
}

void dashboard_show_setup(const char* ip_or_null) {
  char line[36];
  if (ip_or_null && ip_or_null[0]) {
    snprintf(line, sizeof(line), "http://%s", ip_or_null);
  } else {
    snprintf(line, sizeof(line), "http://192.168.4.1");
  }
  lv_label_set_text(setup_ip, line);
  load_screen(scr_setup);
}

void dashboard_update_battery(const DashSnapshot& s, bool force) {
  if (!g_batt_built) return;

  const bool screen_switch = force || lv_screen_active() != scr_batt;
  if (screen_switch) {
    load_screen(scr_batt);
  }

  const bool layout_flip = force || s.sense_mode != g_drawn.sense_mode;
  if (layout_flip) {
    const uint32_t batt_acc = page_accent(s.sense_mode ? kAccentSense : kAccentBattery);
    if (batt_top_strip) {
      lv_obj_set_style_bg_color(batt_top_strip, lv_color_hex(batt_acc), 0);
    }
    lv_obj_set_style_text_color(volt_unit, lv_color_hex(batt_acc), 0);

    if (s.sense_mode) {
      // Sense: hero V + wide TEMP; hide shunt SoC / amps / watts.
      lv_obj_add_flag(soc_bar, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(soc_pct, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(card_status, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(card_watts, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(card_amps, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(card_amps, 10, 86);
      lv_obj_set_size(card_amps, DISPLAY_WIDTH - 20, 44);
      if (card_amps_accent) lv_obj_set_width(card_amps_accent, DISPLAY_WIDTH - 20);
      lv_obj_set_style_bg_color(card_amps_accent, lv_color_hex(batt_acc), 0);
      lv_label_set_text(card_amps_unit, "TEMP");
      lv_obj_set_style_text_color(card_amps_unit, lv_color_hex(batt_acc), 0);
      lv_obj_set_style_text_font(card_amps_val, &lv_font_montserrat_20, 0);
      lv_label_set_text(batt_family, "SENSE");
      lv_obj_set_style_text_color(batt_family, lv_color_hex(batt_acc), 0);
    } else {
      lv_obj_remove_flag(soc_bar, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(soc_pct, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(card_status, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(card_watts, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(card_amps, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(card_amps, 10 + 98 + 6, 86);
      lv_obj_set_size(card_amps, 98, 44);
      if (card_amps_accent) lv_obj_set_width(card_amps_accent, 98);
      lv_obj_set_style_bg_color(card_amps_accent, lv_color_hex(kOrange), 0);
      lv_label_set_text(card_amps_unit, "AMPS");
      lv_obj_set_style_text_color(card_amps_unit, lv_color_hex(kOrange), 0);
      lv_obj_set_style_text_font(card_amps_val, &lv_font_montserrat_16, 0);
      lv_label_set_text(batt_family, "SHUNT");
      lv_obj_set_style_text_color(batt_family, lv_color_hex(batt_acc), 0);
    }
  }

  if (force || strcmp(s.name, g_drawn.name) != 0) {
    set_name_label(hdr_name, s.name, s.sense_mode ? "BATTERY SENSE" : "BATTERY");
  }

  const bool alarm_on = s.alarm != 0;
  if (force || alarm_on != (g_drawn.alarm != 0)) {
    if (alarm_on) {
      lv_obj_remove_flag(alarm_banner, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_y(volt_lbl, 48);
    } else {
      lv_obj_add_flag(alarm_banner, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_y(volt_lbl, 32);
    }
    lv_obj_align_to(volt_unit, volt_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -4);
  }

  if (!s.sense_mode &&
      (force || s.soc_valid != g_drawn.soc_valid || s.soc_pct != g_drawn.soc_pct)) {
    if (s.soc_valid) {
      char pct[8];
      snprintf(pct, sizeof(pct), "%d%%", s.soc_pct);
      const uint32_t c = soc_color(s.soc_pct);
      lv_obj_set_style_text_color(soc_pct, lv_color_hex(c), 0);
      lv_obj_set_style_bg_color(soc_bar, lv_color_hex(c), LV_PART_INDICATOR);
      lv_bar_set_value(soc_bar, s.soc_pct, LV_ANIM_OFF);
      lv_label_set_text(soc_pct, pct);
    } else {
      lv_obj_set_style_text_color(soc_pct, lv_color_hex(kWarn), 0);
      lv_bar_set_value(soc_bar, 0, LV_ANIM_OFF);
      lv_label_set_text(soc_pct, "N/A");
    }
  }

  if (force || s.status != g_drawn.status) {
    apply_status_pill(hdr_status_pill, hdr_status_dot, hdr_status_lbl, s.status);
  }

  if (force || s.voltage_cV != g_drawn.voltage_cV || s.voltage_valid != g_drawn.voltage_valid ||
      s.status != g_drawn.status) {
    char line[16];
    if (s.voltage_valid) {
      snprintf(line, sizeof(line), "%0.2f", s.voltage_cV / 100.0);
    } else {
      snprintf(line, sizeof(line), "--.--");
    }
    const uint32_t col = (s.status == 2 || s.status == 4) ? kWarn : kFg;
    lv_label_set_text(volt_lbl, line);
    lv_obj_set_style_text_color(volt_lbl, lv_color_hex(col), 0);
    lv_obj_align_to(volt_unit, volt_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -4);
  }

  if (s.sense_mode) {
    if (force || layout_flip || s.temp_dC != g_drawn.temp_dC ||
        s.temp_valid != g_drawn.temp_valid) {
      char t_line[12];
      if (s.temp_valid) {
        snprintf(t_line, sizeof(t_line), "%0.1f C", s.temp_dC / 10.0);
      } else {
        snprintf(t_line, sizeof(t_line), "--.- C");
      }
      lv_label_set_text(card_amps_val, t_line);
      lv_obj_set_style_text_color(card_amps_val, lv_color_hex(kFg), 0);
    }
  } else if (force || s.current_dA != g_drawn.current_dA || s.power_w != g_drawn.power_w ||
             s.current_valid != g_drawn.current_valid) {
    char a_line[12];
    char w_line[12];
    if (s.current_valid) {
      const float a = s.current_dA / 10.0f;
      const char* dir = direction_label(a);
      const uint32_t dir_col = direction_color(a);
      snprintf(a_line, sizeof(a_line), "%+0.1f", (double)a);
      snprintf(w_line, sizeof(w_line), "%+d", s.power_w);
      lv_label_set_text(card_status_val, dir);
      lv_obj_set_style_text_color(card_status_val, lv_color_hex(dir_col), 0);
      lv_obj_set_style_bg_color(card_status_accent, lv_color_hex(dir_col), 0);
    } else {
      snprintf(a_line, sizeof(a_line), "--.-");
      snprintf(w_line, sizeof(w_line), "--");
      lv_label_set_text(card_status_val, "----");
      lv_obj_set_style_text_color(card_status_val, lv_color_hex(kMuted), 0);
      lv_obj_set_style_bg_color(card_status_accent, lv_color_hex(kDim), 0);
    }
    lv_label_set_text(card_amps_val, a_line);
    lv_label_set_text(card_watts_val, w_line);
  }

  if (force || s.age_s != g_drawn.age_s || s.rssi != g_drawn.rssi || s.valid != g_drawn.valid ||
      s.status != g_drawn.status || s.sense_mode != g_drawn.sense_mode) {
    char left[40];
    char right[24];
    right[0] = '\0';
    if (s.status == 4) {
      snprintf(left, sizeof(left), "BAD KEY - CHECK SETUP");
    } else if (s.valid) {
      snprintf(left, sizeof(left), "Updated %ds", s.age_s);
      snprintf(right, sizeof(right), "RSSI %d", s.rssi);
    } else {
      snprintf(left, sizeof(left), s.sense_mode ? "Waiting for Sense" : "Waiting for shunt");
    }
    lv_label_set_text(foot_left, left);
    lv_label_set_text(foot_right, right);
  }

  g_drawn = s;
}

void dashboard_update_solar(const SolarSnapshot& s, bool force) {
  if (!g_solar_built) return;

  if (force || lv_screen_active() != scr_solar) {
    load_screen(scr_solar);
  }

  if (force || strcmp(s.name, g_drawn_solar.name) != 0) {
    set_name_label(sol_name, s.name, "MPPT");
  }
  if (force || s.status != g_drawn_solar.status) {
    apply_status_pill(sol_status_pill, sol_status_dot, sol_status_lbl, s.status);
  }

  if (force || s.solar_w != g_drawn_solar.solar_w || s.power_valid != g_drawn_solar.power_valid ||
      s.status != g_drawn_solar.status) {
    char line[16];
    if (s.power_valid) {
      snprintf(line, sizeof(line), "%d", s.solar_w);
    } else {
      snprintf(line, sizeof(line), "---");
    }
    const uint32_t col = (s.status == 2 || s.status == 4) ? kWarn : kFg;
    lv_label_set_text(sol_power, line);
    lv_obj_set_style_text_color(sol_power, lv_color_hex(col), 0);
    lv_obj_align_to(sol_power_unit, sol_power, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -4);
  }

  if (force || strcmp(s.state_label, g_drawn_solar.state_label) != 0 ||
      s.state_valid != g_drawn_solar.state_valid) {
    lv_label_set_text(sol_state, (s.state_valid && s.state_label[0]) ? s.state_label : "----");
  }

  if (force || s.battery_cV != g_drawn_solar.battery_cV ||
      s.voltage_valid != g_drawn_solar.voltage_valid) {
    char line[24];
    if (s.voltage_valid) {
      snprintf(line, sizeof(line), "Batt %0.2f V", s.battery_cV / 100.0);
    } else {
      snprintf(line, sizeof(line), "Batt --.-- V");
    }
    lv_label_set_text(sol_batt_v, line);
  }

  if (force || s.battery_dA != g_drawn_solar.battery_dA ||
      s.current_valid != g_drawn_solar.current_valid) {
    char line[16];
    if (s.current_valid) {
      snprintf(line, sizeof(line), "%+0.1f A", s.battery_dA / 10.0);
    } else {
      snprintf(line, sizeof(line), "--.- A");
    }
    lv_label_set_text(sol_batt_a, line);
  }

  if (force || s.age_s != g_drawn_solar.age_s || s.rssi != g_drawn_solar.rssi ||
      s.valid != g_drawn_solar.valid || s.status != g_drawn_solar.status) {
    char left[40];
    char right[24];
    right[0] = '\0';
    if (s.status == 4) {
      snprintf(left, sizeof(left), "BAD KEY - CHECK SETUP");
    } else if (s.valid) {
      snprintf(left, sizeof(left), "Updated %ds", s.age_s);
      snprintf(right, sizeof(right), "RSSI %d", s.rssi);
    } else {
      snprintf(left, sizeof(left), "Waiting for MPPT");
    }
    lv_label_set_text(sol_foot_l, left);
    lv_label_set_text(sol_foot_r, right);
  }

  g_drawn_solar = s;
}

void dashboard_update_orion(const OrionSnapshot& s, bool force) {
  if (!g_orion_built) return;

  if (force || lv_screen_active() != scr_orion) {
    load_screen(scr_orion);
  }

  if (force || strcmp(s.name, g_drawn_orion.name) != 0) {
    set_name_label(ori_name, s.name, "DC-DC");
  }
  if (force || s.status != g_drawn_orion.status) {
    apply_status_pill(ori_status_pill, ori_status_dot, ori_status_lbl, s.status);
  }

  if (force || strcmp(s.state_label, g_drawn_orion.state_label) != 0 ||
      s.state_valid != g_drawn_orion.state_valid) {
    lv_label_set_text(ori_mode, (s.state_valid && s.state_label[0]) ? s.state_label : "----");
  }

  if (force || s.output_cV != g_drawn_orion.output_cV ||
      s.output_valid != g_drawn_orion.output_valid) {
    char line[24];
    if (s.output_valid) {
      snprintf(line, sizeof(line), "Out %0.2f V", s.output_cV / 100.0);
    } else {
      snprintf(line, sizeof(line), "Out --.-- V");
    }
    lv_label_set_text(ori_out_v, line);
  }

  if (force || s.input_cV != g_drawn_orion.input_cV || s.input_valid != g_drawn_orion.input_valid) {
    char line[24];
    if (s.input_valid) {
      snprintf(line, sizeof(line), "In %0.2f V", s.input_cV / 100.0);
    } else {
      snprintf(line, sizeof(line), "In --.-- V");
    }
    lv_label_set_text(ori_in_v, line);
  }

  if (force || s.age_s != g_drawn_orion.age_s || s.rssi != g_drawn_orion.rssi ||
      s.valid != g_drawn_orion.valid || s.status != g_drawn_orion.status) {
    char left[40];
    char right[24];
    right[0] = '\0';
    if (s.status == 4) {
      snprintf(left, sizeof(left), "BAD KEY - CHECK SETUP");
    } else if (s.valid) {
      snprintf(left, sizeof(left), "Updated %ds", s.age_s);
      snprintf(right, sizeof(right), "RSSI %d", s.rssi);
    } else {
      snprintf(left, sizeof(left), "Waiting for Orion");
    }
    lv_label_set_text(ori_foot_l, left);
    lv_label_set_text(ori_foot_r, right);
  }

  g_drawn_orion = s;
}

void dashboard_update_info(const InfoSnapshot& s, bool force) {
  if (!g_info_built) return;

  if (force || lv_screen_active() != scr_info) {
    load_screen(scr_info);
  }

  if (force || strcmp(s.version, g_drawn_info.version) != 0) {
    char ver[20];
    if (s.version[0]) {
      snprintf(ver, sizeof(ver), "v%s", s.version);
    } else {
      snprintf(ver, sizeof(ver), "v--");
    }
    lv_label_set_text(info_version, ver);
  }

  if (force || s.device_count != g_drawn_info.device_count) {
    char line[24];
    if (s.device_count == 0) {
      snprintf(line, sizeof(line), "None configured");
    } else if (s.device_count == 1) {
      snprintf(line, sizeof(line), "1 configured");
    } else {
      snprintf(line, sizeof(line), "%u configured", (unsigned)s.device_count);
    }
    lv_label_set_text(info_dev_line, line);
  }

  if (force || s.board_present != g_drawn_info.board_present ||
      s.power_on_usb != g_drawn_info.power_on_usb ||
      s.board_voltage_valid != g_drawn_info.board_voltage_valid ||
      s.board_mV != g_drawn_info.board_mV || s.board_soc_valid != g_drawn_info.board_soc_valid ||
      s.board_soc_pct != g_drawn_info.board_soc_pct) {
    char detail[28];
    if (!s.board_present) {
      lv_label_set_text(info_power_status, "USB");
      lv_obj_set_style_text_color(info_power_status, lv_color_hex(kCyan), 0);
      lv_label_set_text(info_power_detail, "No PMIC");
      lv_obj_set_style_text_color(info_power_detail, lv_color_hex(kMuted), 0);
    } else if (s.power_on_usb) {
      lv_label_set_text(info_power_status, "USB");
      lv_obj_set_style_text_color(info_power_status, lv_color_hex(kCyan), 0);
      if (s.board_voltage_valid) {
        snprintf(detail, sizeof(detail), "LiPo %0.2f V", s.board_mV / 1000.0);
        lv_label_set_text(info_power_detail, detail);
        lv_obj_set_style_text_color(info_power_detail, lv_color_hex(kFg), 0);
      } else {
        lv_label_set_text(info_power_detail, "No LiPo");
        lv_obj_set_style_text_color(info_power_detail, lv_color_hex(kMuted), 0);
      }
    } else {
      const bool low = s.board_voltage_valid && s.board_mV < 3400;
      lv_label_set_text(info_power_status, low ? "LiPo low" : "LiPo");
      lv_obj_set_style_text_color(info_power_status, lv_color_hex(low ? kWarn : kOk), 0);
      if (s.board_voltage_valid) {
        if (s.board_soc_valid) {
          snprintf(detail, sizeof(detail), "%0.2f V · %d%%", s.board_mV / 1000.0, s.board_soc_pct);
        } else {
          snprintf(detail, sizeof(detail), "%0.2f V", s.board_mV / 1000.0);
        }
        lv_label_set_text(info_power_detail, detail);
        lv_obj_set_style_text_color(info_power_detail, lv_color_hex(kFg), 0);
      } else {
        lv_label_set_text(info_power_detail, "No reading");
        lv_obj_set_style_text_color(info_power_detail, lv_color_hex(kMuted), 0);
      }
    }
  }

  if (force || s.softap_active != g_drawn_info.softap_active ||
      strcmp(s.softap_ip, g_drawn_info.softap_ip) != 0) {
    if (s.softap_active) {
      lv_label_set_text(info_setup_status, "Active");
      lv_obj_set_style_text_color(info_setup_status, lv_color_hex(kOk), 0);
      lv_label_set_text(info_setup_detail, s.softap_ip[0] ? s.softap_ip : "192.168.4.1");
      lv_obj_set_style_text_color(info_setup_detail, lv_color_hex(kCyan), 0);
    } else {
      lv_label_set_text(info_setup_status, "Not active");
      lv_obj_set_style_text_color(info_setup_status, lv_color_hex(kMuted), 0);
      lv_label_set_text(info_setup_detail, s.softap_ip[0] ? s.softap_ip : "192.168.4.1");
      lv_obj_set_style_text_color(info_setup_detail, lv_color_hex(kMuted), 0);
    }
  }

  g_drawn_info = s;
}
