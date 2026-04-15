# ESP-NOW Multi-Board Blink

A minimal project demonstrating peer-to-peer communication between multiple ESP32 boards using **ESP-NOW**. Press the button on any board and all the others blink their onboard LED.

Every board runs the exact same firmware — no configuration, no pairing, no Wi-Fi network required.

## Table of Contents

- [What is ESP-NOW](#what-is-esp-now)
- [How This Project Works](#how-this-project-works)
  - [Network Topology](#network-topology)
  - [Message Format](#message-format)
  - [Firmware Behavior](#firmware-behavior)
  - [Self-Filtering](#self-filtering)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements (macOS)](#software-requirements-macos)
- [Getting Started](#getting-started)
  - [1. Install Dependencies](#1-install-dependencies)
  - [2. Connect the Boards](#2-connect-the-boards)
  - [3. Build and Flash](#3-build-and-flash)
  - [4. Test](#4-test)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [How ESP-NOW Works Under the Hood](#how-esp-now-works-under-the-hood)
  - [Protocol Overview](#protocol-overview)
  - [Channel and Encryption](#channel-and-encryption)
  - [Delivery Model](#delivery-model)
- [Limitations](#limitations)
  - [Protocol Limitations](#protocol-limitations)
  - [Project-Specific Limitations](#project-specific-limitations)
- [Power Consumption](#power-consumption)
  - [Current Consumption Breakdown](#current-consumption-breakdown)
  - [Optimization Strategies](#optimization-strategies)
    - [1. Light Sleep Between Polls](#1-light-sleep-between-polls)
    - [2. Deep Sleep with External Wake-Up (Sender-Only Boards)](#2-deep-sleep-with-external-wake-up-sender-only-boards)
    - [3. Reduce Wi-Fi TX Power](#3-reduce-wifi-tx-power)
    - [4. Disable Serial Output](#4-disable-serial-output)
    - [5. Use an External Low-Power LED](#5-use-an-external-low-power-led)
    - [6. Lower CPU Frequency](#6-lower-cpu-frequency)
    - [7. Combined Example](#7-combined-example)
  - [Battery Life Estimates](#battery-life-estimates)
- [Troubleshooting](#troubleshooting)
- [Going Further](#going-further)
- [License](#license)

---

## What is ESP-NOW

ESP-NOW is a connectionless communication protocol developed by Espressif. It operates on the Wi-Fi physical layer (2.4 GHz) but does **not** require a Wi-Fi access point, router, or TCP/IP stack. Think of it as a lightweight, low-latency radio link between ESP devices.

Key characteristics:

| Property | Value |
|---|---|
| Max payload per frame | 250 bytes |
| Max peers (encrypted) | 10 |
| Max peers (unencrypted) | 20 |
| Latency | Typically < 5 ms |
| Range (open air) | ~200 m (line of sight, default TX power) |
| Range (indoors) | ~30-80 m depending on walls and interference |
| Frequency | 2.4 GHz (same as Wi-Fi) |
| Encryption | Optional (CCMP, derived from LMK) |
| Requires AP/router | No |

---

## How This Project Works

### Network Topology

The project uses a **broadcast topology**. Every board sends to the broadcast MAC address (`FF:FF:FF:FF:FF:FF`), which means every ESP-NOW device on the same Wi-Fi channel receives the message. There is no need to register individual peers or know MAC addresses in advance.

```
   ┌─────────┐         broadcast          ┌─────────┐
   │  ESP32   │ ──────────────────────────>│  ESP32   │
   │  Board A │                            │  Board B │
   │ (button) │<──────────────────────────>│  (blink) │
   └─────────┘                             └─────────┘
        │              broadcast                │
        │                                       │
        └──────────────>┌─────────┐<────────────┘
                        │  ESP32   │
                        │  Board C │
                        │  (blink) │
                        └─────────┘
```

Any board can be the sender at any time — the roles are symmetric.

### Message Format

The firmware uses a compact 7-byte message:

```
Byte 0       : Message type (0x01 = BLINK)
Bytes 1-6    : Sender MAC address (6 bytes)
```

The struct is packed to avoid padding, ensuring consistent layout across different compiler/alignment settings:

```cpp
struct __attribute__((packed)) Message {
    MsgType type;       // 1 byte
    uint8_t sender[6];  // 6 bytes
};
```

### Firmware Behavior

Every board executes this loop:

1. **Poll the button** (GPIO0 / BOOT). If pressed (active LOW), broadcast a `MSG_BLINK` frame via ESP-NOW. A brief 50 ms LED flash confirms the send locally.
2. **Check for received blink requests**. If the `onReceive` callback set the `shouldBlink` flag, blink the LED 3 times (200 ms on, 200 ms off each).
3. Repeat.

The `onReceive` callback runs in the Wi-Fi task context (not `loop()`), so it only sets a volatile flag. The actual LED blinking happens in `loop()` to avoid blocking the Wi-Fi stack.

### Self-Filtering

Because the broadcast address is used, the sender also receives its own message. The firmware embeds the sender's MAC in every message and compares it on reception — messages from self are silently discarded.

---

## Hardware Requirements

- **2 or more ESP32 dev boards** (ESP32-DevKitC, NodeMCU-32S, ESP32-WROOM, or similar)
- **USB cables** (one per board, for flashing; afterward only for power)
- Boards must have:
  - A **BOOT button** on GPIO0 (virtually all dev boards do)
  - An **onboard LED** on GPIO2 (most common; see [Configuration](#configuration) if yours differs)

> **Note:** ESP32-S2, ESP32-S3, ESP32-C3, and ESP32-C6 variants also support ESP-NOW, but pin assignments for the button and LED may differ. Check your board's schematic and adjust `BUTTON_PIN` and `LED_PIN` in `main.cpp`.

---

## Software Requirements (macOS)

| Tool | Purpose | Install |
|---|---|---|
| [PlatformIO CLI](https://platformio.org) | Build, upload, monitor | `brew install platformio` or `pip install platformio` |
| USB-to-UART driver | Communicate with ESP32 over USB | See below |

### USB Drivers

Most ESP32 dev boards use one of two USB-to-UART bridge chips:

- **CP210x** (Silicon Labs) — install the VCP driver from [silabs.com](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- **CH340/CH9102** (WCH) — `brew install --cask wch-ch34x-usb-serial-driver`

After installing, unplug and re-plug the boards. Verify with:

```bash
ls /dev/cu.usbserial-* /dev/cu.wchusbserial-* /dev/cu.SLAB_USBtoUART* 2>/dev/null
```

You should see one device per connected board.

---

## Getting Started

### 1. Install Dependencies

```bash
brew install platformio
# Install the appropriate USB driver (see above)
```

### 2. Connect the Boards

Plug all ESP32 boards into your Mac via USB. Each board appears as a separate serial port.

### 3. Build and Flash

```bash
./deploy.sh
```

This will:
1. Build the firmware once.
2. Detect all connected ESP32 serial ports.
3. Flash each board sequentially.

To also open a serial monitor on the last board after flashing:

```bash
./deploy.sh --monitor
```

To flash a single specific board manually:

```bash
pio run -e esp32 -t upload --upload-port /dev/cu.usbserial-XXXX
```

### 4. Test

1. Open serial monitors on two boards in separate terminals:
   ```bash
   pio device monitor -p /dev/cu.usbserial-XXXX -b 115200
   ```
2. Press the **BOOT** button on one board.
3. Observe the other boards' LEDs blink 3 times.
4. The sender's serial output shows `Button pressed — sending blink`.
5. The receivers' serial output shows `Blink received`.

---

## Project Structure

```
lab-esp32now/
├── platformio.ini      # PlatformIO project configuration
├── src/
│   └── main.cpp        # Firmware source (runs on all boards)
├── deploy.sh           # Build & flash all connected boards
└── README.md           # This file
```

---

## Configuration

These constants at the top of `src/main.cpp` can be adjusted:

| Constant | Default | Description |
|---|---|---|
| `BUTTON_PIN` | `0` | GPIO connected to the button (BOOT = GPIO0 on most boards) |
| `LED_PIN` | `2` | GPIO connected to the onboard LED |
| `BLINK_COUNT` | `3` | Number of blinks when a message is received |
| `BLINK_MS` | `200` | Duration of each blink on/off phase (ms) |
| `DEBOUNCE_MS` | `300` | Minimum time between accepted button presses (ms) |

---

## How ESP-NOW Works Under the Hood

### Protocol Overview

ESP-NOW sits directly on top of the IEEE 802.11 vendor-specific action frame. The data flow is:

```
Application
    │
    ▼
esp_now_send()          ← your code calls this
    │
    ▼
Wi-Fi driver            ← constructs a vendor action frame
    │
    ▼
2.4 GHz radio           ← transmitted over the air (no association needed)
    │
    ▼
Remote Wi-Fi driver     ← receives the frame
    │
    ▼
esp_now_recv_cb()       ← your callback is invoked with the payload
```

There is no connection establishment, no handshake, no acknowledgement at the application layer (though the MAC layer may ACK unicast frames). This makes ESP-NOW extremely fast — typical end-to-end latency is under 5 ms.

### Channel and Encryption

- All devices must be on the **same Wi-Fi channel** to communicate. This project hardcodes channel 1. If you have heavy 2.4 GHz interference on channel 1, change it in `setup()`.
- Encryption is **disabled** in this project for simplicity. ESP-NOW supports CCMP encryption with a Local Master Key (LMK) per peer, but this requires registering individual peers (not broadcast). See Espressif's documentation for encrypted ESP-NOW.

### Delivery Model

- **Broadcast frames**: No MAC-layer ACK. The sender does not know if anyone received the message. This is a fire-and-forget model.
- **Unicast frames**: MAC-layer ACK is used. `esp_now_send()` reports success/failure via a send callback. This project uses broadcast only.

---

## Limitations

### Protocol Limitations

| Limitation | Detail |
|---|---|
| **Max payload** | 250 bytes per frame. Larger data requires fragmentation at the application level. |
| **No delivery guarantee (broadcast)** | Broadcast frames are not acknowledged. Messages can be lost due to interference, collisions, or the receiver being asleep. |
| **Peer limit** | Max 20 unencrypted peers or 10 encrypted peers per device. Not an issue with broadcast, but relevant if switching to unicast. |
| **Same channel required** | All boards must be on the same Wi-Fi channel. If any board also connects to a Wi-Fi AP, it will be forced to the AP's channel, potentially breaking ESP-NOW communication with boards on a different channel. |
| **Range** | Limited by 2.4 GHz propagation. Walls, metal, and interference reduce range significantly. No mesh/relay capability built in. |
| **No routing/mesh** | ESP-NOW is single-hop only. If board A can't reach board C, board B won't relay for it. A mesh layer must be implemented manually. |
| **Interference with Wi-Fi** | ESP-NOW shares the 2.4 GHz band. Heavy Wi-Fi traffic on the same channel degrades reliability. |

### Project-Specific Limitations

| Limitation | Detail |
|---|---|
| **Blocking blink** | While a board is blinking (3 x 400 ms = 1.2 s), it cannot process new messages or button presses. Messages arriving during a blink are lost. |
| **No message queue** | The `shouldBlink` flag is a single boolean. If multiple blink commands arrive before the first is handled, they collapse into one blink sequence. |
| **No sender identification in LED** | All blink commands look the same to the receiver. There is no visual indication of which board sent the command. |
| **Polling-based button** | The button is polled in `loop()` with `delay()` calls during blinking. An interrupt-driven approach would be more responsive. |
| **No encryption** | Messages are sent in the clear. Any ESP32 on the same channel can receive and send blink commands. |
| **Single message type** | Only `MSG_BLINK` exists. The `MsgType` enum is designed for extension but no other types are implemented. |

---

## Power Consumption

The ESP32 is not inherently a low-power chip — its Wi-Fi radio is the dominant consumer. Understanding where the energy goes is the first step to optimizing it.

### Current Consumption Breakdown

Typical current draw for an ESP32-WROOM-32 module (not the full dev board):

| State | Current | Notes |
|---|---|---|
| Active, Wi-Fi TX | 160-260 mA | Depends on TX power level (default is max) |
| Active, Wi-Fi RX/listen | 95-100 mA | Radio on, listening for frames |
| Active, CPU only (no radio) | 30-50 mA | Wi-Fi/BT off, both cores running |
| Light sleep | 0.8 mA | CPU paused, Wi-Fi off, RTC + ULP running, GPIO wake |
| Deep sleep | 10 uA | Only RTC + ULP active, main CPU and Wi-Fi off, GPIO/timer wake |
| Hibernation | 5 uA | Only RTC timer active, no GPIO wake |

**Dev board overhead**: USB-to-UART bridge, voltage regulator, and power LED add 20-50 mA constant draw. For battery-powered deployments, use a bare module (ESP32-WROOM) with an efficient LDO instead of a dev board.

**Current firmware**: The radio is always on in RX/listen mode, and the CPU is polling in a tight loop. This means **~100-120 mA constant draw** on the module (plus dev board overhead). On a 1000 mAh LiPo, this gives roughly 8-10 hours.

### Optimization Strategies

Listed from easiest to most impactful:

#### 1. Light Sleep Between Polls

The simplest win. Instead of a tight `loop()`, sleep for a few milliseconds between iterations. The Wi-Fi radio can remain active during light sleep if configured, but even basic CPU light sleep saves significant current.

```cpp
#include "esp_sleep.h"

void loop() {
    // ... button check and blink logic ...

    // Sleep 10 ms between polls — saves CPU power while keeping
    // the loop responsive enough for button presses
    esp_sleep_enable_timer_wakeup(10 * 1000); // 10 ms in microseconds
    esp_light_sleep_start();
}
```

**Savings**: Reduces average CPU current from ~30 mA to ~5 mA during idle periods.

**Trade-off**: Adds up to 10 ms latency to button response. Adjust the sleep duration to balance responsiveness and power.

> **Important**: Light sleep with Wi-Fi active requires specific configuration (Wi-Fi modem sleep + DTIM listen interval). In this project, since ESP-NOW doesn't use an AP, light sleep will turn the radio off and you will **miss incoming messages** during sleep. This strategy works best for boards that only need to **send** (button press), not receive.

#### 2. Deep Sleep with External Wake-Up (Sender-Only Boards)

If a board only needs to send (e.g., a battery-powered button node), deep sleep is the most effective strategy. The ESP32 draws ~10 uA in deep sleep and wakes instantly on a GPIO transition.

```cpp
// In setup(), after sending the blink:
void enterDeepSleep() {
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW); // Wake on BOOT press
    esp_deep_sleep_start();
    // Board resets on wake — execution restarts from setup()
}
```

**Savings**: 10 uA idle consumption. A 1000 mAh battery lasts **years** if the button is pressed infrequently.

**Trade-off**: The board **cannot receive** ESP-NOW messages while in deep sleep. It only wakes to send. This creates an asymmetric design: sender nodes (deep sleep) and receiver nodes (always on, USB-powered).

#### 3. Reduce Wi-Fi TX Power

The default TX power is the maximum (~20 dBm / 100 mW). If your boards are close together (same room), reducing it saves power during transmission and reduces interference:

```cpp
// In setup(), after WiFi.mode(WIFI_STA):
esp_wifi_set_max_tx_power(8);  // ~2 dBm — enough for ~10 m indoors
```

The parameter is in units of 0.25 dBm (so 8 = 2 dBm, 40 = 10 dBm, 80 = 20 dBm).

**Savings**: TX current drops from ~260 mA to ~120 mA. Only matters during the brief transmission burst, but every bit counts on battery.

#### 4. Disable Serial Output

`Serial.print()` keeps the UART peripheral active and consumes CPU cycles:

```cpp
// Remove or guard debug prints behind a flag
#define DEBUG 0

#if DEBUG
    #define LOG(x) Serial.println(x)
#else
    #define LOG(x)
#endif
```

**Savings**: ~1-2 mA. Marginal, but free.

#### 5. Use an External Low-Power LED

The onboard LED on GPIO2 typically draws 5-10 mA. For battery-powered nodes, use a high-efficiency external LED with a larger resistor (e.g., 1 kOhm for ~2 mA). Or skip the LED entirely and use the serial output during development only.

#### 6. Lower CPU Frequency

The ESP32 defaults to 240 MHz. For this workload (polling a button and toggling a GPIO), 80 MHz is more than enough:

```cpp
// In setup(), very first line:
setCpuFrequencyMhz(80);
```

**Savings**: CPU current drops from ~30 mA to ~15 mA during active periods.

**Trade-off**: Wi-Fi throughput may decrease slightly, but ESP-NOW's 250-byte frames are unaffected.

#### 7. Combined Example

For a battery-powered receiver node that must stay awake to listen but optimizes everything else:

```cpp
void setup() {
    setCpuFrequencyMhz(80);
    // ... normal setup ...
    esp_wifi_set_max_tx_power(8);
}

void loop() {
    // ... button and blink logic (no Serial prints in production) ...

    // Light sleep between polls — radio off briefly
    esp_sleep_enable_timer_wakeup(20 * 1000); // 20 ms
    esp_light_sleep_start();
}
```

For a battery-powered sender-only node:

```cpp
void setup() {
    setCpuFrequencyMhz(80);
    // ... WiFi + ESP-NOW init ...
    esp_wifi_set_max_tx_power(8);

    // Send immediately on boot (woke from deep sleep = button was pressed)
    sendBlink();
    delay(50); // Give time for TX to complete

    // Go back to deep sleep
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
    esp_deep_sleep_start();
}

void loop() {
    // Never reached
}
```

### Battery Life Estimates

Rough estimates for a **1000 mAh LiPo** (module only, no dev board overhead):

| Strategy | Avg Current | Battery Life |
|---|---|---|
| Current firmware (always listening) | ~100 mA | ~10 hours |
| + CPU at 80 MHz | ~85 mA | ~12 hours |
| + Light sleep 20 ms polls | ~50 mA | ~20 hours |
| + Reduced TX power | ~45 mA | ~22 hours |
| Deep sleep sender-only | ~10 uA idle | ~11 years (theoretical) |

> Real-world battery life will be lower due to voltage regulator quiescent current, self-discharge, and temperature effects. Dev boards with USB bridge chips idle at ~30-50 mA regardless of ESP32 sleep state.

---

## Troubleshooting

| Problem | Solution |
|---|---|
| No serial ports detected | Install the correct USB driver (CP210x or CH340). Unplug and re-plug. |
| Build fails | Run `pio run -e esp32` and check the error. Usually a missing platform — PlatformIO downloads it automatically on first build. |
| Upload fails | Hold the **BOOT** button while pressing **EN/RST** to enter download mode. Some boards require this manually. |
| Boards don't communicate | Make sure all boards are on the same Wi-Fi channel (default: 1). Check serial output for `Ready.` message. Keep boards within range. |
| LED doesn't blink | Verify your board has an LED on GPIO2. Some boards use a different pin (e.g., GPIO5, GPIO19). Check the schematic. |
| Only some boards blink | Broadcast delivery is not guaranteed. Move boards closer together. Check for 2.4 GHz interference. |
| Button doesn't register | GPIO0 is the BOOT button on most boards but is sometimes overloaded. Try pressing and releasing quickly (not holding). |

---

## Going Further

Ideas for extending this project:

- **Add message types**: Use the `MsgType` enum to add commands beyond blink (e.g., set LED color, transmit sensor data).
- **Unicast mode**: Register specific peers by MAC address for acknowledged delivery and optional encryption.
- **Mesh relay**: Implement a simple store-and-forward layer so boards can relay messages beyond direct radio range.
- **OTA updates**: Combine ESP-NOW for control with Wi-Fi for firmware updates using ArduinoOTA.
- **Interrupt-driven button**: Replace polling with a GPIO interrupt for instant response even during blink sequences.
- **Message queue**: Replace the single boolean flag with a queue to handle rapid successive blink commands.

---

## License

This project is provided as-is for educational and experimental purposes. Use it however you like.
