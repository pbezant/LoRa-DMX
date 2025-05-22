# memory.md

## Implementation Decisions
- **Initial LoRaWAN Approach:** Started with MCCI LMIC library (via a `LoRaWANManager` class) aiming for Class C support.
- **Current LoRaWAN Approach:** Migrated to Heltec's official LoRaWAN library (`Heltec ESP32 Dev-Boards`) using a custom `HeltecLoRaWANWrapper` for Class C operation. This was chosen due to insurmountable build and runtime issues with MCCI LMIC on the Heltec WiFi LoRa 32 V3 (SX1262).
- LoRaWAN credentials sourced from `include/secrets.h`.
- Board-specific LoRa (radio type, clock source) and pin configurations are managed via build flags in `platformio.ini` to correctly initialize the Heltec library.
- DMX functionality implemented via `DmxController` class, which wraps the `someweisguy/esp_dmx` library.

## Edge Cases Handled
- LoRaWAN Class C continuous reception setup via Heltec library.
- Event-based LoRaWAN operations (join, downlink, connection state) through the wrapper.
- Always-on operation (no deep sleep considered yet).

## Problems Solved (and Attempted)
- **Attempted:** Resolve MCCI LMIC build/runtime errors for SX1262 and Class C. Ultimately abandoned.
- **Solved (LoRaWAN):** Established a LoRaWAN Class C foundation using the Heltec library and custom wrapper. Resolved Heltec library initialization issues by identifying correct build flags for board type and clock source.
- **Ongoing (DMX):** Actively troubleshooting `esp_dmx` library compilation errors, which are due to an ESP-IDF version mismatch (library expects v5.x, project uses v4.4.x via Arduino core v2.x). Attempts to use older versions of `esp_dmx` are in progress.
- Decoupled hardware-specific LoRaWAN details from `main.cpp` using the wrapper.

## Approaches Rejected
- **MCCI LMIC:** Abandoned due to persistent build/runtime errors related to SX1262 support and internal library structure (e.g., `radio.c` issues) on the Heltec V3 board, despite extensive configuration attempts (`lmic_project_config.h`, `platformio.ini` build flags).
- **RadioLib:** Briefly considered but set aside as the focus was on MCCI LMIC and then Heltec's native library for LoRaWAN.
- No runtime class switching for LoRaWAN (compile/startup only).
- No provisioning dialogs or serial entry for LoRaWAN keys (security, simplicity). 