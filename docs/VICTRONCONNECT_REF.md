# VictronConnect reference screenshots

Local screenshots in [`pics/`](../pics/) of VictronConnect **v6.34** on a **BMV-712 Smart**, used while building SoftAP Help and Instant Readout setup. Not part of the cabin UI product.

**Do not** copy real MAC addresses or encryption keys from these images into docs or commits. `pics/` is gitignored.

Related: [INSTANT_READOUT.md](INSTANT_READOUT.md) · [SETUP.md](SETUP.md)

---

## SoftAP Help steps (from these screens)

1. Open VictronConnect → connect to the device  
2. Gear → **Settings** → **⋮** → **Product info**  
3. Turn **Instant readout via Bluetooth** on  
4. **Instant readout details** → **SHOW** (opens encryption data)  
5. Copy **MAC Address** and **Encryption Key** into our SoftAP setup form  
6. If you later change or reset the Victron Bluetooth **PIN**, the Instant Readout key **changes** — copy it again  

Victron also warns: only share encryption data with people you trust.

---

## Screenshot index

| File | Screen | Use when building |
| --- | --- | --- |
| [`01-settings-product-info-menu.jpg`](../pics/01-settings-product-info-menu.jpg) | Settings · overflow → Product info | Navigation path for Help |
| [`01b-settings-overflow-menu.jpg`](../pics/01b-settings-overflow-menu.jpg) | Same menu (alternate crop) | Same |
| [`02-product-info-bmv712.jpg`](../pics/02-product-info-bmv712.jpg) | Product info — BMV-712 Smart | Product / serial / custom name / Bluetooth |
| [`03-product-info-instant-readout-toggle.jpg`](../pics/03-product-info-instant-readout-toggle.jpg) | Product info — Instant readout **Enabled** + Encryption data **SHOW** | Toggle + SHOW control labels |
| [`04-instant-readout-encryption-modal.jpg`](../pics/04-instant-readout-encryption-modal.jpg) | **Instant readout encryption data** modal | SoftAP field names: MAC Address, Encryption Key; PIN→key note. **Contains secrets — local only** |
| [`05-status-connected.jpg`](../pics/05-status-connected.jpg) | Connected Status tab | Full live values while paired |
| [`06-device-list-instant-readout.jpg`](../pics/06-device-list-instant-readout.jpg) | Device list Instant Readout card | Fields IR can show without connecting |

Captured against custom name / serial pattern like `SmartBMV` + HQ serial (BMV-712 Smart). App chrome: VictronConnect v6.34.

---

## Instant Readout fields (Device list card)

Useful for parser / Battery glance checks (not our layout — we stay voltage-first per [DASHBOARD.md](DASHBOARD.md)):

| Field | Notes |
| --- | --- |
| Battery voltage | House bus — source of truth with shunt |
| Current | Net A |
| State of charge | % |
| Remaining time | May show infinity when idle |
| Starter battery | **Aux** — not house voltage; do not use as Battery glance V |
| Consumed Ah | Detail / later |

Connected Status also shows Power (W) and Consumed Ah in an Output group.

---

## Setup UX notes for our SoftAP

- Accept MAC with or without separators (Victron shows continuous hex).  
- Accept 32-character hex encryption key (case-insensitive).  
- Friendly name can default from Victron custom name if user pastes it; otherwise they type e.g. `House shunt`.  
- Remind users in Help after a PIN reset to update the key.
