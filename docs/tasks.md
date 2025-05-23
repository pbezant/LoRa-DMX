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
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Control a single DMX channel on a connected fixture.
        *   Verify DMX signal timing and integrity.
    *   **PRD Link:** N/A
    *   **Depends On:** N/A

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
    *   **Status:** In Progress (Core LoRaWrapper Implementation Done)
    *   **Notes:** Core `LoRaWrapper` (`ILoRaWanDevice` interface, `HeltecLoRaWan` concrete class) structure implemented. Non-intrusive callback strategy for Heltec backend developed, providing `onJoined`, `onJoinFailed` (timeout), `onDataReceived`. `onSendConfirmed` for confirmed messages currently indicates send cycle completion, not guaranteed server ACK. Full application integration and rigorous testing pending.
    *   **Requirements/Acceptance Criteria:**
        *   Simplify API for main application logic for LoRaWAN communication.

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

## Future/Backlog (Beyond Current Sprint)

*   Implement all advanced pattern commands from `README.md`.
*   Add support for saving/loading configurations (e.g., default DMX scenes).
*   Develop robust error handling and reporting via LoRaWAN uplink.
*   Investigate power optimization for battery operation.
*   Create a more comprehensive TTN payload formatter for uplink status messages.
*   Add more detailed debugging output options.

## Blockers

*   [e.g., Waiting for DMX test fixtures to arrive] 