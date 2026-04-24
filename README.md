# ESP Somfy RTS HA Gateway

ESP8266-based Somfy RTS gateway with Home Assistant MQTT Discovery, Web UI management, and dynamic blind handling.

## Features

- Home Assistant MQTT Discovery for:
  - Covers (blinds)
  - PROG buttons
  - Gateway config buttons (`Re-publish Discovery`, `Reboot`)
  - Gateway diagnostics (`Free Heap`, `IP`, `Uptime`, `WiFi Signal`)
- Dynamic blind management (start with 1, add/remove from Web UI)
- Per-blind settings:
  - Name
  - Type (Blind / Shade / Roller Shutter / Curtain / Awning / Window)
  - Type is mapped to HA `device_class` so icon updates automatically
- Per-blind command controls:
  - Open / Stop / Close / PROG
- Remote ID and rolling code persistence
- Backup/Restore JSON
- AP portal onboarding + Wi-Fi scan
- OTA support
- Device ID normalization for MQTT safety (displayed prettily in UI/HA)

---

## Hardware

- ESP8266 board (tested on Wemos D1 Mini style boards)
- 433.42 MHz RF transmitter module compatible with Somfy RTS
- Optional:
  - Status LED GPIO
  - Long-press reset button GPIO

---

First Setup
Power up device.
Connect to AP: ESPSomfyRemote_<deviceId>
Open portal and set Wi-Fi credentials.
Configure MQTT broker in Config page.
Enable Home Assistant Discovery.
Click Republish HA Discovery (or reboot).
Web UI
Home
Add blind
Remove any blind (red X per card)
Rename blind
Select blind type
Open / Stop / Close / PROG
Regenerate single blind Remote ID
Config
Device / GPIO settings
Wi-Fi / AP settings
MQTT settings
Regenerate all active remotes
Reboot
Republish HA Discovery
Backup / Restore
Home Assistant Entities
Per blind
cover entity
button entity for PROG
Gateway
button: Re-publish Discovery
button: Reboot
sensor: Free Heap (B)
sensor: IP
sensor: Uptime (min)
sensor: WiFi Signal (%)

Backup combines both into one JSON bundle.

Device ID Notes
Internal ID is sanitized for MQTT/discovery stability.
UI and HA display name are shown in human-friendly format (underscores displayed as spaces).
Safety Notes
Regenerating remote IDs or resetting rolling codes may require re-pairing motors.
Test PROG actions carefully on production shades/blinds.
