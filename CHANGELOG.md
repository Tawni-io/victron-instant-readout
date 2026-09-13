# Changelog

On-device string: `VICTRONDASH_VERSION` in `platformio.ini`.  
Release binaries: `victron-instant-readout-t-display-c5-vX.Y.Z.bin`

Newest first.

## 0.3.8

- Auto soft-off when the board LiPo is discharging at **≤ 3.20 V** for ~4 s (before backlight fade / pack UVLO ~3.10 V)
- USB charging stays awake so a flat cell can recover; cabin shows **BATTERY EMPTY** then the same deep-sleep path as dual-button
- Info treats ≥ 2.50 V as a real cell (open protection still reads ~0 → `No LiPo`)

## 0.3.7

- Soft power: hold both buttons 3 s → deep sleep (backlight/panel off, BLE paused)
- Wake: hold GPIO0 ~1.5 s (USB/RST cold boot stays on; no pocket re-sleep)
- SoftAP long-press unchanged (GPIO0 alone); soft-off blocked during OTA

## 0.3.6

- SoftAP page stream yields periodically (avoid watchdog reboot kicking phone + cabin out of setup)
- Safer LVGL suspend: tiny placeholder buffer instead of NULL draw buf
- Serial reasons for SoftAP leave (GPIO long / stop)
- SoftAP header shows `v0.3.6`

## 0.3.5

- SoftAP re-entry without power cycle: free LVGL DMA draw buffer while hotspot is up
- Longer WIFI_OFF settle + SoftAP start retry; longer teardown before BLE resume
- SoftAP header shows `v0.3.5`

## 0.3.4

- SoftAP setup page: native `chunkResponseBegin` + 192-byte flash windows (no big heap page)
- Nearby JSON no longer copied into an Arduino String (SoftAP heap fix)
- SoftAP Flip note: long-press marked setup button after flip
- Confirm flash via SoftAP header `v0.3.4`

## 0.3.3

- SoftAP **Flip Display 180** setting (Off / On); landscape MADCTL mirror presets
- Flip persisted in NVS (`orient`); survives OTA; cleared on Factory reset
- Cabin buttons not remapped on flip (enclosure marks setup button)
- SoftAP setup page streamed in chunks (no big heap buffer)

## 0.3.2

- Cabin footer page dots (count + active page accent)
- Info page: USB vs LiPo from AXP2602 charge current
- Dashboard / touch polish for multi-page cabin cycle

## 0.3.1

- SoftAP offline firmware upload (OTA to inactive slot)
- SoftAP Factory reset
- NVS devices/keys survive OTA
- Cabin Info page shows firmware version
