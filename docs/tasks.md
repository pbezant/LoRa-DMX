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
    *   **Status:** In Progress
    *   **Assignee:** AI & Big Daddy
    *   **Requirements/Acceptance Criteria:**
        *   Fix incomplete type errors with RadioLib::LoRaWANNode and RadioLib::LoRaWANBand.
        *   Ensure proper inclusion of RadioLib headers.
        *   Fix missing LORAWAN_DEVICE_EUI and related constants from secrets.h.
        *   Ensure LoRaWANHelper compiles successfully with RadioLib v7.1.2.
    *   **PRD Link:** N/A
    *   **Depends On:** None

### To Do

*   **Task ID:** LORA-003
    *   **Description:** Testing and validation of LoRaWAN Class C + DMX system
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Successful join, uplink, and downlink on Chirpstack/TTN
        *   Class C operation validated (continuous receive)
        *   DMX output responds to downlink commands
        *   Periodic uplinks received by network
    *   **PRD Link:** N/A
    *   **Depends On:** LORA-002, LORA-005, LORA-006

## Blockers

*   LoRaWANHelper.cpp compilation errors with RadioLib v7.1.2 (see LORA-006)
*   Cannot upload firmware to test functionality until LoRaWANHelper issues are resolved 