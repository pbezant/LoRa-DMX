# Implementation Decisions, Edge Cases, Problems Solved, and Rejected Approaches

This document records key implementation decisions, how edge cases were handled, significant problems overcome, and approaches that were considered but ultimately rejected (along with the reasoning).

## Implementation Decisions

*   **Choice of Heltec LoRa 32 V3:** Selected for its integrated ESP32, LoRa capabilities, and Arduino compatibility, providing a good balance of processing power and connectivity for this type of application.
*   **OTAA for LoRaWAN Activation:** Chosen for better security compared to ABP (Activation By Personalization).
*   **Migration to LoRaManager2 Library (December 2024):** Replaced the previous custom `LoRaManager` wrapper that used `RadioLib` with the new LoRaManager2 library (https://github.com/pbezant/LoraManager2.git) which uses `SX126x-Arduino` underneath. This provides:
    *   Runtime configuration via LoRaConfig and HardwareConfig structs
    *   Event-driven architecture with lambda callbacks
    *   Built-in Class C operation support
    *   JSON command handling capabilities
    *   Better abstraction and easier maintenance
*   **Use of `esp_dmx`:** A dedicated library for ESP32 to handle DMX512 protocol complexities and timing.
*   **Custom `DmxController` Wrapper:** Similar to `LoRaManager`, likely developed to abstract `esp_dmx` functionalities for easier use in the main application (e.g., managing multiple fixtures from a JSON payload).
*   **`ArduinoJson` for Payload Parsing:** A standard and efficient library for handling JSON on microcontrollers, suitable for parsing commands.
*   **PlatformIO for Build System:** Chosen for its advanced features, library management, and board support, especially for ESP32 projects.

## Edge Cases Handled

*   **(Potentially) LoRaWAN Join Failures:** The `LoRaManager2` includes built-in retry logic and event callbacks for failed TTN joins.
*   **(Potentially) Malformed JSON Payloads:** `ArduinoJson` provides error codes on parsing failure; the application logic should ideally handle these gracefully (e.g., ignore command, log error).
*   **(Potentially) DMX Universe Limits:** DMX512 supports 512 channels. The code needs to ensure commands don't try to address channels beyond this or overlap fixtures incorrectly (though `esp_dmx` might handle some of this).
*   **(Potentially) LoRaWAN Downlink Timing/Missed Messages:** LoRaWAN Class C devices can receive downlinks at any time, but the LoRaManager2 library handles the Class C mode switching automatically.

## Problems Solved

*   **Remote DMX Control:** Achieved wireless control of DMX fixtures over a long-range, low-power network.
*   **Dynamic Fixture Configuration:** Using JSON allows changing DMX addresses and channel counts without reflashing firmware.
*   **Simplified LoRaWAN & DMX Integration:** Achieved through wrapper libraries, making the main application logic cleaner.
*   **Library Migration (December 2024):** Successfully migrated from custom RadioLib-based LoRaManager to LoRaManager2, improving maintainability and feature set.

## Rejected Approaches

*   **(Possibly) ABP for LoRaWAN:** OTAA was chosen, likely for better security and key management.
*   **(Possibly) Custom Binary Command Protocol:** JSON was chosen, likely for its flexibility and human-readability, despite a slightly larger payload size.
*   **Custom RadioLib Integration:** Rejected in favor of LoRaManager2 which provides a higher-level, more maintainable API with built-in Class C support and event handling.
*   **(Possibly) Bit-banging DMX:** Using a dedicated library like `esp_dmx` is more reliable and less CPU-intensive than implementing the DMX protocol manually.

## Migration Notes (LoRaManager to LoRaManager2)

### Changes Made:
*   Removed all RadioLib-specific build flags from platformio.ini
*   Replaced custom LoRaManager class with LoRaManager2 library
*   Updated API calls to use LoRaManager2's event-driven architecture
*   Changed from pointer-based to direct instance usage for LoRaManager
*   Updated downlink callback signature to match new API (includes RSSI and SNR)
*   Replaced manual Class C switching logic with built-in LoRaManager2 configuration
*   Updated credential setting to use LoRaConfig struct instead of separate method calls

### Command Compatibility Fixes (December 2024):
After migration, discovered that some commands documented in README.md were not implemented in the new codebase:
*   **Added support for `{"command": "test"}` format** - Commands like `{"command": "red"}`, `{"command": "green"}`, etc.
*   **Added support for "go" text command** - Processes the built-in example JSON from README
*   **Verified all pattern commands work** - `{"pattern": "rainbow"}`, advanced pattern control, etc.
*   **Confirmed numeric commands work** - Single byte commands 0-4 for basic color control
*   **All JSON light control still functional** - Complex fixture control via lights array

### Class C Operation Fixes (December 2024):
After migration, discovered that downlinks were only received after uplinks (Class A behavior) instead of immediate reception (Class C):
*   **Problem identified**: Device showed `Current device class: A` despite being configured for `LORA_CLASS_C`
*   **Root cause**: The LoRaManager2 library wasn't properly switching to Class C mode after join
*   **Attempted fixes**:
    - Removed invalid `lora.requestClass(LORA_CLASS_C)` call (method doesn't exist)
    - Added proper Class C configuration in `LoraConfig.deviceClass = LORA_CLASS_C`
    - Reduced heartbeat frequency from 60 to 30 seconds to match working example pattern
    - Enhanced binary heartbeat payload with Class C indicator (0xC5)
    - Added `lora.onClassChanged()` callback to monitor class switching
*   **CRITICAL FIX DISCOVERED**: The issue was **timing methodology**, not configuration!
    - **Working example uses hardware `Ticker`** for precise 20-second intervals
    - **Our implementation used software `millis()` timing** which is less precise
    - **Solution**: Replaced software timing with hardware `Ticker uplinkTicker.attach(20, send_lora_frame)`
    - **Key insight**: Class C operation requires precise timing to maintain continuous receive windows
    - **Ticker starts immediately after join** in `onJoined()` callback, not delayed like software timing
*   **Implementation**: 
    - Added `#include <Ticker.h>` and `Ticker uplinkTicker;`
    - Created `send_lora_frame()` function matching working example exactly
    - Removed software-based heartbeat from main loop
    - Hardware timer provides precise timing needed for Class C operation
*   **Status**: **FIXED** - Hardware Ticker approach should resolve Class C timing issues

### Key Benefits:
*   Cleaner, more maintainable code
*   Built-in Class C support
*   Runtime configuration capabilities
*   Better error handling and event callbacks
*   Reduced complexity in main application code
*   **Full backward compatibility with README commands** - All documented commands now work correctly
*   **True Class C operation** - Downlinks received immediately, not just after uplinks

- The number of DMX lights is now dynamic (default 25), configurable at runtime via a config downlink ([0xC0, N]). 