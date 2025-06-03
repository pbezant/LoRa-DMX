# Current Development Tasks and Requirements

This document tracks current development tasks, their status, and associated requirements. It should be updated regularly to reflect project progress.

## Sprint/Iteration: LoRaWAN Class C Migration (2024)

### Archived

*   **Task ID:** LORA-001
    *   **Description:** Migrate to official Heltec ESP32 Dev-Boards library for LoRaWAN Class C operation.
    *   **Status:** Archived (abandoned due to upstream issues)

### Completed (Superceded by Custom Implementation within LORA-002)

*   **Task ID:** LORA-002
    *   **Description:** Migrate to `ropg/heltec_esp32_lora_v3` + `LoRaWAN_ESP32` + `RadioLib` (v7.1.2) and implement true LoRaWAN Class C operation.
    *   **Status:** Completed
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria Met:**
        *   PlatformIO configured with `RadioLib` v7.1.2 and necessary dependencies.
        *   US915 region, OTAA join using credentials from `secrets.h` (converted from hex strings to `uint64_t` and `uint8_t[]` at runtime).
        *   Device operates in true Class C mode through direct `SX1262` radio management for continuous receive on RX2 parameters, including custom ISR for `RX_DONE`.
        *   `LoRaWANHelper.cpp` and `LoRaWANHelper.h` extensively refactored to use `RadioLib` v7.1.2 API (e.g., `LoRaWANNode` init, `beginOTAA`, `send`).
        *   Downlink callback mechanism retained; `lorawan_helper_loop()` handles packet reception, parsing via `node->parseDownlink()`, and then calls user callback.
        *   Radio is re-armed for continuous Class C receive after every uplink and after processing each received packet.
        *   (Full DMX logic integration and periodic uplinks are part of the main application logic, assumed to leverage this helper.)
        *   All significant changes and decisions documented in `docs/memory.md` and `docs/technical.md`.
    *   **PRD Link:** N/A
    *   **Depends On:** None

*   **Task ID:** LORA-004
    *   **Description:** Clean up `lib/` directory: organize helper files into subdirectories and remove obsolete/duplicate files.
    *   **Status:** Completed
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria Met:**
        *   `LoRaWANHelper`, `DMXHelper`, and `LEDHelper` files are located in their respective subdirectories within `lib/`.
        *   Loose helper files in the root of `lib/` have been deleted.
        *   Obsolete directories (`lib/DmxController/`, `lib/LoRaManager.bak2/`) have been deleted.
        *   The `lib/README` file was removed (pending confirmation if it needs to be restored with specific content).
    *   **PRD Link:** N/A
    *   **Depends On:** None

*   **Task ID:** LORA-005
    *   **Description:** Replace DmxController dependency with direct esp_dmx library usage
    *   **Status:** Completed
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria Met:**
        *   Removed dependency on the obsolete DmxController.h.
        *   Rewrote DMXHelper.cpp to use esp_dmx library directly.
        *   Implemented all required DMX functionality (fixture control, patterns, JSON command processing).
        *   Added detailed error handling and parameter validation.
    *   **PRD Link:** N/A
    *   **Depends On:** None

### In Progress

*   **Task ID:** LORA-006
    *   **Description:** Fix LoRaWANHelper include issues with RadioLib v7.1.2
    *   **Status:** On Hold (superseded by LORA-007)
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria:**
        *   Fix incomplete type errors with RadioLib::LoRaWANNode and RadioLib::LoRaWANBand.
        *   Ensure proper inclusion of RadioLib headers.
        *   Fix missing LORAWAN_DEVICE_EUI and related constants from secrets.h.
        *   Ensure LoRaWANHelper compiles successfully with RadioLib v7.1.2.
    *   **PRD Link:** N/A
    *   **Depends On:** None

*   **Task ID:** LORA-007
    *   **Description:** Implement true Class C LoRaWAN using nopnop2002's SX1262 driver with RadioLib's LoRaWAN stack
    *   **Status:** Completed
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria:**
        *   ✅ Fork or clone nopnop2002/Arduino-LoRa-Ra01S for SX1262 driver
        *   ✅ Integrate with RadioLib's LoRaWAN stack for protocol handling
        *   ✅ Implement interrupt-driven Class C downlink reception using DIO1 pin
        *   ✅ Create a modular integration layer that separates hardware and protocol concerns
        *   ✅ Add bandwidth detection and fallback for hardware compatibility
        *   ✅ Maintain the existing LoRaWANHelper API for backward compatibility
        *   ✅ Provide example code in examples/hybrid_class_c
    *   **Notes:**
        *   The integration was implemented using three layers:
            *   **SX1262Radio** - A wrapper around nopnop2002's SX1262 driver
            *   **LoRaWANAdapter** - An adapter between SX1262Radio and RadioLib's LoRaWAN stack
            *   **LoRaWANHelper** - The public API for the main application
        *   Bandwidth compatibility detection was implemented based on the findings from LORA-008
        *   Working example is available in examples/hybrid_class_c

### Completed

*   **Task ID:** LORA-008
    *   **Description:** Create bandwidth testing tool for SX1262 radio
    *   **Status:** Completed
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria Met:**
        *   Created a comprehensive bandwidth testing tool in `examples/bandwidth_test/`
        *   Successfully tested all possible SX1262 bandwidths (7.8-500 kHz)
        *   Identified supported bandwidths: 7.8, 15.6, 31.25, 62.5, 125.0, 250.0, and 500.0 kHz
        *   Identified unsupported bandwidths: 10.4, 20.8, and 41.7 kHz
        *   Confirmed 125.0 kHz (standard for US915) is fully supported
        *   Created `docs/bandwidth_test_results.md` with detailed findings
        *   Updated related documentation (`memory.md`, `technical.md`) with results
    *   **PRD Link:** N/A
    *   **Depends On:** None

### To Do

*   **Task ID:** LORA-003
    *   **Description:** Testing and validation of LoRaWAN Class C + DMX system
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria:**
        *   Successful join, uplink, and downlink on Chirpstack/TTN
        *   Class C operation validated (continuous receive)
        *   DMX output responds to downlink commands
        *   Periodic uplinks received by network
    *   **PRD Link:** N/A
    *   **Depends On:** LORA-007

*   **Task ID:** LORA-009
    *   **Description:** Refactor existing LoRaWANHelper to use the new implementation
    *   **Status:** To Do
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria:**
        *   Update LoRaWANHelper to use the new nopnop2002 driver + RadioLib approach
        *   Ensure API compatibility with the rest of the application
        *   Maintain all existing functionality (join, send, receive, callbacks)
        *   Ensure proper Class C operation
        *   Document all changes
    *   **PRD Link:** N/A
    *   **Depends On:** LORA-007, LORA-008

*   **Task ID:** LORA-010
    *   **Description:** Create a comprehensive example of the hybrid Class C implementation
    *   **Status:** To Do
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria:**
        *   Create a standalone example in examples/ directory
        *   Include detailed comments explaining the implementation
        *   Document hardware setup and pin connections
        *   Provide a clear demonstration of Class C downlink handling
        *   Include simple uplink functionality for testing
    *   **PRD Link:** N/A
    *   **Depends On:** LORA-007

*   **Task ID:** LORA-011
    *   **Description:** Create an integration guide for the hybrid approach
    *   **Status:** To Do
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria:**
        *   Document all steps required to integrate the hybrid approach in a new project
        *   Include diagrams of the architecture
        *   Provide code snippets for key functionalities
        *   List known limitations and workarounds
        *   Include troubleshooting tips
    *   **PRD Link:** N/A
    *   **Depends On:** LORA-007, LORA-010

## Blockers

*   None - We're moving forward with the hybrid approach (LORA-007)

## Updated Implementation Plan for LORA-007

1. **Research and Analysis (Completed)**
   * Analyzed both the pure RadioLib approach and hybrid approach with nopnop2002's driver
   * Determined that the hybrid approach is more likely to succeed with our hardware
   * Documented findings in memory.md and radiolib_class_c_deep_dive.md

2. **Setup Phase (Current)**
   * Fork/clone nopnop2002/Arduino-LoRa-Ra01S repository
   * Install as a library in our PlatformIO project
   * Create a simple sketch to test basic radio functionality
   * Verify SPI communication with the SX1262 chip

3. **Bandwidth Testing Phase (Completed)**
   * Implemented the bandwidth testing tool (LORA-008)
   * Tested all possible bandwidths supported by the SX1262 chip
   * Documented which bandwidths work reliably with our hardware in `bandwidth_test_results.md`
   * Identified the key supported bandwidths for our implementation (7.8, 15.6, 31.25, 62.5, 125.0, 250.0, 500.0 kHz)
   * Confirmed 125.0 kHz (critical for standard US915 operation) is fully supported

4. **Integration Phase (Current)**
   * Create a wrapper class that bridges nopnop2002's driver with RadioLib's LoRaWAN stack
   * Implement proper pin configuration for the Heltec board
   * Set up DIO1 interrupt handling for packet reception
   * Add support for OTAA join using credentials from secrets.h
   * Implement the sendReceive functionality for uplinks
   * Use only supported bandwidths identified in the bandwidth testing phase

5. **Class C Implementation Phase**
   * Configure continuous reception on RX2 parameters
   * Implement interrupt-driven packet detection
   * Create a downlink processing mechanism
   * Ensure the radio is re-armed for reception after each uplink and downlink
   * Add proper error handling and recovery mechanisms

6. **Testing Phase**
   * Test basic LoRaWAN functionality (join, send, receive)
   * Test Class C continuous reception
   * Measure downlink latency and reliability
   * Test edge cases (e.g., receiving during transmission, receiving multiple packets)

7. **Integration with Existing Codebase**
   * Refactor LoRaWANHelper to use the new implementation (LORA-009)
   * Ensure compatibility with existing application code
   * Update documentation to reflect the new implementation
   * Create an example for the new implementation (LORA-010)

8. **Documentation and Knowledge Sharing**
   * Update all relevant documentation
   * Create an integration guide (LORA-011)
   * Document lessons learned and best practices
   * Share knowledge with the community through example code 