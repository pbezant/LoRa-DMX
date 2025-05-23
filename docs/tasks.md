# Current Development Tasks and Requirements

This document tracks current development tasks, their status, and associated requirements. It should be updated regularly to reflect project progress.

## Sprint/Iteration: LoRaWAN Refactor & Class C Implementation

### In Progress

*   **Task ID:** LORA-RFC-001
    *   **Description:** Refactor LoRaWAN functionality to use MCCI LMIC library for Class C operations.
    *   **Assignee:** [Developer Name] / AI Assistant
    *   **Requirements/Acceptance Criteria:**
        *   Replace RadioLib with MCCI LMIC in `platformio.ini`.
        *   Create a new `McciLmicWrapper` library in `lib/`.
        *   Implement OTAA join functionality using `McciLmicWrapper`.
        *   Implement data sending functionality.
        *   Implement basic Class C enable/disable methods.
        *   Ensure proper pin mapping for Heltec LoRa 32 V3 SX1262.
        *   Update `main.cpp` to use the new wrapper.
        *   Device can successfully join TTN and send/receive data using Class C mode.
    *   **PRD Link:** N/A
    *   **Depends On:** N/A
    *   **Status:** Initial `platformio.ini` changes and `McciLmicWrapper` skeleton created.

*   **Task ID:** DOC-UPDATE-001
    *   **Description:** Update all relevant documentation for LoRaWAN MCCI LMIC refactor and Class C changes.
    *   **Assignee:** AI Assistant / [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Update `docs/architecture.md` to reflect new LoRaWAN library and wrapper.
        *   Update `docs/memory.md` with decisions related to MCCI LMIC and Class C.
        *   Update `docs/project.md` with new library in tech stack and API pattern changes.
        *   Update `docs/tasks.md` (this file) with progress.
        *   Update `docs/technical.md` with MCCI LMIC details and Class C patterns.
    *   **PRD Link:** N/A
    *   **Depends On:** LORA-RFC-001
    *   **Status:** Pending completion of LORA-RFC-001


### To Do (Backlog for Current Sprint)

*   **Task ID:** LORA-CLASS-C-002
    *   **Description:** Thoroughly test and validate Class C functionality.
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Confirm continuous receive capability after TX.
        *   Test downlink message reception latency in Class C.
        *   Verify TTN configuration for Class C is correct and compatible.
        *   Assess power consumption implications.
    *   **PRD Link:** N/A
    *   **Depends On:** LORA-RFC-001

*   **Task ID:** JSON-001
    *   **Description:** Implement JSON command parsing for basic DMX control (single fixture, multiple channels).
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Parse JSON payload like `{"address": 1, "channels": [255, 0, 128, 0]}`.
        *   Update DMX output based on parsed JSON.
    *   **PRD Link:** N/A
    *   **Status:** Not Started

*   **Task ID:** DMX-001
    *   **Description:** Initial DMX output implementation and testing. (Carried over)
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Control a single DMX channel on a connected fixture.
        *   Verify DMX signal timing and integrity.
    *   **PRD Link:** N/A
    *   **Status:** Not Started (or carry over progress from previous)

*   **Task ID:** PATTERN-001
    *   **Description:** Implement basic light pattern command (e.g., `{"pattern": "strobe"}`). (Carried over)
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Parse pattern command.
        *   Execute a simple strobe effect on all configured fixtures.
    *   **PRD Link:** N/A
    *   **Status:** Not Started

### On Hold / Superseded

*   **Task ID:** WRAP-001
    *   **Description:** Develop `LoRaManager` and `DmxController` wrapper libraries.
    *   **Assignee:** [Developer Name]
    *   **Status:** Superseded by LORA-RFC-001 (for LoRaManager part). DmxController part might still be relevant or separate.
    *   **Notes:** The `LoRaManager` (based on RadioLib) is being replaced by `McciLmicWrapper`.

*   **Task ID:** CORE-001
    *   **Description:** Basic LoRaWAN communication setup and testing with TTN. (Using RadioLib)
    *   **Assignee:** [Developer Name]
    *   **Status:** Superseded by LORA-RFC-001.

### Completed (This Sprint/Phase)

*   **Task ID:** SETUP-001
    *   **Description:** Initial project setup with PlatformIO, Heltec board configuration, and core library dependencies.
    *   **Assignee:** [Developer Name]
    *   **Completion Date:** [Date]
    *   **Link to PR/Commit:** [Initial Commit Link]

## Future/Backlog (Beyond Current Sprint)

*   Implement all advanced pattern commands from `README.md`.
*   Add support for saving/loading configurations (e.g., default DMX scenes).
*   Develop robust error handling and reporting via LoRaWAN uplink (using MCCI LMIC).
*   Investigate power optimization for battery operation (especially with Class C considerations).
*   Create a more comprehensive TTN payload formatter for uplink status messages.
*   Add more detailed debugging output options for `McciLmicWrapper`.

## Blockers

*   Verification of `lmic_pinmap` for Heltec LoRa 32 V3 SX1262 in `McciLmicWrapper.cpp` (Critical for LORA-RFC-001).
*   [e.g., Waiting for DMX test fixtures to arrive] 