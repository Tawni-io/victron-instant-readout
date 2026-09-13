# Multi-device setup and source of truth

How a user adds Victron kit and which readings own the cabin **Battery** glance.

**Status:** Implemented in firmware — SoftAP multi-device list + NVS (up to 8), nearby `model_id` labels, automatic Battery source.

Related: [DASHBOARD.md](DASHBOARD.md) · [COMPATIBILITY.md](COMPATIBILITY.md) · [INSTANT_READOUT.md](INSTANT_READOUT.md) · [FIRMWARE.md](FIRMWARE.md) · [VICTRONCONNECT_REF.md](VICTRONCONNECT_REF.md)

---

## Why voltages disagree

| Device | Voltage meaning | Current / SoC |
| --- | --- | --- |
| SmartShunt / BMV | House battery (**best**) | Net I + SoC |
| MPPT | At that charger | Charger I only |
| Orion | In / out | DC-DC path only |
| BatteryProtect | Usually battery side | Not a full monitor |
| Battery Sense | Battery V (+ temp) | No I / SoC |

One **Battery** glance, one source — never average.

---

## SoftAP — one config page

**First boot (no key):** SoftAP starts automatically → cabin **SETUP MODE** → join `VictronDash`.  
**Later:** long-press the marked setup button (GPIO0) for the same **one page** (no Status / Devices / Help tabs). Button roles do not change when Flip Display 180 is On.

| Field | Behaviour |
| --- | --- |
| **Saved devices** | List with Remove; shows type + which is Battery source |
| **Nearby** | Product label (`model_id`) + family + MAC + RSSI; tap fills MAC and suggests Name |
| **Name** | Friendly label you choose (not from VictronConnect) |
| **MAC** + **key** | From VictronConnect Instant Readout |
| **Type** | Auto / Battery / Solar / DC-DC |
| **Firmware** | Upload app `.bin`; Factory reset erases devices and reboots to SETUP MODE ([FIRMWARE.md](FIRMWARE.md)) |
| **Stop hotspot** | Leaves SoftAP; cabin uses saved NVS list |

```text
VictronDash setup

Battery data from: House shunt

Saved devices
  House shunt · AA:BB:… · Battery · Battery source   [Remove]
  Roof MPPT   · CC:DD:… · Solar                      [Remove]

Nearby Victron (frozen while hotspot is on)
  SmartShunt 500A          ← product from model_id
  aabbccddeeff · Battery · RSSI -68 · 1s

Name  [ House shunt     ]
MAC   [ paste or tap nearby ]
Key   [ 32 hex chars…   ]
Type  [ Auto ▼ ]

[ Save device ]   [ Clear all ]

How to get MAC + key (short steps…)

[ Stop hotspot ]
```

### What nearby can and cannot show

Instant Readout ads include **MAC**, **model_id**, and **record type** (Battery / Solar / DC-DC). Many Victron radios also broadcast a BLE **GAP name** (factory-style, often with serial).

They do **not** include your VictronConnect custom name. The SoftAP **Name** field is yours — tap nearby may prefill from product/GAP label; edit freely.

A short BLE listen runs just before SoftAP starts. While the hotspot is up, BLE is paused by default so Wi‑Fi stays stable; tap **Scan nearby** for another ~5s listen without leaving SoftAP (association is kept). Strongest RSSI first. Key still comes from VictronConnect.

Max **8** devices. Saving the same MAC again updates that row.

**Set Primary** only appears later if two devices share the same priority rank. Today the first Battery/Auto device wins as Battery source.

---

## Battery glance — automatic priority

First match among **enabled** devices:

| Rank | Device | Battery screen gets |
| --- | --- | --- |
| 1 | SmartShunt / BMV (or Type=Battery / Auto) | V, I, direction, SoC, alarms |
| 2 | Lynx / Smart BMS *(when we add the type)* | Pack V / SoC if IR has it |
| 3 | Battery Sense *(later type)* | V only → SoC unavailable |
| 4 | Orion | Output V; no house SoC |
| 5 | MPPT | Battery V / charger I; no house SoC |
| 6 | BatteryProtect *(later type)* | Input V only |

SoftAP always shows: **Battery data from: &lt;name&gt;**. Cabin Battery header shows that friendly name.

### Other kit

| Kit | Role |
| --- | --- |
| MPPT | Own Solar page (GPIO0/GPIO28 cycle) |
| Orion | Own Orion page |
| Extra shunt | First Battery/Auto wins until Primary UI exists |

---

## No shunt

Still useful — Battery page falls back to MPPT or Orion voltage when that device is the priority source:

```text
|  13.4 V           (from MPPT / Orion) |
|  CHARGING         +18 A               |
|  SoC N/A — add a SmartShunt for %     |
```

Do not invent SoC from voltage (LiFePO4).

---

## Dongle

Type = what it is for (MPPT / Shunt / Auto). MAC + key = **dongle** Instant Readout details.

Exact VictronConnect taps: [VICTRONCONNECT_REF.md](VICTRONCONNECT_REF.md). SoftAP Help reuses that step list, including the PIN→key rotation warning.

---

## Cerbo / GX

Cerbo GX / Venus OS are **not** Instant Readout BLE peers for this path. Add shunt / MPPT / Orion (or their dongles) by MAC + key.

---

## Deferred

- Sense / Protect / BMS type picker entries  
- Always showing Primary UI when two shunts  
- Silent failover when Primary goes stale  
- Voltage-based SoC guess  
