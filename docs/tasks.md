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
    *   **Description:** Develop `LoRaManager` and `DmxController` wrapper libraries.
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Simplify API for main application logic.
        *   Encapsulate library-specific configurations.
    *   **PRD Link:** N/A

*   **Task ID:** PATTERN-001
    *   **Description:** Implement basic light pattern command (e.g., `{"pattern": "strobe"}`).
    *   **Assignee:** [Developer Name]
    *   **Requirements/Acceptance Criteria:**
        *   Parse pattern command.
        *   Execute a simple strobe effect on all configured fixtures.
    *   **PRD Link:** N/A

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