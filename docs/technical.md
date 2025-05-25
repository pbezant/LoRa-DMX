# Technical Specifications and Patterns

## LoRaWAN Stack (2024)
- **Library:** [ropg/heltec_esp32_lora_v3](https://github.com/ropg/heltec_esp32_lora_v3)
- **LoRaWAN Layer:** [LoRaWAN_ESP32](https://github.com/ropg/LoRaWAN_ESP32)
- **Radio Layer:** [RadioLib](https://github.com/jgromes/RadioLib) v7.1.2
- **Region:** US915
- **Join Mode:** OTAA (credentials in `secrets.h`)
- **Class:** C (continuous receive)
- **Downlink Format:** JSON (see README.md)
- **Uplink:** Periodic status/heartbeat
- **DMX Output:** Full DMX logic, including patterns and direct channel control

## Rationale for Library Choice
- The official Heltec library is deprecated for this project due to upstream build issues and unreliable Class C support for V3 boards.
- The new stack is actively maintained, modern, and easier to debug/extend.
- RadioLib provides robust LoRaWAN Class C support and flexibility.

## Key Technical Decisions
- **Credentials:** Managed in `secrets.h` as hex strings, converted to byte arrays at runtime.
- **LoRaWAN Join:** OTAA only, using US915 region.
- **Downlink Handling (True Class C):** All downlinks are expected as JSON. True asynchronous Class C is achieved by:
    - Directly managing the `SX1262` radio for continuous receive on RX2 parameters.
    - Using a custom ISR on DIO1 for `RX_DONE` to flag incoming packets.
    - `LoRaWANHelper::loop()` processes these flags, reads directly from the radio, then uses `RadioLib::LoRaWANNode::parseDownlink()` and `getDownlinkData()`.
    - The radio is immediately re-armed for continuous receive after an uplink and after processing a received packet.
- **RadioLib v7.x API Migration:** The `LoRaWANHelper.cpp` has been significantly refactored to align with the breaking changes in RadioLib v7.1.2, particularly for node initialization, credential passing (OTAA), and uplink mechanisms.
- **DMX Implementation:** Complete rewrite of DMXHelper.cpp to use esp_dmx library directly:
    - Configured with appropriate pins for the Heltec hardware (TX: GPIO17, RX: GPIO16, ENABLE: GPIO21).
    - Uses a buffer-based approach with DMX_PACKET_SIZE array.
    - DMX functions like dmx_driver_install(), dmx_set_pin(), dmx_write(), and dmx_send_num() for direct control.
    - Handles DMX patterns (color fade, rainbow, strobe, etc.) with the appropriate timing and buffer management.
    - Robust JSON command parsing with ArduinoJson for dynamic control.
- **Periodic Uplinks:** Added to main.cpp with configurable interval (default: 5 minutes).
- **Uplink Format:** Simple JSON status message: `{"status":"alive"}`, extensible for future needs.
- **DMX Logic:** Follows the structure and command formats in README.md, including support for patterns and direct channel control.
- **Error Handling:** Robust error handling for join, downlink, and DMX operations.

## Current Technical Challenges
- **LoRaWANHelper Compilation Issues:** The LoRaWANHelper.cpp file is experiencing compilation errors with RadioLib v7.1.2:
    - Incomplete type errors for RadioLib::LoRaWANNode and RadioLib::LoRaWANBand.
    - Missing LORAWAN_DEVICE_EUI and related constants from secrets.h.
    - Issues with forward declarations and include paths.
    - Work in progress to resolve these issues (see LORA-006 in tasks.md).

## Patterns
- **Separation of Concerns:** LoRaWAN, DMX, and JSON logic are modular and decoupled.
- **Runtime Credential Conversion:** No hardcoded byte arrays; all credentials are converted at runtime from hex strings.
- **Documentation:** All changes and rationale are documented in the docs/ folder.
- **Direct Hardware Library Usage:** For DMX, moved from a wrapper approach to direct esp_dmx library usage for better maintainability.

## Deprecated/Abandoned Approaches
- The official Heltec ESP32 Dev-Boards library is no longer used due to upstream issues.
- DmxController.h wrapper has been removed in favor of direct esp_dmx library usage.

## Next Steps
- Resolve LoRaWANHelper compilation issues with RadioLib v7.1.2
- Complete the testing phase
- Keep documentation up to date

# Technical Specifications and Established Patterns

This document details technical specifications for various aspects of the project and outlines established coding patterns and practices.

## Coding Standards

*   **Language Version(s):** C++ (Arduino flavor, typically based on a recent C++ standard like C++11 or C++17 depending on ESP-IDF version used by PlatformIO for ESP32)
*   **Linting & Formatting:** (Not explicitly defined in `platformio.ini`, but PlatformIO can integrate with tools like ClangFormat. Assume default or user-configured.)
*   **Naming Conventions:** (Likely Arduino/C++ conventions, e.g., camelCase for functions/variables, PascalCase for classes/structs, UPPER_CASE for constants/macros. To be confirmed by inspecting source code.)

## Established Patterns

### Pattern A: Wrapper Libraries for Hardware Abstraction

*   **Description:** Custom libraries (`LoRaWANHelper.cpp`, `DMXHelper.cpp`) are used to simplify interaction with underlying hardware/communication libraries (`RadioLib`, `esp_dmx`). This provides a cleaner API for the main application logic and encapsulates hardware-specific details. `LoRaWANHelper.cpp` has been extensively refactored for RadioLib v7.x, while `DMXHelper.cpp` now uses the esp_dmx library directly instead of through a wrapper.
*   **When to Use:** When interacting with complex external libraries or hardware peripherals to provide a simplified and project-specific interface.
*   **Example (or link to example code):**
    ```cpp
    // Conceptual example based on README description
    // LoRaManager lora;
    // lora.joinNetwork(joinEUI, devEUI, appKey);
    // if (lora.messageReceived()) { /* process message */ }

    // Using DMXHelper
    dmx_helper_init();
    dmx_helper_set_fixture_channels(1, channelValues, 4);
    dmx_helper_update();
    ```

### Pattern B: JSON for Dynamic Configuration/Commands

*   **Description:** JSON payloads received over LoRaWAN are parsed to dynamically control DMX fixtures. This avoids hardcoding fixture settings and allows flexible remote configuration.
*   **When to Use:** For sending structured data or commands to the device, especially when the parameters can vary.
*   **Example (or link to example code):** 
    ```json
    // Example JSON command to set a fixture
    {"cmd":"set","addr":1,"values":[255,0,0,0]}
    
    // Example JSON command to start a pattern
    {"cmd":"rainbow","speed":50,"cycles":0}
    ```

### Pattern C: Direct Radio Management for Custom True Class C Behavior

*   **Description:** To achieve immediate and continuous LoRaWAN Class C reception, the `SX1262` radio is managed directly by `LoRaWANHelper.cpp` rather than solely relying on `RadioLib::LoRaWANNode::loop()` for downlink opportunities. This involves:
    *   Obtaining RX2 parameters (frequency, data rate) from the `LoRaWANNode`.
    *   Converting LoRaWAN data rates to radio-specific Spreading Factor (SF) and Bandwidth (BW).
    *   Configuring the `SX1262` with these parameters, plus sync word and coding rate.
    *   Setting up a hardware interrupt (ISR) on the radio's DIO1 pin for the `RX_DONE` event.
    *   In the main loop, checking a flag set by this ISR, then reading the packet directly from the radio using `sx1262_radio->readData()`.
    *   The raw packet is then passed to `node->parseDownlink()` for the LoRaWAN stack to process.
    *   The radio is explicitly re-enabled for continuous reception after every uplink and after processing any received downlink.
*   **When to Use:** When precise control over the radio's receive state is needed to implement features like true Class C that require minimizing any potential non-listening periods, beyond what high-level library functions might guarantee.
*   **Example (or link to example code):** See `lorawan_helper_enable_class_c_receive()` and the Class C handling within `lorawan_helper_loop()` in `lib/LoRaWANHelper/LoRaWANHelper.cpp`.

### Pattern D: Direct Hardware Library Usage (esp_dmx)

*   **Description:** For DMX control, the project now uses the esp_dmx library directly instead of through a wrapper, providing more direct access to the hardware capabilities while maintaining a clean API for the application.
*   **When to Use:** When the underlying library is well-designed and the abstraction layer adds unnecessary complexity.
*   **Example (or link to example code):** See the implementation in `lib/DMXHelper/DMXHelper.cpp`.

## API Specifications

*   **Primary Interface:** LoRaWAN Downlink Messages via The Things Network (TTN)
*   **Payload Format:** JSON (see `README.md` for structure). Also supports simplified text/numeric commands via TTN payload formatter.
*   **Authentication:** OTAA (Over-The-Air Activation) for LoRaWAN network join. AppKey provides device authentication.
*   **TTN Payload Formatter:** `ttn_payload_formatter.js` (JavaScript) used in TTN console to decode uplink and encode downlink messages.

## Security Considerations

*   **LoRaWAN Security:** Relies on standard LoRaWAN security mechanisms (AES-128 encryption for network and application sessions).
*   **Credential Management:** `joinEUI`, `devEUI`, and `appKey` are critical for network access. `README.md` indicates these are in `main.cpp` and should be user-configured.
*   **Input Sanitization:** (Assumed to be handled by `ArduinoJson` during parsing, but complex or malformed JSON could still be a concern. Further inspection of parsing logic in source code needed.)

## Performance Guidelines

*   **Real-time DMX:** DMX requires consistent timing. `esp_dmx` library likely handles this at a low level.
*   **LoRaWAN Duty Cycle:** Adherence to LoRaWAN regional duty cycle limitations is critical (managed by `RadioLib` and TTN).
*   **JSON Parsing:** `ArduinoJson` is generally efficient, but very large/complex JSON payloads could impact performance or memory on the microcontroller. Payload size is also limited by LoRaWAN constraints.
*   **Power Consumption:** For battery-powered applications (if any), deep sleep and minimizing radio transmission time are important. (Heltec LoRa 32 V3 has Wi-Fi/Bluetooth which should be disabled if not used to save power). 