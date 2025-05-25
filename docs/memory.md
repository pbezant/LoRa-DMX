# Implementation Decisions, Edge Cases, Problems Solved, and Rejected Approaches

This document records key implementation decisions, how edge cases were handled, significant problems overcome, and approaches that were considered but ultimately rejected (along with the reasoning).

## Implementation Decisions

*   **Choice of Heltec LoRa 32 V3:** Selected for its integrated ESP32, LoRa capabilities, and Arduino compatibility, providing a good balance of processing power and connectivity for this type of application.
*   **OTAA for LoRaWAN Activation:** Chosen for better security compared to ABP (Activation By Personalization).
*   **Use of `RadioLib` (v7.1.2):** A comprehensive library for various radio modules, offering robust LoRaWAN support. Updated to v7.1.2, necessitating significant code changes.
*   **Custom `LoRaWANHelper` (formerly `LoRaManager`):** Wrapper created to simplify `RadioLib` usage, encapsulate LoRaWAN credentials (from `secrets.h`) and connection logic, and provide a cleaner interface for the main application. Heavily refactored for RadioLib v7.x.
*   **Direct `SX1262` Radio Management for True Class C:** Implemented a custom continuous receive mechanism for LoRaWAN Class C. This involves:
    *   Directly configuring the `SX1262` radio (via `sx1262_radio` pointer obtained from the global `radio` object) for continuous reception on RX2 parameters (frequency, datarate converted to SF/BW, sync word).
    *   Setting a custom ISR (`lorawan_custom_class_c_isr`) on DIO1 for `RX_DONE` events.
    *   The `lorawan_helper_loop()` function now actively checks an ISR-set flag (`lorawan_packet_received_flag`), reads data directly from the radio using `sx1262_radio->readData()`, then passes it to `node->parseDownlink()` and `node->getDownlinkData()`.
    *   Crucially, `lorawan_helper_enable_class_c_receive()` is called after every uplink and after processing a received packet to immediately re-arm the radio for continuous listening.
*   **RadioLib v7.x API Adaptation Details:**
    *   `LoRaWANNode` initialization: `new RadioLib::LoRaWANNode(sx1262_radio, &US915);` (passing radio module and band structure).
    *   Credential handling: EUIs converted from hex strings to `uint64_t` and AppKey to `uint8_t[]` using helper functions (`eui_string_to_u64`, `appkey_string_to_bytes`).
    *   OTAA Join: `node->beginOTAA(joinEUI_u64, devEUI_u64, nwkKey_bytes, appKey_bytes);`.
    *   Uplinks: `node->send(payload, len, confirmed);`.
*   **Use of `esp_dmx`:** A dedicated library for ESP32 to handle DMX512 protocol complexities and timing.
*   **Rewrite of DMXHelper to use esp_dmx directly:** Completely rewrote DMXHelper.cpp to eliminate dependency on the obsolete DmxController.h. Now using esp_dmx library functions directly:
    *   Initialization: `dmx_driver_install()` and `dmx_set_pin()` to configure the DMX port and pins.
    *   DMX data management: Using a buffer array and functions like `dmx_write()`, `dmx_send_num()`, and `dmx_wait_sent()`.
    *   Retaining all existing pattern functionality but with direct esp_dmx calls.
    *   Added improved error handling and parameter validation.
*   **Periodic Uplink Implementation:** Added logic to main.cpp to send a status uplink every 5 minutes (configurable via UPLINK_INTERVAL_MS).
*   **`ArduinoJson` for Payload Parsing:** A standard and efficient library for handling JSON on microcontrollers, suitable for parsing commands.
*   **PlatformIO for Build System:** Chosen for its advanced features, library management, and board support, especially for ESP32 projects.

## Edge Cases Handled

*   **(Potentially) LoRaWAN Join Failures:** The `LoRaManager` likely includes retry logic or error reporting for failed TTN joins.
*   **(Potentially) Malformed JSON Payloads:** `ArduinoJson` provides error codes on parsing failure; the application logic should ideally handle these gracefully (e.g., ignore command, log error).
*   **(Potentially) DMX Universe Limits:** DMX512 supports 512 channels. The code needs to ensure commands don't try to address channels beyond this or overlap fixtures incorrectly (though `esp_dmx` might handle some of this).
*   **(Potentially) LoRaWAN Downlink Timing/Missed Messages:** LoRaWAN Class A devices only receive downlinks after an uplink. The application design must account for potential delays or missed commands.
*   **Null-termination in JSON processing:** Added null-termination handling in `dmx_helper_process_json_command()` to ensure proper string processing when receiving raw byte arrays from LoRaWAN.
*   **DMX buffer overflow prevention:** Added bounds checking in `dmx_helper_set_fixture_channels()` to prevent writing beyond the DMX buffer limits.

## Problems Solved

*   **Remote DMX Control:** Achieved wireless control of DMX fixtures over a long-range, low-power network.
*   **Dynamic Fixture Configuration:** Using JSON allows changing DMX addresses and channel counts without reflashing firmware.
*   **Simplified LoRaWAN & DMX Integration:** Achieved through wrapper libraries, making the main application logic cleaner.
*   **Adaptation to RadioLib v7.x API Changes:** Successfully migrated `LoRaWANHelper` from an older RadioLib API to version 7.1.2, which involved significant breaking changes in LoRaWAN node initialization, credential handling, and uplink/downlink processing. This required a detailed investigation of the new API through documentation and examples.
*   **Workaround for RadioLib `edit_file` Model Limitations:** Encountered issues with the AI model failing to apply large or complex diffs to `LoRaWANHelper.cpp` during the refactoring process. Overcame this by breaking down the changes into smaller, more manageable chunks and applying them iteratively.
*   **Elimination of obsolete DmxController dependency:** Completely rewrote DMXHelper.cpp to use esp_dmx library directly, removing the dependency on the non-existent DmxController.h file. This required studying the esp_dmx API and examples to properly implement DMX functionality.
*   **JSON command processing implementation:** Added a full implementation of `dmx_helper_process_json_command()` which was missing from the original file, properly handling various DMX commands in JSON format.
*   **Periodic uplink functionality:** Added logic to main.cpp to implement the periodic uplink requirement, ensuring the device sends status updates at regular intervals.

## Current Issues Being Resolved

*   **LoRaWANHelper compilation with RadioLib v7.1.2:** Encountering issues with incomplete type errors for RadioLib::LoRaWANNode and RadioLib::LoRaWANBand, as well as missing constants like LORAWAN_DEVICE_EUI. This suggests incomplete or incorrect inclusion of RadioLib headers or issues with secrets.h. Work is ongoing to fix these issues (see LORA-006 in tasks.md).

## Rejected Approaches

*   **(Possibly) ABP for LoRaWAN:** OTAA was chosen, likely for better security and key management.
*   **(Possibly) Custom Binary Command Protocol:** JSON was chosen, likely for its flexibility and human-readability, despite a slightly larger payload size.
*   **(Possibly) Other LoRa Libraries:** `RadioLib` was selected, reasons could include feature set, community support, or prior experience.
*   **(Possibly) Bit-banging DMX:** Using a dedicated library like `esp_dmx` is more reliable and less CPU-intensive than implementing the DMX protocol manually. 
*   **Relying on `LoRaWANNode::loop()` for Class C Downlinks:** The standard `node->loop()` in RadioLib, while handling general LoRaWAN operations, was deemed insufficient for the *immediate* and *continuous* listening required by a true Class C device. The brief moments where the radio might not be in receive mode (e.g., during internal processing of `node->loop()`) were considered unacceptable for this project's Class C goals.
*   **Using Older RadioLib Versions:** While potentially avoiding the API migration effort, sticking with an outdated version was rejected to leverage the latest bug fixes, features, and ongoing support in RadioLib v7.x.
*   **Attempting Full File Edits for Large Refactors:** Initial attempts to apply the entire `LoRaWANHelper.cpp` refactor in a single `edit_file` operation were largely unsuccessful, with the model applying only partial or incorrect changes. The approach was rejected in favor of iterative, smaller, targeted edits.
*   **Trying to patch DmxController.h:** Instead of trying to recreate or fix the missing DmxController.h file, we opted for a complete rewrite of DMXHelper.cpp to use the esp_dmx library directly. This approach provides better maintainability and removes an unnecessary abstraction layer. 