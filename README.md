# Tawni — Victron Instant Readout

Cabin gauge for Victron kit. Same Tawni box, this firmware face.

**Hardware:** [LILYGO T-Display C5](https://www.lilygo.cc/products/t-display-c5) (ESP32-C5, 1.9″ 320×170).  
**Product:** [tawni.io](https://tawni.io)

Tawni is **not affiliated with, endorsed by, or sponsored by Victron Energy B.V.** Victron, VictronConnect, and Instant Readout are trademarks of Victron Energy.

Current version: **v0.3.8** — [changelog](CHANGELOG.md) · [releases](https://github.com/Tawni-io/victron-instant-readout/releases)

---

## What this face does

Reads Victron **Instant Readout** over Bluetooth (no pairing). Up to **8** saved devices.

| Page | Kit | Shows |
| --- | --- | --- |
| Battery | SmartShunt / BMV | Voltage, charge/discharge + amps, SoC |
| Solar | MPPT | PV watts, charge state, battery V/I |
| DC-DC | Orion | Mode, in/out voltage |
| Info | This unit | Firmware version, USB vs LiPo |

Settings live on your phone, not in a cabin menu.

---

## Buttons

Two buttons. Enclosure marks the **setup** button (GPIO0). Flip Display 180 does not swap them.

| Input | Action |
| --- | --- |
| GPIO0 short | Next page |
| GPIO28 short | Previous page |
| GPIO0 long (~2 s) | Setup hotspot on/off |
| Both held (~3 s) | Soft power-off (deep sleep) |
| GPIO0 after wake (~1.5 s) | Stay on |

Swipe left/right does the same as next/previous if the touch IC is fitted.

---

## Install

### USB — first flash or recovery

Python 3.12+, [PlatformIO Core](https://platformio.org/install/cli), [Git](https://git-scm.com/downloads) on PATH, USB-C cable.

```bash
pio run -e bringup_ble -t upload
pio device monitor -b 115200
```

If upload fails: hold **BOOT**, tap **RST**, release **BOOT**, then upload again.

**Windows:** run this before upload so the flash progress bar does not hang the COM port:

```powershell
chcp 65001
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"
pio run -e bringup_ble -t upload
```

### Phone — later updates

1. Download `victron-instant-readout-t-display-c5-vX.Y.Z.bin` from [Releases](https://github.com/Tawni-io/victron-instant-readout/releases)
2. Long-press the marked setup button
3. Join Wi-Fi **VictronDash** → open `http://192.168.4.1`
4. **Firmware** → upload the `.bin` → wait for reboot

Saved devices stay after an update. Stay powered during upload.

---

## Setup (this face)

First boot with no devices: hotspot is already on. Cabin shows **SETUP MODE**.

This face’s hotspot is **VictronDash**. Other Tawni faces use their own SSID so two boxes on the bench do not collide.

1. Join **VictronDash** (open network) → `http://192.168.4.1`
2. In VictronConnect: enable Instant Readout, copy **MAC** and **encryption key**
3. SoftAP **Nearby** (or paste MAC) → Name → key → Type → **Save device**
4. **Flip Display 180** if the unit is mounted upside-down
5. **Stop hotspot** (or long-press setup) to return to cabin pages

Keys live in on-device storage. Do not put real keys in source files.

Help: [docs/SETUP.md](docs/SETUP.md) · [docs/VICTRONCONNECT_REF.md](docs/VICTRONCONNECT_REF.md)

---

## Build from source

```bash
pio run -e bringup_ble
```

Field binary: `.pio/build/bringup_ble/firmware.bin`  
Rename for a release: `victron-instant-readout-t-display-c5-vX.Y.Z.bin`

Version string: `-DVICTRONDASH_VERSION` in `platformio.ini` (keep the `#ifndef` fallbacks in `src/main.cpp` and `src/softap/softap.cpp` the same). Bump that, add a [CHANGELOG](CHANGELOG.md) entry, then tag `vX.Y.Z`.

Touch-IC gold test: `pio run -e bringup_touch -t upload`

---

## Docs

| Doc | What |
| --- | --- |
| [CHANGELOG.md](CHANGELOG.md) | Versions |
| [docs/SETUP.md](docs/SETUP.md) | Adding devices |
| [docs/FIRMWARE.md](docs/FIRMWARE.md) | OTA, slots, versioning |
| [docs/INSTANT_READOUT.md](docs/INSTANT_READOUT.md) | BLE decrypt notes |
| [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) | Victron kit matrix |
| [docs/DASHBOARD.md](docs/DASHBOARD.md) | Cabin pages |

---

## License

Firmware: [MIT](LICENSE). Tawni is not affiliated with Victron Energy B.V.

Vendored drivers keep their own licenses: [`lib/esp_lcd_st7789`](lib/esp_lcd_st7789) (LilyGO / João Brilha), [`lib/CST816S`](lib/CST816S) (Felix Biego).
