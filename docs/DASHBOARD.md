# Dashboard UI — LILYGO T-Display C5

Cabin look & feel.

Related: [SETUP.md](SETUP.md) · [INSTANT_READOUT.md](INSTANT_READOUT.md) · [FIRMWARE.md](FIRMWARE.md) · [COMPATIBILITY.md](COMPATIBILITY.md)

**Status:** Implemented — Battery / Solar / Orion / Info pages + SoftAP multi-device setup; GPIO0/GPIO28 page cycle + CST816S swipe (when touch IC responds). Header shows SoftAP device Name.

---

## Intent

Front-of-vehicle gauge; Victron kit in the rear. One look:

1. **Voltage**
2. **Charging / discharging + rate**
3. **SoC** (when a shunt/BMV is the source)

Landscape **320×170**. SoftAP **Flip Display 180** for mount orientation — no auto-rotate, no portrait. Button roles stay fixed (GPIO0 = setup long-press); after a flipped mount that control may sit at the top of the readable UI — mark it on the enclosure, not as “bottom”. Buttons always work; swipe is for future touch hardware when the CST816S is present.

---

## Hardware

| Item | Value |
| --- | --- |
| Panel | ST7789 IPS LCD, landscape 320×170 (SoftAP Flip Display 180) |
| Nav | GPIO0 next / long = SoftAP (marked setup button); GPIO28 previous. Roles do not swap when Flip is On |
| Touch | CST816S capacitive (I2C) — wiki pins below; `-DUSE_TOUCH_SWIPE=1` (default). Boot Serial: I2C scan + `Touch:` / `int_edges` |
| Power | Prefer van USB-C; board LiPo via AXP2602 on the Info page |

### CST816S pins (no jumper)

[LilyGO wiki](https://wiki.lilygo.cc/products/t-display-series/t-display-c5/) and [T-Display-C5 pin map](https://github.com/Xinyuan-LilyGO/T-Display-C5) agree with [`include/board_config.h`](../include/board_config.h):

| Signal | GPIO |
| --- | --- |
| SDA / SCL | 2 / 3 |
| INT / RST | 27 / 24 |

LilyGO also marks touch **optional** (`Touch (optional)` in their readme; factory `CONFIG_TOUCH_CST816S` defaults off). Schematic nets (`NLTP0INT` / `NLTP0RST` / `CTP`) exist, but Amazon “touch” units can still ship without the CTP populated.

**Idle I2C:** CST816S often **NACKs at 0x15 while asleep** and only ACKs after a touch/IRQ wake. `probe0x15=0` alone is not proof the IC is missing. A live chip should pulse **INT** while touching — watch `int_edges` in Serial.

**Acceptance test (before RMA):** flash the stock-style bringup:

```bash
pio run -e bringup_touch -t upload
pio device monitor -e bringup_touch
```

Touch/swipe ~30 s. **Pass:** gesture / X/Y lines and rising `int_edges`. **Fail:** `int_edges` stays 0 on ≥2 boards → treat as missing/unpowered CTP → Amazon RMA. (Upstream LilyGO `examples/touch` is the same idea; our env adds real `TP_RST`, `FALLING` IRQ, and sleep-disable `0xFE=0x07`.)

---

## Design rules

1. One job per screen  
2. Biggest number = voltage on Battery; PV W on Solar; mode on Orion  
3. Say `CHARGING` / `DISCHARGING` / `IDLE` — don’t make people read the sign of amps alone  
4. High contrast, large type  
5. Honest when stale  
6. Colour = **status** (ok / warn / alarm) **and page identity** (accent + footer page dots) — red reserved for alarms  

### Page shell (all device pages)

Same chrome on every configured-device cabin page; only hero + metric row change:

1. Top accent strip  
2. Family chip (`SHUNT` / `SENSE` / `SOLAR` / `ORION` — later `PROTECT`)  
3. SoftAP **Name**  
4. BLE / status pill  
5. One large **hero**  
6. **1–3 metric cards** — only fields Instant Readout actually provides  
7. Optional SoC bar / alarm banner when that family has them  
8. Footer: age · page dots · RSSI  

**Capability rule:** if the device does not publish a field, that card does not exist (Sense = no amps/watts/SoC).  

**Variant rule:** same IR family with a field subset (Sense vs shunt on `0x02`) may share a screen with an explicit layout mode. A different IR record (e.g. Protect `0x09`) gets its own page kind — do not morph shunt cards into Protect UI.  

MPPT / Orion already use one carousel page per SoftAP device on a shared screen. Battery Protect is planned as a first-class page when hardware is available.

### Colour tokens

| Token | Use | Colour |
| --- | --- | --- |
| `--bg` | Background | `#0D1117` (factory-style) |
| `--card` | Metric cards | `#161B22` |
| `--fg` | Primary text | `#FFFFFF` |
| `--muted` | Labels | `#8B949E` |
| `--ok` | Fresh / charging / high SoC | `#00E676` |
| `--warn` | Stale / mid SoC | `#FFD600` |
| `--alarm` | Alarm / low SoC | `#FF1744` |

### Page accents

Each cabin page has a unique non-red accent (top strip + family/title + hero unit / identity card bars). Defaults live in `g_page_accent[]` in `dashboard.cpp` — SoftAP-customizable later.

| Page | Accent | Hex |
| --- | --- | --- |
| Battery (shunt) | Cyan | `#00BCD4` |
| Battery Sense | Mint | `#1DE9B6` |
| Solar | Amber | `#FFB300` |
| Orion | Orange | `#FF9100` |
| Info | Violet | `#B388FF` |

---

## Screens

| Screen | When | Hero |
| --- | --- | --- |
| **Battery** | Priority source (shunt first) | Voltage |
| **Solar** | Each configured MPPT | PV W |
| **Orion** | Each configured DC-DC | Mode / out V |
| **Info** | Always last in the cycle | Status summary (read-only) |
| **Setup overlay** | SoftAP active | `SETUP MODE` + `VictronDash` (not the battery glance) |

Cycle configured device pages, then Info. Wrap with GPIO0 / GPIO28 or swipe.

---

## Battery screen (MVP)

Source of truth: [SETUP.md](SETUP.md) (shunt first, automatic).

```text
+--------------------------------------+
| SHUNT  HOUSE SHUNT          (BLE)    |
|                                      |
|  13.55                    V          |  ← large hero (dash-readable)
|                                      |
| ┌ STATUS ┐ ┌ AMPS ┐ ┌ WATTS ┐       |  ← factory-style cards
| │ CHARGE │ │ -0.0 │ │  0   │       |
| └────────┘ └──────┘ └──────┘       |
| ████████████░░░░          87%        |  ← SoC only here (not in header)
| Updated 2s      o ● o o      RSSI -70|  ← page dots (accent = active)
+--------------------------------------+
```

Inspired by LilyGO T-Display-C5 factory dashboard (dark cards + accent bars), with **voltage kept as the hero** for cabin use.

Header title is the SoftAP **Name** for the priority Battery source. Family chip is `SHUNT` (cyan) for BMV / SmartShunt.

| Region | Content |
| --- | --- |
| Header | `SHUNT` · device name · BLE/status pill |
| Hero | Large voltage + cyan **V** |
| Cards | Status · Amps · Watts (accent bars) |
| Remaining | SoC bar + right `%` |
| Footer | Updated age · page dots (center) · RSSI |
| Alarm | Red banner under header |

Shunt current: `>±0.1 A` → charging/discharging, else idle.

### Battery Sense layout

Smart Battery Sense (`0xA3A4` / `0xA3A5`) only has **voltage + temperature** in Instant Readout (same as VictronConnect). Same Battery page slot; layout switches when `model_id` is Sense:

```text
+--------------------------------------+
| SENSE  SmartBatterySense    (BLE)    |
|  11.13                    V          |
| ┌ TEMP ──────────────────────────┐  |
| │ 15.0 C                         │  |
| └────────────────────────────────┘  |
| Updated 2s      o ● o o      RSSI -50|
+--------------------------------------+
```

No SoC bar / amps / watts (those fields are unused on Sense).

---

## Solar / Orion pages

**Solar:** big PV W, charge state, batt V / I as secondary. One page per configured MPPT (header = device Name).

**Orion:** mode hero, out V, in V secondary. One page per configured DC-DC.

---

## Info screen

Read-only status page — always last in the cabin cycle (not a settings maze). SoftAP **SETUP MODE** still replaces the cabin while the hotspot is up.

```text
+--------------------------------------+
| INFO                         v0.3.2  |
| ┌ DEVICES ─────────────────────────┐ |
| │ 2 configured                     │ |
| └──────────────────────────────────┘ |
| ┌ POWER ──────┐ ┌ SETUP ───────────┐ |
| │ USB         │ │ Not active       │ |
| │ LiPo 3.85 V │ │ 192.168.4.1      │ |
| └─────────────┘ └──────────────────┘ |
|               o o o ●                |  ← page dots (last in cycle)
+--------------------------------------+
```

| Region | Content |
| --- | --- |
| Header | `INFO` · firmware `VICTRONDASH_VERSION` |
| Devices | Count only (`N configured` / `None configured`) |
| Power | Operating source `USB` or `LiPo` (from AXP2602 charge current) · LiPo voltage when present |
| Setup | `Active` / `Not active` · SoftAP IP always shown (`192.168.4.1`) |
| Footer | Centered page dots (same strip as other cabin pages) |

---

## Interaction

| Input | Action |
| --- | --- |
| GPIO0 short | Next page |
| GPIO0 long (~2 s) | SoftAP on/off — marked **setup** button on the enclosure (auto-on at boot if no key saved) |
| GPIO28 short | Previous page |
| Swipe left / up (CST816S) | Next page (ignored in SoftAP) |
| Swipe right / down (CST816S) | Previous page (ignored in SoftAP) |
| SoftAP phone | **One page** — saved devices + nearby + add + help + stop |

**Page dots:** centered in the cabin footer (between age and RSSI). Inactive = dim circles; active = short accent pill in the current page colour. Hidden when only one page, and on SoftAP / needs-setup / message screens. Not tappable — GPIO and swipe remain the only navigation.

Soft power (both buttons ~3 s → deep sleep; GPIO0 hold to wake) shipped in 0.3.7. **Board LiPo empty** (0.3.8): discharging **≤ 3.20 V** for ~4 s → **BATTERY EMPTY** → same deep sleep (blocked during OTA; USB charge stays awake). Dim after ~90 s later. **Board temp (AXP2602) later** — Info readout; if **> ~50 °C** auto soft-off (cabin empty / not viewing; AU summer). Buttons remain the fallback if the touch IC is missing or silent. Boot Serial: `I2C scan` + `Touch: armed` / periodic `Touch: INT=… confirmed=… int_edges=…`. If `int_edges` never rises while touching on a sold-as-touch unit (after `bringup_touch`), treat as hardware/RMA — not a VictronDash software bug.

### SoftAP screen (cabin)

While the hotspot is up, replace the Battery glance:

```text
+--------------------------------------+
| SETUP MODE                           |
|                                      |
|  Wi-Fi: VictronDash                  |
|  http://192.168.4.1                  |
|  Join on your phone                  |
|  Open that address                   |
|  Long-press setup to cancel          |
+--------------------------------------+
```

| Condition | UI |
| --- | --- |
| Fresh | Normal |
| &gt; ~45 s quiet | **No signal**, dim numbers |
| Alarm | Red banner, keep numbers |
| No key / first boot | Auto **SETUP MODE** + SoftAP (not an empty battery gauge) |
| SoftAP active | **SETUP MODE** overlay (above) |
| Bad key | **Bad key** |

---

## Later (not on the glance)

Victron temps / aux / Ah detail · tanks · board LiPo % on glance · **AXP board temp + auto soft-off above ~50 °C (AU summer)** · BLE capture UI (Help → Advanced) · SoftAP PIN · optional USB-plug wake from soft-off

---

## Acceptance

- [x] Big voltage, clear charge/discharge + SoC when shunt present  
- [x] Stale / alarm honest  
- [x] Buttons cycle pages; long-press SoftAP  
- [x] SoftAP: multi-device Name/MAC/key; Battery source from priority  
- [x] Nearby shows product label (`model_id`) / GAP name hints  
- [ ] Readable at arm’s length (field check)  
