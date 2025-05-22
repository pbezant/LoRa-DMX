# project.md

## Core Architecture
- Entry point: `src/main.cpp`
- LoRaWAN logic: `lib/HeltecLoRaWANWrapper/Heltec_LoRaWAN_Wrapper.h`
- LoRaWAN Credentials: `include/secrets.h` (used by the wrapper)
- DMX logic: `lib/DmxController/` (interfacing with `someweisguy/esp_dmx` library)
- Board & Core LoRa Peripheral Configuration: `platformio.ini` (via build flags like `-DHELTEC_BOARD=30`, `-DSLOW_CLK_TPYE=1`, `-DWIFI_LORA_32_V3`)

## Tech Stack
- ESP32 (Heltec WiFi LoRa 32 V3)
- PlatformIO
- Heltec LoRaWAN Library (via `heltecautomation/Heltec ESP32 Dev-Boards` PlatformIO library) for Class C support
- `Heltec_LoRaWAN_Wrapper` (custom wrapper for the Heltec LoRaWAN library)
- `someweisguy/esp_dmx` (currently trying to use `^4.1.0`, facing ESP-IDF version compatibility issues)
- ArduinoJson

## API Patterns
- `HeltecLoRaWANWrapper` exposes methods like `begin()`, `loop()`, `sendUplink()`, `onDownlink()`, `onConnectionStateChange()`.
- Event-based architecture for LoRaWAN events (join, downlink, connection status) managed through the wrapper, leveraging Heltec library's underlying callbacks.
- Downlink callback in `main.cpp` (`handleDownlink`) processes incoming LoRaWAN messages.
- DMX control via `DmxController` class, which uses the `esp_dmx` library.

## Database Schema
- N/A (no persistent database, only device-side storage via Preferences if needed) 