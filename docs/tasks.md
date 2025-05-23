# Current Development Tasks and Requirements

This document tracks current development tasks, their status, and associated requirements. It should be updated regularly to reflect project progress.

## Sprint/Iteration: Initial Development Phase

### In Progress

*   **Task ID:** CORE-001
    *   **Description:** Basic LoRaWAN communication setup and testing with TTN.
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Device successfully joins TTN using OTAA.
        *   Device can receive simple downlink messages.
        *   Device can send simple uplink messages (e.g., ACK, status).
    *   **PRD Link:** N/A
    *   **Depends On:** N/A



*   **Task ID:** DMX-001
    *   **Description:** Initial DMX output implementation and testing.
    *   **Assignee:** AI Assistant
    *   **Status:** ✅ **COMPLETED**
    *   **Completion Date:** Current Session
    *   **Notes:** Resolved ESP32S3 compatibility issues by implementing custom UART-based DMX controller. Created fully functional DmxController class with FreeRTOS task management, proper DMX timing, and support for multi-channel fixtures.
    *   **Requirements/Acceptance Criteria:**
        *   ✅ Custom DMX implementation that compiles successfully on ESP32S3
        *   ✅ Proper DMX512 timing (break, mark-after-break, data transmission)
        *   ✅ Multi-channel fixture support via JSON parsing
    *   **PRD Link:** N/A
    *   **Depends On:** N/A

*   **Task ID:** BUILD-001
    *   **Description:** Resolve compilation and linker errors for ESP32S3 target.
    *   **Assignee:** AI Assistant  
    *   **Status:** ✅ **COMPLETED**
    *   **Completion Date:** Current Session
    *   **Notes:** Fixed all compilation issues including DMX library incompatibility and missing LoRaWAN global variables. Created `src/lora_globals.cpp` to define required symbols.
    *   **Requirements/Acceptance Criteria:**
        *   ✅ Project compiles successfully without errors
        *   ✅ All library dependencies resolved
        *   ✅ Linker errors for LoRaWAN globals resolved

### In Progress

### To Do (Backlog for Current Sprint)

*   **Task ID:** JSON-001
    *   **Description:** Implement JSON command parsing for basic DMX control (single fixture, multiple channels).
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Parse JSON payload like `{"address": 1, "channels": [255, 0, 128, 0]}`.
        *   Update DMX output based on parsed JSON.
    *   **PRD Link:** N/A

*   **Task ID:** WRAP-001
    *   **Description:** Develop `LoRaManager` (now `LoRaWrapper`) and `DmxController` wrapper libraries.
    *   **Assignee:** [Developer Name] / AI Assistant
    *   **Status:** **Partially Complete** (LoRaWrapper: ✅ Done, DmxController: ✅ Done)
    *   **Notes:** 
        *   ✅ Core `LoRaWrapper` (`ILoRaWanDevice` interface, `HeltecLoRaWan` concrete class) structure implemented
        *   ✅ Custom `DmxController` wrapper completed with ESP32S3-compatible UART implementation
        *   ✅ Non-intrusive callback strategy for Heltec backend developed, providing `onJoined`, `onJoinFailed` (timeout), `onDataReceived`
        *   ⏳ Full application integration and rigorous testing pending
    *   **Requirements/Acceptance Criteria:**
        *   ✅ Simplify API for main application logic for LoRaWAN communication
        *   ✅ Custom DMX controller with proper timing and multi-fixture support

*   **Task ID:** PATTERN-001
    *   **Description:** Implement basic light pattern command (e.g., `{"pattern": "strobe"}`).
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Parse pattern command.
        *   Execute a simple strobe effect on all configured fixtures.
    *   **PRD Link:** N/A

*   **Task ID:** WRAP-INT-001
    *   **Description:** Integrate `LoRaWrapper` into main application (`src/main.cpp`) and perform initial communication tests (join, uplink, downlink).
    *   **Assignee:** [Developer Name] / AI Assistant
    *   **Status:** To Do
    *   **Requirements/Acceptance Criteria:**
        *   `main.cpp` uses `LoRaWrapper` for all LoRaWAN operations.
        *   Device successfully joins LoRaWAN network via the wrapper.
        *   Test uplink and downlink message handling via wrapper callbacks.
    *   **PRD Link:** N/A
    *   **Depends On:** WRAP-001

### Completed (This Sprint)

*   **Task ID:** SETUP-001
    *   **Description:** Initial project setup with PlatformIO, Heltec board configuration, and core library dependencies.
    *   **Assignee:** [Developer Name]
    *   **Completion Date:** [Date]
    *   **Link to PR/Commit:** [Initial Commit Link]

*   **Task ID:** SETUP-001
    *   **Description:** Initial project setup with PlatformIO, Heltec board configuration, and core library dependencies.
    *   **Assignee:** [Developer Name]
    *   **Completion Date:** [Date]
    *   **Link to PR/Commit:** [Initial Commit Link]

## Future/Backlog (Beyond Current Sprint)

*   Implement all advanced pattern commands from `README.md`.
*   Add support for saving/loading configurations (e.g., default DMX scenes).
*   Develop robust error handling and reporting via LoRaWAN uplink.
*   Investigate power optimization for battery operation.
*   Create a more comprehensive TTN payload formatter for uplink status messages.
*   Add more detailed debugging output options.

## Blockers

*   [e.g., Waiting for DMX test fixtures to arrive] 