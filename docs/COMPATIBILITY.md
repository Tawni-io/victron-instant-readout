# Victron Instant Readout — compatibility list

Living matrix: Instant Readout kit, what we plan to decode, what we have tested.

**Enthusiast focus:** MVP is SmartShunt/BMV on the Battery glance; MPPT/Orion next; the tables below stay for capability tracking.

**Status legend:** — · Planned · In progress · Tested · Working · Blocked · N/A

**BLE path**

| Path | Meaning |
| --- | --- |
| Built-in | Device has Bluetooth; key + MAC are the device’s |
| Dongle | Needs [VE.Direct Bluetooth Smart dongle](https://www.victronenergy.com/communication-centres/ve-direct-bluetooth-smart-dongle) (fw ≥ 2.41); key + MAC are the **dongle’s** |
| VE.Bus dongle | Instant Readout via VE.Bus Smart dongle (or MK3 for wired VC — not our BLE cabin path) |
| Non-BLE IR | Instant Readout exists in VictronConnect/VRM contexts without useful cabin BLE ads for us |

Sources: [VictronConnect Instant Readout compatibility](https://www.victronenergy.com/media/pg/VictronConnect_app/en/stored-trends---instant-readout.html), open parsers ([keshavdv/victron-ble](https://github.com/keshavdv/victron-ble), [SH3D/VictronBLE](https://github.com/SH3D/VictronBLE), Home Assistant `victron_ble`). Record type hex values follow Victron extra-manufacturer-data / community tables (`0x01` solar, `0x02` battery monitor, `0x04` DC-DC, …). Firmware decode notes: [INSTANT_READOUT.md](INSTANT_READOUT.md#firmware-build-notes-sh3dvictronble).

---

## Quick status (this project)

| Priority | Device (your kit / roadmap) | Model ID | Our status | Notes |
| --- | --- | --- | --- | --- |
| 1 | BMV-712 Smart | `0xA381` | Working | IR `0x02` — cabin V / I / SoC. Tested 2026-07-20. |
| 1b | Smart Battery Sense | `0xA3A4` | Working | IR `0x02` — cabin V + temp only (no I/SoC). Tested 2026-07-20. |
| 2 | SmartSolar / BlueSolar MPPT | — | Planned (phase 2) | Multiple chargers; BlueSolar needs dongle |
| 3 | Orion-Tr Smart / Orion XS | — | Planned (phase 3) | Confirm exact Orion model when testing |
| — | Everything else below | — | — | Track here as we expand |

Update this file when you test a new product (model ID from SoftAP / Serial `model=0x…`).

---

## Master compatibility table

Columns:

- **Victron IR** — Victron documents Instant Readout support  
- **Stored trends** — separate VictronConnect feature (history); we do **not** implement this  
- **IR record** — typical Instant Readout inner record family (where known)  
- **Dashboard** — our planned/implemented support  
- **Tested** / **Working** — fill in as we go (`Yes` / `No` / date)

### Battery monitors & shunts

| Product | BLE path | Victron IR | Stored trends | IR record | Typical IR fields | Dashboard | Tested | Working |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SmartShunt (all sizes) | Built-in | Yes | Yes | `0x02` | V, I, SoC, Ah, TTG, alarm, aux | Planned MVP | No | No |
| SmartShunt IP65 | Built-in | Yes | Yes | `0x02` | Same as SmartShunt | Planned MVP | No | No |
| BMV-712 Smart (`0xA381`) | Built-in | Yes | Yes | `0x02` | V, I, SoC, alarm | Working | 2026-07-20 | Yes |
| BMV-712 Smart / SmartShunt with mid/temp/starter aux | Built-in | Yes | Yes | `0x02` | Aux mode + value | Partial (Sense uses temp aux) | See Sense | — |
| BMV-702 | Dongle | Yes | No* | `0x02` | Via dongle Instant Readout | Planned (dongle path) | No | No |
| BMV-700 | Dongle | Yes | No* | `0x02` | Via dongle Instant Readout | Planned (dongle path) | No | No |

\*Stored trends on older BMV + dongle may differ; Instant Readout is the column that matters for us.

### Solar chargers (MPPT)

| Product | BLE path | Victron IR | Stored trends | IR record | Typical IR fields | Dashboard | Tested | Working |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SmartSolar MPPT (VE.Direct) | Built-in | Yes | Yes | `0x01` | Batt V/I, PV power, charge state, yield, load | Planned phase 2 | No | No |
| SmartSolar MPPT VE.Can | Built-in* | Yes | Yes | `0x01` | Same family (confirm per model) | Planned phase 2 | No | No |
| BlueSolar MPPT | Dongle | Yes | No* | `0x01` | Via dongle | Planned phase 2 | No | No |
| MPPT RS | Built-in | Yes | No | (RS family) | Solar / system snapshot | Later | No | No |
| SolarSense 750 | Built-in | Yes | Yes | (sense) | Irradiance / related | Later / low priority | No | No |

### DC-DC chargers (Orion)

| Product | BLE path | Victron IR | Stored trends | IR record | Typical IR fields | Dashboard | Tested | Working |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Orion-Tr Smart DC-DC (isolated & non-isolated) | Built-in | Yes | No | `0x04` | In V, out V, mode, error / off reason | Planned phase 3 | No | No |
| Orion XS 12/12-50A | Built-in | Yes | No | `0x0F` or variant* | Similar DC-DC fields | Planned phase 3 (verify layout) | No | No |

\*Orion XS may use a newer record layout than classic `0x04`. Confirm against capture + VictronConnect when testing.

### Inverters & inverter/chargers

| Product | BLE path | Victron IR | Stored trends | IR record | Typical IR fields | Dashboard | Tested | Working |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Phoenix Inverter Smart | Built-in | Yes | No | `0x03` | AC/mode/alarm style fields | Later | No | No |
| Sun Inverter | Built-in | Yes | No | `0x03` | Similar | Later | No | No |
| Phoenix Inverter VE.Direct | Dongle | Yes | No | `0x03` | Via dongle | Later | No | No |
| Inverter RS | Built-in | Yes | No | `0x06` | RS snapshot | Later | No | No |
| Multi RS | Built-in | Yes | No | `0x0B` | Multi RS snapshot | Later | No | No |
| VE.Bus Inverter/Charger (MultiPlus, Quattro, …) | VE.Bus dongle | Yes | No | `0x0C` | Via VE.Bus Smart dongle ads | Later | No | No |
| VE.Bus Smart dongle (alone as IR radio) | Built-in | Yes | No | `0x0C` | Proxies VE.Bus Instant Readout | Later | No | No |

### Batteries, BMS, protect, sense

| Product | BLE path | Victron IR | Stored trends | IR record | Typical IR fields | Dashboard | Tested | Working |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Lithium Battery Smart | Built-in | Yes | No | `0x05` | Pack V, temp, cells / balancer | Later | No | No |
| Lynx Smart BMS | Built-in | Yes | No | `0x0A` | BMS / bus status | Later | No | No |
| Smart BMS 12-200 / CL | Built-in | Yes | No | (BMS) | Charge allow / status | Later | No | No |
| Smart BatteryProtect | Built-in | Yes | No | `0x09` | In/out V, state, alarms | Later | No | No |
| Smart Battery Sense (`0xA3A4` / `0xA3A5`) | Built-in | Yes | Yes | `0x02` | V, temperature (aux) | Working (Sense layout) | 2026-07-20 | Yes |

### Chargers, meters, other

| Product | BLE path | Victron IR | Stored trends | IR record | Typical IR fields | Dashboard | Tested | Working |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Blue Smart IP22 / IP65 / IP67 chargers | Built-in | Yes* | No | `0x08` | Charge state, V/I | Later | No | No |
| DC Energy Meter | Built-in | Yes | No | `0x0D` | Meter channels | Later | No | No |
| VM-3P75CT Energy Meter | Non-BLE IR | Yes (wired/VRM) | No | — | Not a cabin BLE target | N/A | N/A | N/A |
| VE.Direct Bluetooth Smart dongle | Built-in | Yes | No | *of attached product* | IR for BlueSolar / BMV-700/702 / Phoenix VE.Direct | Planned (as BLE radio) | No | No |

\*Confirm Instant Readout per Blue Smart charger SKU in VictronConnect Product info; not every Bluetooth Victron product is listed in Victron’s IR table.

### Explicitly out of Instant Readout (examples)

Products can have Bluetooth for VictronConnect **without** Instant Readout. If Victron does not list them in the Instant Readout table, treat as **N/A** until proven otherwise with a BLE capture.

| Product class | Notes for this project |
| --- | --- |
| GX devices (Cerbo, Ekrano, …) | Not Instant Readout BLE sources for our cabin gauge |
| Many non-listed Bluetooth accessories | Capture CSV if unsure; do not assume IR |

---

## How to update this file after a test

1. Enable Instant Readout; copy MAC + 32-hex key (device or dongle).  
2. Run Stage A sniffer or SoftAP **BLE capture** (Victron-only).  
3. Confirm decrypt + fields match VictronConnect Instant Readout.  
4. Edit the row:

```text
| SmartShunt 500A | Built-in | Yes | Yes | 0x02 | … | Working | 2026-07-12 | Yes |
```

5. Add a short note under **Test log** below.

### Test log

| Date | Product | Model ID | Firmware / notes | Result |
| --- | --- | --- | --- | --- |
| 2026-07-20 | BMV-712 Smart (HQ2120UVM3M) | `0xA381` | IR decrypt OK; cabin V/I/SoC; multi-page with Sense | Working |
| 2026-07-20 | Smart Battery Sense (HQ21029XHLM) | `0xA3A4` | IR decrypt OK; V matches VictronConnect (~11.1 V); temp via aux; cabin Sense layout (V + °C). Ignore VE.Smart ads (`0x5D`). | Working |

---

## Feature wishlist vs Instant Readout limits

| Want on cabin display | Available via Instant Readout? | Notes |
| --- | --- | --- |
| Live V / I / SoC from shunt | Yes | MVP |
| Live MPPT PV watts / charge state | Yes | Phase 2 |
| Orion input/output / mode | Yes | Phase 3 |
| Multi-day graphs (Stored trends) | No (needs connect) | Out of scope |
| Change Victron settings from cabin UI | No | VictronConnect / GX only |
| Multiple rear devices on one display | Yes | One key+MAC per BLE radio |

---

## References

- [README.md](../README.md) — what this Firmware ships  
- [INSTANT_READOUT.md](INSTANT_READOUT.md) — sniff / decrypt (+ VictronBLE firmware notes)  
- [DASHBOARD.md](DASHBOARD.md) — UI  
- [SETUP.md](SETUP.md) — devices & source of truth  
- [FIRMWARE.md](FIRMWARE.md) — releases & OTA  
- [VICTRONCONNECT_REF.md](VICTRONCONNECT_REF.md) — VictronConnect screenshots / Help steps  
- [Victron Instant Readout manual](https://www.victronenergy.com/media/pg/VictronConnect_app/en/stored-trends---instant-readout.html)  
- [VE.Direct Bluetooth Smart dongle](https://www.victronenergy.com/communication-centres/ve-direct-bluetooth-smart-dongle)  
- [SH3D/VictronBLE](https://github.com/SH3D/VictronBLE) — C++ Instant Readout library (MIT); candidate `lib_deps` when building  
- [keshavdv/victron-ble](https://github.com/keshavdv/victron-ble) — Python protocol reference  

