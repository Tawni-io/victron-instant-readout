#include "victron/ir_models.h"

#include <stdio.h>

namespace {

struct ModelEntry {
  uint16_t id;
  const char* label;
};

// Curated subset from keshavdv/victron-ble MODEL_ID_MAPPING — short SoftAP labels.
constexpr ModelEntry kModels[] = {
    {0x0203, "BMV-700"},
    {0x0204, "BMV-702"},
    {0xA040, "BlueSolar 75/50"},
    {0xA042, "BlueSolar 75/15"},
    {0xA043, "BlueSolar 100/15"},
    {0xA046, "BlueSolar 150/70"},
    {0xA047, "BlueSolar 150/100"},
    {0xA04C, "BlueSolar 75/10"},
    {0xA050, "SmartSolar 250/100"},
    {0xA051, "SmartSolar 150/100"},
    {0xA053, "SmartSolar 75/15"},
    {0xA054, "SmartSolar 75/10"},
    {0xA055, "SmartSolar 100/15"},
    {0xA056, "SmartSolar 100/30"},
    {0xA057, "SmartSolar 100/50"},
    {0xA058, "SmartSolar 150/35"},
    {0xA05F, "SmartSolar 100/20"},
    {0xA061, "SmartSolar 150/45"},
    {0xA062, "SmartSolar 150/60"},
    {0xA063, "SmartSolar 150/70"},
    {0xA065, "SmartSolar 250/100"},
    {0xA074, "SmartSolar 75/10"},
    {0xA075, "SmartSolar 75/15"},
    {0xA182, "VE.Direct dongle"},
    {0xA188, "VE.Direct dongle"},
    {0xA189, "VE.Direct dongle"},
    {0xA380, "BMV-710 Smart"},
    {0xA381, "BMV-712 Smart"},
    {0xA382, "BMV-710H Smart"},
    {0xA383, "BMV-712 Smart"},
    {0xA389, "SmartShunt 500A"},
    {0xA38A, "SmartShunt 1000A"},
    {0xA38B, "SmartShunt 2000A"},
    {0xA38C, "SmartShunt IP67"},
    {0xA38D, "SmartShunt IP67"},
    {0xA38E, "SmartShunt IP67"},
    {0xA3A4, "Battery Sense"},
    {0xA3A5, "Battery Sense"},
    {0xA3B0, "BatteryProtect"},
    {0xA3B1, "BatteryProtect"},
    {0xA3B2, "BatteryProtect"},
    {0xA3B3, "BatteryProtect"},
    {0xA3C0, "Orion 12/12-18"},
    {0xA3C1, "Orion 12/24-10"},
    {0xA3C2, "Orion 24/12-20"},
    {0xA3C3, "Orion 24/24-12"},
    {0xA3C8, "Orion 12/12-30"},
    {0xA3C9, "Orion 12/24-15"},
    {0xA3CA, "Orion 24/12-30"},
    {0xA3CB, "Orion 24/24-17"},
    {0xA3D0, "Orion BB 12/12"},
    {0xA3D1, "Orion BB 12/24"},
    {0xA3E5, "Lynx Smart BMS"},
    {0xA3E6, "Lynx Smart BMS"},
    {0xC030, "SmartShunt IP65"},
    {0xC031, "SmartShunt IP65"},
    {0xC032, "SmartShunt IP65"},
    {0xC034, "BMV-800 Smart"},
    {0xC035, "SmartShunt IP65"},
    {0xC036, "SmartShunt IP65"},
    {0xC037, "SmartShunt IP65"},
    {0xC038, "SmartShunt 300A"},
};

const char* range_label(uint16_t id) {
  if (id >= 0xA040 && id <= 0xA07E) return "SmartSolar/MPPT";
  if (id >= 0xA100 && id <= 0xA117) return "SmartSolar VE.Can";
  if (id >= 0xA380 && id <= 0xA38E) return "Battery monitor";
  if (id >= 0xA3C0 && id <= 0xA3D3) return "Orion DC-DC";
  if (id >= 0xC030 && id <= 0xC038) return "SmartShunt";
  return nullptr;
}

}  // namespace

const char* victron_record_label(uint8_t record_type) {
  switch (record_type) {
    case 0x01:
      return "Solar";
    case 0x02:
      return "Shunt";
    case 0x04:
      return "DC-DC";
    default:
      return "Victron";
  }
}

bool victron_model_is_battery_sense(uint16_t model_id) {
  return model_id == 0xA3A4 || model_id == 0xA3A5;
}

const char* victron_model_label(uint16_t model_id, char* out, size_t out_len) {
  if (!out || out_len == 0) return "";
  for (size_t i = 0; i < sizeof(kModels) / sizeof(kModels[0]); i++) {
    if (kModels[i].id == model_id) {
      snprintf(out, out_len, "%s", kModels[i].label);
      return out;
    }
  }
  if (const char* r = range_label(model_id)) {
    snprintf(out, out_len, "%s", r);
    return out;
  }
  snprintf(out, out_len, "0x%04X", model_id);
  return out;
}
