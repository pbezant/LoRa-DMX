# LoRa-DMX Project Bookmark

## Current Status
- Project is a DMX LoRa Control System for Heltec LoRa 32 V3
- PlatformIO is set up and working
- **Previous attempt to use the official Heltec library is archived due to upstream issues.**
- **New direction:** Using [ropg/heltec_esp32_lora_v3](https://github.com/ropg/heltec_esp32_lora_v3) + [LoRaWAN_ESP32](https://github.com/ropg/LoRaWAN_ESP32) + [RadioLib](https://github.com/jgromes/RadioLib)
- **Region:** US915
- **Join Mode:** OTAA (credentials in `secrets.h`)
- **Focus:** LoRaWAN Class C operation, full DMX logic, JSON downlink format, periodic uplinks

---

## Implementation Plan (2024)

### 1. Documentation Updates
- Update all docs to reflect new stack, rationale, and implementation plan

### 2. PlatformIO Setup
- Remove Heltec official library from platformio.ini
- Add ropg/heltec_esp32_lora_v3, ropg/LoRaWAN_ESP32, jgromes/RadioLib to lib_deps
- Ensure ArduinoJson and esp_dmx remain

### 3. Credential Handling
- Use secrets.h for OTAA credentials (DEVEUI, APPEUI, APPKEY)
- Convert hex strings to byte arrays at runtime if needed

### 4. LoRaWAN Initialization
- Initialize LoRaWAN stack for US915 region
- Set up OTAA join using credentials
- Configure device for Class C operation

### 5. Downlink Handling
- Implement downlink callback for Class C (always listening)
- Parse downlink payloads as JSON (per README.md)
- Route commands to DMX logic

### 6. DMX Logic Integration
- Integrate full DMX output logic as described in README.md
- Support all JSON command formats, patterns, and error handling

### 7. Periodic Uplink
- Implement periodic status/heartbeat uplinks

### 8. Testing & Validation
- Test join, uplink, and downlink on Chirpstack/TTN
- Validate Class C operation (continuous receive)
- Test DMX output in response to downlink commands
- Confirm periodic uplinks are sent and received

### 9. Documentation & Code Cleanliness
- Keep all changes and rationale documented in docs/
- Reference secrets.h for credentials, do not hardcode in main code

---

## New Approach Summary
- **LoRaWAN credentials**: Still stored as hex strings in `secrets.h`, converted to byte arrays at runtime
- **LoRaWAN join/config**: Uses global variables (`devEui_lora`, `appEui_lora`, `appKey_lora`) as required by Heltec API
- **Class C operation**: Set via `deviceClass = CLASS_C;` in `setup()`
- **Downlink handling**: Uses global `downLinkDataHandleCb` function pointer
- **No LoRaWAN object**: All configuration is via globals and callbacks, not a class instance

---

## Current Status & Next Steps

*   **Build Status:**
    *   `LoRaWANHelper.cpp` and `LoRaWANHelper.h` have been refactored for RadioLib v7.1.2.
    *   The helper now compiles successfully after addressing include order, type definitions, and ensuring `RadioLib::SX1262*` is passed from `main.cpp`.
    *   Credential constants (`LORAWAN_DEVICE_EUI`, etc.) are correctly sourced from `secrets.h`.
    *   `LORAWAN_DOWNLINK_MAX_SIZE` is defined in `LoRaWANHelper.h`.
*   **Library Organization:**
    *   The `lib/` directory has been cleaned up. Helper files (`LoRaWANHelper`, `DMXHelper`, `LEDHelper`) are now located within their respective subdirectories. Duplicate and obsolete files/folders have been removed.
*   **Immediate Next Steps:**
    1.  **Thorough Testing (LORA-003):**
        *   Test OTAA join with the network server.
        *   Verify successful uplink transmission.
        *   Validate true Class C operation: continuous downlink reception.
        *   Test downlink message processing and DMX command execution.
        *   Confirm periodic uplinks (if implemented in `main.cpp` logic).
    2.  **DMX Logic Integration:** Ensure `dmx_helper_process_json_command` in `main.cpp` correctly interfaces with the `DMXHelper` library and controls DMX output as expected.
    3.  **LED Feedback:** Ensure `LEDHelper` provides correct status indications for network join, uplink/downlink activity, and errors.
    4.  **Documentation Review:** Update `docs/memory.md` and `docs/technical.md` with any new insights or final decisions made during the RadioLib v7 migration and Class C implementation.

---

## References
- See `lorawan_research.md` for library research and rationale
- See `docs/technical.md` for radio and board configuration
- See `docs/tasks.md` for actionable tasks 