# ESP Somfy RTS HA Gateway

ESP8266-based Somfy RTS gateway with Home Assistant MQTT Discovery, Web UI management, and dynamic blind handling.

---

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
- Per-blind command controls: Open / Stop / Close / PROG
- Remote ID and rolling code persistence
- Dual WiFi — primary + fallback SSID
- Backup / Restore JSON
- AP portal onboarding with Wi-Fi scan
- OTA firmware update via Web UI
- Factory reset via Web UI
- Device ID normalization for MQTT safety

---

## Hardware

- ESP8266 board (Wemos D1 Mini, NodeMCU v2, or Generic ESP8266)
- 433.42 MHz RF transmitter module compatible with Somfy RTS
- Optional:
  - Status LED GPIO
  - Long-press reset button GPIO (hold 15 s for factory reset)

---

## Supported Boards

| PlatformIO environment | Board |
|---|---|
| `d1_mini` | Wemos D1 Mini |
| `nodemcuv2` | NodeMCU 1.0 (ESP-12E) |
| `esp8266_generic` | Generic ESP8266 Module |

---

## First Setup

1. Power up device.
2. Connect to AP: `ESPSomfyRemote_<deviceId>`
3. Open browser at `http://192.168.4.1`
4. Select your Wi-Fi network and enter credentials, then tap **Save & Connect**.
5. After reboot, open the device at the assigned IP.
6. Go to **Config** and set your MQTT broker details.
7. Enable Home Assistant Discovery and click **Republish HA Discovery**.

---

## Link Status LED

The status LED reflects the device state via blink patterns:

| LED pattern | Meaning |
|---|---|
| **Solid ON** | WiFi connected + MQTT connected (or no MQTT server configured) |
| **Double pulse** - ●● pause ●● pause | WiFi connected, MQTT disconnecting / reconnecting |
| **Fast blink** - 100 ms on / 100 ms off | WiFi connecting, retries in progress |
| **Slow blink** - 500 ms on / 1500 ms off | AP mode only - no WiFi or all retries exhausted |

> The LED pin and active-low logic are configurable on the **Config** page.

---

## WiFi Fallback Behavior

1. Device tries the **primary SSID** up to 5 times.
2. If all fail and a **fallback SSID** is configured, it switches and tries that up to 5 times.
3. If both are exhausted, the device opens **AP mode** and stays reachable at `192.168.4.1`.
4. Once WiFi connects (on either SSID), the AP shuts down automatically.

---

## Web UI

### Home
- Add / remove blinds
- Rename blind and select type
- Open / Stop / Close / PROG per blind
- Regenerate single blind Remote ID

### Config
- Device ID and GPIO settings (TX pin, LED, reset button)
- Primary and fallback WiFi credentials
- AP SSID and password
- MQTT broker, port, user/password
- Home Assistant Discovery toggle
- Somfy RTS: Regenerate all remote IDs / Republish HA Discovery
- Backup / Restore configuration
- Factory Reset

### Firmware Update
- View current vs latest release version
- Upload and flash `.bin` file via browser

---

## Home Assistant Entities

**Per blind:**
- `cover` entity (Open / Stop / Close)
- `button` entity for PROG

**Gateway:**
- `button`: Re-publish Discovery
- `button`: Reboot
- `sensor`: Free Heap (B)
- `sensor`: IP address
- `sensor`: Uptime (min)
- `sensor`: WiFi Signal (%)

---

## Safety Notes

- Regenerating remote IDs or resetting rolling codes requires re-pairing motors with the PROG button.
- Test PROG actions carefully on production shades/blinds.
- Factory reset erases **all** settings including WiFi, MQTT, and all blind configurations.
