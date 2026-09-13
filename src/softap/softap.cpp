#include "softap/softap.h"

#include "ble_scan.h"
#include "config/device_config.h"
#include "display.h"
#include "touch.h"
#include "victron/ir_models.h"

#include <Arduino.h>
#include <Update.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <string.h>
#include <stdio.h>

#ifndef VICTRONDASH_VERSION
#define VICTRONDASH_VERSION "0.3.8"
#endif

namespace {

WebServer g_server(80);
DNSServer g_dns;
bool g_active = false;
bool g_stop_req = false;
bool g_cfg_changed = false;
bool g_ota_ok = false;
bool g_ota_active = false;
char g_ip[16] = "";

// In-setup BLE burst: SoftAP stays up (no Wi‑Fi teardown); NimBLE only.
constexpr uint32_t kBleBurstMs = 5000;
bool g_ble_bursting = false;
uint32_t g_ble_burst_until = 0;
int g_ble_burst_stations_before = 0;

void html_escape_to(const char* in, char* out, size_t out_len) {
  if (!out || out_len == 0) return;
  size_t o = 0;
  out[0] = '\0';
  if (!in) return;
  for (size_t i = 0; in[i] && o + 1 < out_len; i++) {
    const char c = in[i];
    const char* rep = nullptr;
    if (c == '&') rep = "&amp;";
    else if (c == '<') rep = "&lt;";
    else if (c == '>') rep = "&gt;";
    else if (c == '"') rep = "&quot;";
    if (rep) {
      const size_t n = strlen(rep);
      if (o + n >= out_len) break;
      memcpy(out + o, rep, n);
      o += n;
    } else {
      out[o++] = c;
    }
  }
  out[o] = '\0';
}

void send_flash_chunks(PGM_P s) {
  if (!s) return;
  // Small RAM window — never assemble the full page in heap.
  char buf[192];
  const size_t len = strlen_P(s);
  size_t fed = 0;
  for (size_t off = 0; off < len;) {
    size_t n = len - off;
    if (n > sizeof(buf)) n = sizeof(buf);
    memcpy_P(buf, s + off, n);
    g_server.chunkWrite(buf, n);
    off += n;
    fed += n;
    // Long chunked pages can exceed the TWDT if we never yield.
    if (fed >= 512) {
      fed = 0;
      delay(0);
      yield();
    }
  }
}

void send_ram_chunk(const char* s) {
  if (s && s[0]) {
    g_server.chunkWrite(s, strlen(s));
  }
}

// Stream setup HTML via native chunk API — SoftAP heap is too fragmented for one String.
void send_page(const char* flash_msg, bool flash_ok) {
  DeviceList list = {};
  device_config_load_all(&list);
  const size_t batt_idx = device_config_battery_source_index(&list);
  const uint8_t orient = device_config_get_orient();

  Serial.printf("SoftAP page stream (heap %u maxblk %u)\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());

  g_server.chunkResponseBegin("text/html");
  delay(0);

  send_flash_chunks(PSTR(
      "<!DOCTYPE html><html><head><meta charset=utf-8>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<title>VictronDash</title><style>"
      "body{font-family:system-ui,sans-serif;background:#0B0F14;color:#E8EEF4;"
      "margin:0;padding:16px;max-width:480px}"
      "h1{font-size:1.25rem;margin:0 0 4px}"
      "h2{font-size:1rem;margin:20px 0 8px;color:#E8EEF4}"
      ".muted{color:#8B98A8;font-size:.9rem;margin:0 0 16px}"
      "label{display:block;margin:12px 0 4px;color:#8B98A8;font-size:.85rem}"
      "input,select{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;"
      "border:1px solid #2A3544;background:#151C26;color:#E8EEF4;font-size:1rem}"
      "button,.btn{display:inline-block;margin:8px 8px 0 0;padding:12px 16px;"
      "border:0;border-radius:6px;font-size:1rem;cursor:pointer;text-decoration:none}"
      ".primary{background:#3DDC97;color:#0B0F14;font-weight:600}"
      ".secondary{background:#2A3544;color:#E8EEF4}"
      ".danger{background:#F05152;color:#fff}"
      ".ok{color:#3DDC97}.err{color:#F05152}"
      "ol{padding-left:1.2rem;color:#8B98A8;font-size:.9rem}"
      "li{margin:6px 0}"
      "hr{border:0;border-top:1px solid #2A3544;margin:20px 0}"
      "ul.list{list-style:none;padding:0;margin:0 0 16px}"
      "ul.list li{display:flex;justify-content:space-between;gap:8px;align-items:center;"
      "padding:10px;margin:0 0 8px;border:1px solid #2A3544;border-radius:6px;"
      "background:#151C26}"
      "ul.list b{display:block}"
      "ul.list span{display:block;color:#8B98A8;font-size:.85rem;margin-top:2px}"
      "#nearby{margin:0 0 16px}"
      ".near{display:block;width:100%;text-align:left;margin:0 0 8px;"
      "padding:12px;border-radius:6px;border:1px solid #2A3544;"
      "background:#151C26;color:#E8EEF4;font-size:.95rem}"
      ".near:active{border-color:#3DDC97}"
      ".near b{display:block}"
      ".near span{color:#8B98A8;font-size:.85rem}"
      "</style></head><body>"
      "<h1>VictronDash setup</h1>"));

  {
    char line[160];
    const char* from = "(not configured)";
    char from_esc[48];
    if (batt_idx != SIZE_MAX) {
      from = list.devices[batt_idx].name[0] ? list.devices[batt_idx].name : "Battery";
      html_escape_to(from, from_esc, sizeof(from_esc));
      from = from_esc;
    }
    snprintf(line, sizeof(line),
             "<p class=muted>v" VICTRONDASH_VERSION " · Battery data from: <b>%s</b></p>",
             from);
    send_ram_chunk(line);
  }

  if (flash_msg && flash_msg[0]) {
    char esc[96];
    char line[160];
    html_escape_to(flash_msg, esc, sizeof(esc));
    snprintf(line, sizeof(line), "<p class=\"%s\">%s</p>", flash_ok ? "ok" : "err", esc);
    send_ram_chunk(line);
  }

  send_flash_chunks(PSTR(
      "<h2>Flip Display 180</h2>"
      "<p class=muted style=\"margin-bottom:8px\">For upside-down mounts. Long-press the "
      "marked setup button (same physical button after flip).</p>"
      "<form method=POST action=/orient>"
      "<select name=orient>"));
  send_flash_chunks(orient != kOrientFlip ? PSTR("<option value=0 selected>Off</option>"
                                                 "<option value=1>On</option>")
                                          : PSTR("<option value=0>Off</option>"
                                                 "<option value=1 selected>On</option>"));
  send_flash_chunks(PSTR("</select>"
                         "<button class=primary type=submit>Save</button>"
                         "</form>"
                         "<h2>Saved devices</h2>"));

  if (list.count == 0) {
    send_flash_chunks(PSTR("<p class=muted>No saved devices yet.</p>"));
  } else {
    send_flash_chunks(PSTR("<ul class=list>"));
    for (size_t i = 0; i < list.count; i++) {
      const DeviceEntry& e = list.devices[i];
      char name_esc[48];
      char mac_esc[40];
      char line[256];
      html_escape_to(e.name, name_esc, sizeof(name_esc));
      html_escape_to(e.mac, mac_esc, sizeof(mac_esc));
      snprintf(line, sizeof(line),
               "<li><div><b>%s</b><span>%s · %s%s</span></div>"
               "<form method=POST action=/remove style=\"margin:0\">"
               "<input type=hidden name=index value=\"%u\">"
               "<button class=secondary type=submit>Remove</button></form></li>",
               name_esc, mac_esc, device_config_type_label(e.type),
               (i == batt_idx) ? " · Battery source" : "", (unsigned)i);
      send_ram_chunk(line);
    }
    send_flash_chunks(PSTR("</ul>"));
  }

  send_flash_chunks(PSTR(
      "<h2>Nearby Victron</h2>"
      "<p class=muted style=\"margin-bottom:8px\">Tap to fill MAC and suggest Name. "
      "Use <b>Scan nearby</b> if empty (Wi-Fi stays on).</p>"
      "<div id=nearby><p class=muted>Loading...</p></div>"
      "<p id=scanStatus class=muted style=\"margin:8px 0 0\"></p>"
      "<button type=button class=secondary id=scanBtn onclick=\"scanNearby()\">"
      "Scan nearby</button>"
      "<form method=POST action=/save>"
      "<label>Name</label>"
      "<input name=name id=name maxlength=31 placeholder=\"House shunt\" value=\"\">"
      "<label>MAC address</label>"
      "<input name=mac id=mac maxlength=31 placeholder=\"paste or tap nearby\" "
      "autocapitalize=off autocomplete=off spellcheck=false value=\"\">"
      "<p class=muted style=\"margin:4px 0 0\">Paste as shown - colons optional "
      "(e.g. aabbccddeeff).</p>"
      "<label>Encryption key (32 hex chars)</label>"
      "<input name=key id=key maxlength=64 placeholder=\"paste from VictronConnect\" "
      "autocapitalize=off autocomplete=off spellcheck=false value=\"\">"
      "<label>Type</label>"
      "<select name=type id=type>"
      "<option value=auto selected>Auto</option>"
      "<option value=battery>Shunt</option>"
      "<option value=solar>Solar</option>"
      "<option value=dcdc>DC-DC</option>"
      "</select>"
      "<button class=primary type=submit>Save device</button>"
      "</form>"
      "<form method=POST action=/clear style=\"display:inline\">"
      "<button class=secondary type=submit>Clear all</button>"
      "</form>"
      "<hr>"
      "<p class=muted><b>How to get MAC + key</b></p>"
      "<ol>"
      "<li>VictronConnect - connect to the device</li>"
      "<li>Gear - Settings - Product info</li>"
      "<li>Turn Instant readout via Bluetooth on</li>"
      "<li>Instant readout details - SHOW</li>"
      "<li>Paste MAC (or tap nearby) and Encryption Key above</li>"
      "<li>If you change the Victron Bluetooth PIN later, the key changes - "
      "copy it again</li>"
      "</ol>"
      "<hr>"
      "<h2>Firmware</h2>"
      "<p class=muted style=\"margin-bottom:8px\">Current: <b>v" VICTRONDASH_VERSION
      "</b>. Upload one app <code>.bin</code>. Keep power on. Devices survive update.</p>"
      "<form id=fwForm>"
      "<label>Firmware file (.bin)</label>"
      "<input type=file id=fwFile name=firmware accept=.bin,application/octet-stream>"
      "<button class=primary type=submit id=fwBtn>Upload firmware</button>"
      "</form>"
      "<p id=fwStatus class=muted style=\"margin:8px 0 0\"></p>"
      "<form method=POST action=/factory-reset id=factoryForm style=\"margin-top:12px\">"
      "<button class=danger type=submit>Factory reset</button>"
      "</form>"
      "<p class=muted style=\"margin:8px 0 0\">Factory reset erases devices and Flip "
      "Display 180, then reboots to SETUP MODE.</p>"
      "<hr>"
      "<form method=POST action=/stop>"
      "<button class=danger type=submit>Stop hotspot</button>"
      "</form>"
      "<p class=muted>Cabin shows SETUP MODE while hotspot is on. Up to 8 devices.</p>"));

  send_flash_chunks(PSTR(
      "<script>"
      "function esc(s){return String(s||'').replace(/[&<>\"']/g,function(c){"
      "return ({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'})[c];});}"
      "function pick(mac,label,type){"
      "var m=document.getElementById('mac');if(m){m.value=mac;}"
      "var n=document.getElementById('name');"
      "if(n&&(!n.value||n.dataset.suggested==='1')){n.value=label||'';n.dataset.suggested='1';}"
      "var t=document.getElementById('type');"
      "if(t&&type){var map={Shunt:'battery',Battery:'battery',Solar:'solar','DC-DC':'dcdc'};"
      "if(map[type])t.value=map[type];}"
      "if(m)m.focus();"
      "}"
      "function render(d){"
      "var box=document.getElementById('nearby');if(!box)return;"
      "if(!d.devices||!d.devices.length){"
      "box.innerHTML='<p class=muted>No Victron ads yet - move closer, "
      "Instant Readout on.</p>';return;}"
      "var h='';"
      "d.devices.forEach(function(x){"
      "var label=x.model||x.gap||x.type||'Victron';"
      "h+='<button type=button class=near onclick=\"pick(\\''+x.mac+"
      "'\\',\\''+String(label).replace(/'/g,'')+"
      "'\\',\\''+x.type+'\\')\"><b>'+esc(label)+'</b><span>'+"
      "esc(x.mac)+' · '+esc(x.type)+' · RSSI '+x.rssi+' · heard '+x.age_s+'s ago';"
      "if(x.gap){h+=' · '+esc(x.gap);}"
      "h+='</span></button>';"
      "});"
      "box.innerHTML=h;"
      "}"
      "function poll(){"
      "fetch('/nearby.json').then(function(r){return r.json();})"
      ".then(render).catch(function(){});"
      "}"
      "function setScanStatus(t){"
      "var s=document.getElementById('scanStatus');if(s)s.textContent=t||'';"
      "}"
      "function scanNearby(){"
      "var b=document.getElementById('scanBtn');"
      "if(b){b.disabled=true;}"
      "setScanStatus('Scanning... Wi-Fi stays on');"
      "fetch('/scan',{method:'POST'}).then(function(r){return r.json();})"
      ".then(function(j){"
      "if(!j||!j.ok){setScanStatus('Scan failed');if(b)b.disabled=false;return;}"
      "var n=0;"
      "var iv=setInterval(function(){"
      "n++;poll();"
      "fetch('/scan/status').then(function(r){return r.json();})"
      ".then(function(st){"
      "if(st&&!st.scanning){"
      "clearInterval(iv);poll();setScanStatus('Scan done');"
      "if(b)b.disabled=false;"
      "}else if(n>=10){"
      "clearInterval(iv);poll();setScanStatus('Scan done');"
      "if(b)b.disabled=false;"
      "}"
      "}).catch(function(){});"
      "},1000);"
      "}).catch(function(){"
      "setScanStatus('Scan failed');if(b)b.disabled=false;"
      "});"
      "}"
      "function setFwStatus(t,ok){"
      "var s=document.getElementById('fwStatus');if(!s)return;"
      "s.textContent=t||'';s.className=ok===true?'ok':(ok===false?'err':'muted');"
      "}"
      "var fwForm=document.getElementById('fwForm');"
      "if(fwForm){fwForm.addEventListener('submit',function(ev){"
      "ev.preventDefault();"
      "var f=document.getElementById('fwFile');"
      "var btn=document.getElementById('fwBtn');"
      "if(!f||!f.files||!f.files.length){setFwStatus('Choose a .bin file first.',false);return;}"
      "var name=(f.files[0].name||'').toLowerCase();"
      "if(name && name.indexOf('.bin')<0){"
      "if(!confirm('File does not end in .bin. Upload anyway?'))return;}"
      "var fd=new FormData();fd.append('firmware',f.files[0],f.files[0].name);"
      "var xhr=new XMLHttpRequest();"
      "xhr.open('POST','/update');"
      "xhr.upload.onprogress=function(e){"
      "if(e.lengthComputable){"
      "var pct=Math.floor(e.loaded*100/e.total);"
      "setFwStatus('Uploading... '+pct+'%',null);"
      "}else{setFwStatus('Uploading...',null);}"
      "};"
      "xhr.onload=function(){"
      "if(btn)btn.disabled=false;"
      "if(xhr.status>=200&&xhr.status<300){"
      "setFwStatus(xhr.responseText||'OK - rebooting...',true);"
      "}else{setFwStatus(xhr.responseText||('Upload failed ('+xhr.status+')'),false);}"
      "};"
      "xhr.onerror=function(){"
      "if(btn)btn.disabled=false;setFwStatus('Upload failed - keep power on and retry.',false);"
      "};"
      "if(btn)btn.disabled=true;setFwStatus('Uploading...',null);xhr.send(fd);"
      "});}"
      "var factoryForm=document.getElementById('factoryForm');"
      "if(factoryForm){factoryForm.addEventListener('submit',function(ev){"
      "if(!confirm('Erase all saved devices and return to SETUP MODE?'))ev.preventDefault();"
      "});}"
      "setTimeout(poll,800);setInterval(poll,3000);"
      "</script>"
      "</body></html>"));

  g_server.chunkResponseEnd();
}

void handle_root() { send_page(nullptr, true); }

void handle_nearby() {
  BleNearbyDevice list[kBleNearbyMax];
  size_t n = ble_scan_nearby(list, kBleNearbyMax);

  // Stack buffer — avoid Arduino String churn on every 2s poll.
  char json[1536];
  int pos = snprintf(json, sizeof(json), "{\"devices\":[");
  if (pos < 0) pos = 0;
  for (size_t i = 0; i < n && pos < (int)sizeof(json) - 8; i++) {
    char mac[13];
    snprintf(mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x", list[i].mac[0],
             list[i].mac[1], list[i].mac[2], list[i].mac[3], list[i].mac[4],
             list[i].mac[5]);
    char model[40];
    victron_model_label(list[i].model_id, model, sizeof(model));
    const int wrote =
        snprintf(json + pos, sizeof(json) - (size_t)pos,
                 "%s{\"mac\":\"%s\",\"rssi\":%d,\"type\":\"%s\",\"model\":\"%s\","
                 "\"gap\":\"%s\",\"age_s\":%u}",
                 (i ? "," : ""), mac, (int)list[i].rssi,
                 victron_record_label(list[i].record_type), model, list[i].gap_name,
                 (unsigned)(list[i].age_ms / 1000u));
    if (wrote < 0) break;
    pos += wrote;
  }
  if (pos < (int)sizeof(json) - 3) {
    snprintf(json + pos, sizeof(json) - (size_t)pos, "]}");
  } else {
    json[sizeof(json) - 3] = ']';
    json[sizeof(json) - 2] = '}';
    json[sizeof(json) - 1] = '\0';
  }
  // Avoid send(char*) → String copy of the whole JSON (fragments SoftAP heap).
  const size_t json_len = strlen(json);
  g_server.setContentLength(json_len);
  g_server.send(200, "application/json", "");
  g_server.sendContent(json, json_len);
}

void handle_save() {
  DeviceEntry e = {};
  String name = g_server.hasArg("name") ? g_server.arg("name") : "";
  String mac = g_server.hasArg("mac") ? g_server.arg("mac") : "";
  String key = g_server.hasArg("key") ? g_server.arg("key") : "";
  String type = g_server.hasArg("type") ? g_server.arg("type") : "auto";

  strncpy(e.name, name.c_str(), kDeviceNameMax - 1);
  strncpy(e.mac, mac.c_str(), kMacInputBuf - 1);
  strncpy(e.key_hex, key.c_str(), kKeyHexLen);
  if (!device_config_parse_type(type.c_str(), &e.type)) {
    send_page("Type must be Auto / Shunt / Solar / DC-DC.", false);
    return;
  }

  uint8_t key_raw[16];
  if (!device_config_parse_key_hex(e.key_hex, key_raw)) {
    send_page("Key must be 32 hex characters.", false);
    return;
  }
  if (!e.mac[0]) {
    send_page("MAC is required when adding a device.", false);
    return;
  }
  uint8_t macb[6];
  if (!device_config_parse_mac(e.mac, macb)) {
    send_page("MAC looks invalid. Paste the 12 hex chars from VictronConnect.", false);
    return;
  }

  DeviceList before = {};
  device_config_load_all(&before);
  if (before.count >= kMaxDevices) {
    bool updating = false;
    for (size_t i = 0; i < before.count; i++) {
      uint8_t existing[6];
      if (device_config_parse_mac(before.devices[i].mac, existing) &&
          memcmp(existing, macb, 6) == 0) {
        updating = true;
        break;
      }
    }
    if (!updating) {
      send_page("Maximum 8 devices. Remove one first.", false);
      return;
    }
  }

  if (!device_config_upsert(&e)) {
    send_page("Could not save. Check MAC and key.", false);
    return;
  }

  g_cfg_changed = true;
  send_page("Saved. Add another device or tap Stop hotspot.", true);
}

void handle_remove() {
  if (!g_server.hasArg("index")) {
    send_page("Missing device index.", false);
    return;
  }
  const int idx = g_server.arg("index").toInt();
  if (idx < 0 || !device_config_remove_at((size_t)idx)) {
    send_page("Could not remove device.", false);
    return;
  }
  g_cfg_changed = true;
  send_page("Removed.", true);
}

void handle_clear() {
  device_config_clear();
  g_cfg_changed = true;
  send_page("Cleared. Add MAC + key again to decrypt.", true);
}

void handle_orient() {
  const int raw = g_server.hasArg("orient") ? g_server.arg("orient").toInt() : 0;
  const uint8_t orient = (raw == 1) ? kOrientFlip : kOrientNormal;
  if (!device_config_set_orient(orient)) {
    send_page("Could not save Flip Display 180.", false);
    return;
  }
  display_reconfigure(orient);
  touch_set_nav_flip(orient == kOrientFlip);
  send_page(orient == kOrientFlip ? "Flip Display 180: On." : "Flip Display 180: Off.",
            true);
}

void handle_factory_reset() {
  device_config_clear();
  device_config_reset_display_prefs();
  g_cfg_changed = true;
  Serial.println("SoftAP factory reset — clearing devices/display prefs and rebooting");
  g_server.sendHeader(F("Connection"), F("close"));
  g_server.send(200, "text/html",
                F("<!DOCTYPE html><html><head><meta charset=utf-8>"
                  "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
                  "<title>VictronDash</title>"
                  "<style>body{font-family:system-ui,sans-serif;background:#0B0F14;"
                  "color:#E8EEF4;margin:0;padding:24px}</style></head><body>"
                  "<h1>Factory reset</h1>"
                  "<p>Saved devices erased. Rebooting into SETUP MODE…</p>"
                  "</body></html>"));
  delay(500);
  ESP.restart();
}

void handle_update_upload() {
  HTTPUpload& upload = g_server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    g_ota_ok = false;
    g_ota_active = true;
    if (g_ble_bursting) {
      ble_scan_pause();
      g_ble_bursting = false;
    }
    Serial.printf("SoftAP OTA start: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      g_ota_active = false;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.isRunning()) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      g_ota_ok = true;
      Serial.printf("SoftAP OTA success: %u bytes\n", (unsigned)upload.totalSize);
    } else {
      Update.printError(Serial);
    }
    g_ota_active = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    g_ota_ok = false;
    g_ota_active = false;
    Serial.println("SoftAP OTA aborted");
  }
}

void handle_update_done() {
  g_server.sendHeader(F("Connection"), F("close"));
  if (g_ota_ok && !Update.hasError()) {
    g_server.send(200, "text/plain", F("OK — rebooting…"));
    delay(400);
    ESP.restart();
    return;
  }
  String err = F("Update failed");
  if (Update.hasError()) {
    err += F(" (check Serial / keep power on / use app .bin not full flash image)");
  }
  g_server.send(500, "text/plain", err);
  g_ota_ok = false;
  g_ota_active = false;
}

// Shown only while stop is in progress. After SoftAP re-enters setup, browsers often
// still have /goodbye bookmarked — send them home instead of a stale "stopping" page.
void handle_goodbye() {
  g_server.sendHeader(F("Cache-Control"), F("no-store"), true);
  if (!g_stop_req) {
    g_server.sendHeader(F("Location"), F("/"), true);
    g_server.send(302, "text/plain", "");
    return;
  }
  g_server.send(200, "text/html",
                F("<!DOCTYPE html><html><head><meta charset=utf-8>"
                  "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
                  "<title>VictronDash</title>"
                  "<style>"
                  "body{font-family:system-ui,sans-serif;background:#0B0F14;color:#E8EEF4;"
                  "margin:0;padding:24px;max-width:480px}"
                  "h1{font-size:1.25rem;margin:0 0 8px}"
                  "p{color:#8B98A8;line-height:1.4}"
                  "a.btn{display:inline-block;margin-top:16px;padding:12px 16px;"
                  "background:#3DDC97;color:#0B0F14;font-weight:600;text-decoration:none;"
                  "border-radius:6px}"
                  "</style></head><body>"
                  "<h1>Hotspot stopping…</h1>"
                  "<p>You can leave this Wi‑Fi network. Cabin gauge returns.</p>"
                  "<p>Next time you join VictronDash, tap below for the setup page.</p>"
                  "<a class=btn href=\"/\">Open setup / Home</a>"
                  "</body></html>"));
}

void handle_stop() {
  g_stop_req = true;
  // 303 → GET /goodbye so Refresh does not re-POST /stop.
  g_server.sendHeader(F("Location"), F("/goodbye"), true);
  g_server.send(303, "text/plain", "");
}

void handle_stop_get() {
  // Accidental GET/bookmark of /stop must not tear down SoftAP.
  g_server.sendHeader(F("Location"), F("/goodbye"), true);
  g_server.send(302, "text/plain", "");
}

void handle_scan() {
  if (!g_active) {
    g_server.send(503, "application/json", F("{\"ok\":false,\"error\":\"inactive\"}"));
    return;
  }
  if (g_ota_active) {
    g_server.send(503, "application/json", F("{\"ok\":false,\"error\":\"ota\"}"));
    return;
  }
  if (g_ble_bursting) {
    g_server.send(200, "application/json",
                  F("{\"ok\":true,\"scanning\":true,\"already\":true,\"ms\":5000}"));
    return;
  }

  // SoftAP association must stay up — resume NimBLE only, never tear down Wi‑Fi.
  g_ble_burst_stations_before = (int)WiFi.softAPgetStationNum();
  Serial.printf("SoftAP BLE scan start (stations=%d heap=%u maxblk=%u)\n",
                g_ble_burst_stations_before, (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());

  if (!ble_scan_resume()) {
    g_server.send(500, "application/json", F("{\"ok\":false,\"error\":\"ble\"}"));
    return;
  }
  g_ble_bursting = true;
  g_ble_burst_until = millis() + kBleBurstMs;
  g_server.send(200, "application/json", F("{\"ok\":true,\"scanning\":true,\"ms\":5000}"));
}

void handle_scan_status() {
  g_server.send(200, "application/json",
                g_ble_bursting ? F("{\"scanning\":true}") : F("{\"scanning\":false}"));
}

void finish_ble_burst_if_due(void) {
  if (!g_ble_bursting) return;
  if ((int32_t)(millis() - g_ble_burst_until) < 0) return;

  const int after = (int)WiFi.softAPgetStationNum();
  ble_scan_pause();
  g_ble_bursting = false;
  Serial.printf("SoftAP BLE scan done (stations %d→%d heap=%u)\n",
                g_ble_burst_stations_before, after, (unsigned)ESP.getFreeHeap());
  if (after < g_ble_burst_stations_before) {
    Serial.println("WARN: SoftAP station count dropped during BLE scan");
  }
}

// Captive OS probes used to rebuild the full ~9KB setup page — that blew the heap
// (min free ~1.8KB) and made the real page intermittent. Keep probes tiny.
void handle_generate_204() { g_server.send(204); }

void handle_apple_captive() {
  g_server.send(200, "text/html",
                F("<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"));
}

void handle_ms_connecttest() {
  g_server.send(200, "text/plain", F("Microsoft Connect Test"));
}

void handle_ms_ncsi() { g_server.send(200, "text/plain", F("Microsoft NCSI")); }

void handle_not_found() {
  char loc[40];
  snprintf(loc, sizeof(loc), "http://%s/", g_ip[0] ? g_ip : "192.168.4.1");
  g_server.sendHeader(F("Location"), loc, true);
  g_server.send(302, "text/plain", "");
}

}  // namespace

bool softap_start(void) {
  if (g_active) return true;

  g_stop_req = false;
  g_ble_bursting = false;
  g_ble_burst_until = 0;
  g_ota_ok = false;
  g_ota_active = false;

  ble_scan_pause();
  delay(150);

  WiFi.persistent(false);
  // Long WIFI_OFF settle reclaim Wi‑Fi buffers so SoftAP re-entry works without
  // power-cycling after Flip / leave / re-enter.
  WiFi.mode(WIFI_OFF);
  {
    const uint32_t settle_until = millis() + 600;
    while ((int32_t)(millis() - settle_until) < 0) {
      delay(40);
      if (ESP.getMaxAllocHeap() >= 24000) {
        break;
      }
    }
    delay(150);
  }
  Serial.printf("SoftAP pre-AP (heap %u maxblk %u)\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());
  WiFi.mode(WIFI_AP);
  delay(80);

  // One station is enough for phone setup; fewer Wi‑Fi buffers.
  bool ok = WiFi.softAP(kSoftApSsid, nullptr, 1, 0, 1);
  if (!ok) {
    Serial.println("SoftAP start FAILED — retry after WIFI_OFF");
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_AP);
    delay(80);
    ok = WiFi.softAP(kSoftApSsid, nullptr, 1, 0, 1);
  }
  if (!ok) {
    Serial.println("SoftAP start FAILED");
    WiFi.mode(WIFI_OFF);
    ble_scan_resume();
    return false;
  }

  delay(200);
  IPAddress ip = WiFi.softAPIP();
  snprintf(g_ip, sizeof(g_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  Serial.printf("SoftAP %s  http://%s  ch=%d  heap=%u maxblk=%u\n", kSoftApSsid, g_ip,
                WiFi.channel(), (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());

  g_dns.start(53, "*", ip);

  g_server.on("/", HTTP_GET, handle_root);
  g_server.on("/nearby.json", HTTP_GET, handle_nearby);
  g_server.on("/scan", HTTP_POST, handle_scan);
  g_server.on("/scan/status", HTTP_GET, handle_scan_status);
  g_server.on("/save", HTTP_POST, handle_save);
  g_server.on("/orient", HTTP_POST, handle_orient);
  g_server.on("/remove", HTTP_POST, handle_remove);
  g_server.on("/clear", HTTP_POST, handle_clear);
  g_server.on("/factory-reset", HTTP_POST, handle_factory_reset);
  g_server.on("/update", HTTP_POST, handle_update_done, handle_update_upload);
  g_server.on("/stop", HTTP_POST, handle_stop);
  g_server.on("/stop", HTTP_GET, handle_stop_get);
  g_server.on("/goodbye", HTTP_GET, handle_goodbye);
  g_server.on("/generate_204", HTTP_GET, handle_generate_204);
  g_server.on("/hotspot-detect.html", HTTP_GET, handle_apple_captive);
  g_server.on("/library/test/success.html", HTTP_GET, handle_apple_captive);
  g_server.on("/connecttest.txt", HTTP_GET, handle_ms_connecttest);
  g_server.on("/ncsi.txt", HTTP_GET, handle_ms_ncsi);
  g_server.on("/fwlink", HTTP_GET, handle_not_found);
  g_server.onNotFound(handle_not_found);
  g_server.begin();

  g_active = true;
  return true;
}

void softap_stop(void) {
  if (!g_active) return;
  // End any in-setup burst cleanly before cabin resume.
  if (g_ble_bursting) {
    g_ble_bursting = false;
    ble_scan_pause();
  }
  g_server.stop();
  g_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  g_active = false;
  g_stop_req = false;
  g_ip[0] = '\0';
  Serial.println("SoftAP stopped");
  delay(350);
  ble_scan_resume();
}

void softap_loop(void) {
  if (!g_active) return;
  finish_ble_burst_if_due();
  g_dns.processNextRequest();
  g_server.handleClient();
}

bool softap_active(void) { return g_active; }

const char* softap_ip(void) { return g_ip; }

bool softap_stop_requested(void) { return g_stop_req; }

void softap_clear_stop_request(void) { g_stop_req = false; }

bool softap_config_changed(void) { return g_cfg_changed; }

void softap_clear_config_changed(void) { g_cfg_changed = false; }

bool softap_ota_active(void) { return g_ota_active; }
