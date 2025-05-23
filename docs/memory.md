# Implementation Decisions, Edge Cases, Problems Solved, and Rejected Approaches

This document records key implementation decisions, how edge cases were handled, significant problems overcome, and approaches that were considered but ultimately rejected (along with the reasoning).

## Implementation Decisions

*   **Decision: Use Official Heltec LoRaWAN Library:** After evaluating different options, we chose to use the official Heltec LoRaWAN library for optimal compatibility with the Heltec LoRa 32 V3 hardware. This provides:
    - Native support for the SX126x radio module
    - Built-in state machine for LoRaWAN operations
    - Direct hardware abstraction without additional wrapper layers
    - Proven compatibility with the Heltec hardware platform

*   **Decision: State Machine Based Operation:** Following Heltec's recommended approach, implemented a state machine for device operation:
    - DEVICE_STATE_INIT: Initialize LoRaWAN stack
    - DEVICE_STATE_JOIN: Handle network join process
    - DEVICE_STATE_SEND: Transmit data
    - DEVICE_STATE_CYCLE: Manage transmission cycles
    - DEVICE_STATE_SLEEP: Handle power management

*   **Decision: Direct SX126x Integration:** Using Heltec's SX126x driver directly instead of a custom abstraction layer:
    - Eliminates potential compatibility issues
    - Provides access to all radio features
    - Maintains proper type safety through Heltec's enum definitions
    - Simplifies maintenance by following official examples

*   **Custom UART-based DMX Implementation:** After discovering ESP32S3 compatibility issues with existing DMX libraries (esp_dmx, LXESP32DMX), developed a custom UART-based DMX512 implementation that uses HardwareSerial and FreeRTOS tasks for precise timing control.
*   **Custom `DmxController` Wrapper:** Developed to provide a clean API for the custom DMX implementation, handling multi-channel fixtures, JSON parsing for fixture data, and DMX timing management via FreeRTOS tasks.
*   **`ArduinoJson` for Payload Parsing:** A standard and efficient library for handling JSON on microcontrollers, suitable for parsing commands.
*   **PlatformIO for Build System:** Chosen for its advanced features, library management, and board support, especially for ESP32 projects.

## Edge Cases Handled

*   **Join Failures:** Implemented comprehensive join failure handling:
    - Exponential backoff for retry attempts
    - Watchdog reset for persistent failures
    - Detailed error logging
    - Power management during retries

*   **Radio Configuration:** Properly handling radio state transitions:
    - Safe mode switching
    - Power level management
    - Channel mask configuration
    - Timing requirements

*   **Memory Constraints:** Managing limited resources:
    - Static memory allocation where possible
    - Efficient message queue implementation
    - Careful stack usage in callbacks
    - Proper string handling for JSON

## Problems Solved

*   **Remote DMX Control:** Achieved wireless control of DMX fixtures over a long-range, low-power network.
*   **Dynamic Fixture Configuration:** Using JSON allows changing DMX addresses and channel counts without reflashing firmware.
*   **Simplified LoRaWAN & DMX Integration:** Achieved through wrapper libraries, making the main application logic cleaner.
*   **ESP32S3 Compatibility Issues:** Resolved compilation problems with existing DMX libraries (LXESP32DMX, esp_dmx) that were incompatible with ESP32S3 by developing a custom UART-based DMX implementation.
*   **Missing LoRaWAN Global Variables:** Fixed linker errors by creating `src/lora_globals.cpp` to define all required global variables and structures expected by the Heltec LoRaWAN library.
*   **DMX Timing Precision:** Implemented FreeRTOS task-based timing control to ensure proper DMX512 break and mark-after-break timing requirements.
*   **Radio Driver Integration:** Resolved type conversion issues by adopting Heltec's type definitions:
    - RadioStandbyModes_t for standby mode configuration
    - RadioPacketTypes_t for packet type selection
    - RadioRampTimes_t for power ramp timing
    - Proper casting when calling SX126x functions
*   **LoRaWAN Join Process:** Implemented robust join handling:
    - Proper initialization sequence using Mcu.begin()
    - State machine based join process
    - Error handling with retry mechanism
    - Debug output for troubleshooting
*   **Memory Management:** Optimized memory usage:
    - Using static allocations where possible
    - Proper string handling for JSON processing
    - Efficient message queue implementation
    - Careful management of stack usage in callbacks

## Rejected Approaches

*   **Custom LoRaWAN Stack:** Initially considered building a custom LoRaWAN stack:
    - Rejected due to complexity and maintenance overhead
    - Official library provides better long-term support
    - Hardware-specific optimizations already included

*   **MCCI LMIC Library:** Evaluated using MCCI LMIC:
    - More complex setup required
    - Less direct hardware support
    - Additional abstraction layer unnecessary
    - Not optimized for Heltec hardware

*   **RadioLib Abstraction:** Considered using RadioLib:
    - Added unnecessary complexity
    - Limited benefit for single-radio project
    - Potential compatibility issues with Heltec hardware
    - Official library provides needed functionality

*   **(Potentially) Malformed JSON Payloads:** `ArduinoJson` provides error codes on parsing failure; the application logic should ideally handle these gracefully (e.g., ignore command, log error).
*   **(Potentially) DMX Universe Limits:** DMX512 supports 512 channels. The code needs to ensure commands don't try to address channels beyond this or overlap fixtures incorrectly (though `esp_dmx` might handle some of this).
*   **(Potentially) LoRaWAN Downlink Timing/Missed Messages:** LoRaWAN Class A devices only receive downlinks after an uplink. The application design must account for potential delays or missed commands.

*   **(Possibly) ABP for LoRaWAN:** OTAA was chosen, likely for better security and key management.
*   **(Possibly) Custom Binary Command Protocol:** JSON was chosen, likely for its flexibility and human-readability, despite a slightly larger payload size.
*   **(Possibly) Other LoRa Libraries:** `RadioLib` was selected, reasons could include feature set, community support, or prior experience.
*   **esp_dmx and LXESP32DMX Libraries:** Both libraries had compatibility issues with ESP32S3, including missing timer APIs and configuration constants. Custom UART-based implementation was chosen for better ESP32S3 compatibility and control.
*   **Bit-banging DMX:** Manual bit manipulation was considered but rejected in favor of UART-based implementation for better timing accuracy and reduced CPU overhead. 