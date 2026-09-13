# Firmware storage and updates

Clean field updates for a dash-mounted gauge.

Related: [DASHBOARD.md](DASHBOARD.md) · [SETUP.md](SETUP.md) · [INSTANT_READOUT.md](INSTANT_READOUT.md) · [CHANGELOG.md](../CHANGELOG.md)

**Status:** SoftAP offline upload + Factory reset implemented. Dual OTA slots via board `default_16MB` (`app0` / `app1`).

---

## User story (what enthusiasts do)

1. Download **one** app `.bin` from GitHub Releases or the project website (on phone with internet)  
2. Long-press the marked setup button (GPIO0) → join `VictronDash` → **Firmware** section on the one setup page → upload  
3. Wait for reboot — check version on SoftAP header / cabin **Info** page (`v…`)

USB-C only for **first flash** or if something goes wrong.

No van Wi‑Fi. No “device phones home to GitHub” in MVP.

---

## SoftAP Firmware section (same page as setup)

- Current version  
- File picker + upload progress  
- Help: download from Releases · keep power on · USB if brick  
- **Factory reset** — erase all saved devices and Flip Display 180; reboot into SETUP MODE  

Lives on the **single** SoftAP config page — not a separate nav item.

**Clear all** (above Firmware) only empties the device list without reboot. Factory reset is the recover-to-setup path.

---

## Recovery ladder

1. **Bad new image that fails early** — dual-slot rollback (automatic after OTA; confirmed with `esp_ota_mark_app_valid_cancel_rollback` on healthy boot)  
2. **Wrong / confused config** — SoftAP **Factory reset**  
3. **Brick / interrupted OTA / no SoftAP** — USB-C reflash  

Release notes: [CHANGELOG.md](../CHANGELOG.md).

---

## Versioning

Use this section when shipping firmware. Do **not** invent a second version channel.

### Version string

| Rule | Detail |
| --- | --- |
| **Canonical define** | `-DVICTRONDASH_VERSION=\"X.Y.Z\"` on env `bringup_ble` in [`platformio.ini`](../platformio.ini) |
| **Format** | Semver `MAJOR.MINOR.PATCH` (e.g. `0.3.1`). Optional short suffix only for lab builds (`0.3.1-test`) — strip before a public Release |
| **Where it appears** | SoftAP setup page header + Firmware section; cabin **Info** page |
| **Fallbacks** | `#ifndef VICTRONDASH_VERSION` in `src/softap/softap.cpp` and `src/main.cpp` — keep these equal to the current `platformio.ini` value so a missing `-D` cannot show a stale string |

**Every release bump:** change `platformio.ini` **and** both `#ifndef` fallbacks to the same string.

### Config / wipe policy

| Flag | Field builds (`bringup_ble`) | Lab only |
| --- | --- | --- |
| `WIPE_CONFIG_ON_NEW_FW` | **`0`** — NVS devices/keys **survive** SoftAP OTA and USB flash of a new binary | Set `1` only to retest first-boot SETUP MODE |

Wipe logic: `src/config/device_config.cpp`. Intentional erase for users: SoftAP **Factory reset** (not a new firmware version).

### Which binary

| Do | Don’t |
| --- | --- |
| Ship / SoftAP-upload `.pio/build/bringup_ble/firmware.bin` | Upload a merged full-flash image (bootloader + partitions + app) |
| Rename for Releases: `victron-instant-readout-t-display-c5-vX.Y.Z.bin` (copy of that app `.bin`) | Use `bringup_lcd` / `bringup_touch` builds as cabin releases |

SoftAP OTA writes the **inactive app slot only**.

### Code map (do not relocate without updating this doc)

| Piece | Path |
| --- | --- |
| SoftAP Firmware UI + `POST /update` + Factory reset | `src/softap/softap.cpp` |
| Mark app valid after healthy boot | `src/main.cpp` (`esp_ota_mark_app_valid_cancel_rollback`) |
| NVS devices + wipe-on-new-fw | `src/config/device_config.cpp` |
| Version build flag | `platformio.ini` → `[env:bringup_ble]` |
| Partitions | board `default_16MB.csv` (`app0` / `app1` ~6.25 MB each) |

### Hard don’ts (MVP)

- Do **not** add ArduinoOTA / always-on van Wi‑Fi / auto-download from GitHub  
- Do **not** put Firmware on a separate SoftAP page or cabin edit form  
- Do **not** set `WIPE_CONFIG_ON_NEW_FW=1` on field/release builds  
- Do **not** treat git tags as the on-device version string — `VICTRONDASH_VERSION` is what users see  

### Release checklist

1. Bump `VICTRONDASH_VERSION` in `platformio.ini` + both `#ifndef` fallbacks (same value)  
2. Add a short entry at the top of [CHANGELOG.md](../CHANGELOG.md)  
3. `pio run -e bringup_ble` — confirm `.bin` fits an OTA slot (~6.25 MB max practical)  
4. Smoke: USB flash → SoftAP OTA of the **same** app `.bin` → SoftAP / Info show new `v…` → **saved devices still present**  
5. Optional: Factory reset once on a test config → SETUP MODE / empty list  
6. Copy `firmware.bin` → `victron-instant-readout-t-display-c5-vX.Y.Z.bin` · tag + GitHub Release + short notes  
7. Update [COMPATIBILITY.md](COMPATIBILITY.md) if behaviour or supported kit changed  

CI attach-on-tag / signed images: later. SoftAP upload stays the reliable customer path.

---

## How we ship builds

| Piece | Role |
| --- | --- |
| GitHub repo | Source |
| GitHub Releases | `victron-instant-readout-t-display-c5-vX.Y.Z.bin` + short notes |
| SoftAP upload | Field update |
| USB-C | Factory / recovery / dev |

---

## On device (engineering)

16 MB flash (`default_16MB.csv`): `nvs` · `otadata` · `ota_0` / `ota_1` (~6.25 MB each) · `spiffs` · `coredump`.

Upload writes the inactive slot → reboot → mark app valid after a healthy boot (enables rollback). Mark runs after BLE starts successfully; a SoftAP-only crash after that may not auto-roll back (USB / previous known-good `.bin` still recover).

Web UI stays embedded in firmware (LittleFS optional later).

**Stay powered** during upload. Interrupted OTA may need USB recovery. Prefer `http://192.168.4.1` in a normal browser tab on phone SoftAP (captive portals can drop large uploads).
