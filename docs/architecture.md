# System Architecture and Component Relationships

This document describes the overall system architecture, its major components, and how they interact.

## High-Level Overview

This project implements an embedded DMX lighting controller. The system's core is a Heltec LoRa 32 V3 microcontroller. It receives commands via LoRaWAN from The Things Network (TTN), parses these commands (typically JSON), and then drives DMX lighting fixtures accordingly. The architecture is monolithic on the embedded device, with distinct C++ classes/libraries handling specific concerns like LoRa communication, DMX output, and command processing.

[Diagram Idea: TTN Cloud -> (LoRaWAN) -> Heltec LoRa 32 V3 [RadioLib/LoRaManager -> JSON Parser (ArduinoJson) -> DMX Logic (esp_dmx/DmxController)] -> MAX485 -> DMX Fixtures]

## Major Components

### 1. Heltec LoRa 32 V3 Microcontroller
*   **Responsibilities:** Main processing unit. Runs the Arduino application firmware. Manages all peripherals and executes control logic.
*   **Key Technologies:** ESP32, Arduino framework.

### 2. LoRaWAN Communication Module (LoRaManager & RadioLib)
*   **Responsibilities:** Handles joining The Things Network (TTN) via OTAA. Manages receiving downlink messages (commands) and potentially sending uplink messages (status/acknowledgments, though not detailed in README).
*   **Key Technologies:** `RadioLib` library, custom `LoRaManager` wrapper, SX1262 LoRa transceiver.
*   **Interfaces/APIs Exposed:** Provides an API to the main application for sending/receiving LoRaWAN messages (e.g., `lora.joinNetwork()`, `lora.messageReceived()`, `lora.getPayload()`).
*   **Dependencies:** `RadioLib` library, underlying ESP32 hardware SPI for radio communication.

### 3. Command Processing Module (ArduinoJson & Custom Logic)
*   **Responsibilities:** Parses incoming JSON payloads from LoRaWAN messages. Extracts DMX addresses and channel values for individual fixtures or pattern commands.
*   **Key Technologies:** `ArduinoJson` library.
*   **Interfaces/APIs Exposed:** Consumes raw payload data from the LoRaWAN module. Outputs structured DMX control information to the DMX Control Module.
*   **Dependencies:** `ArduinoJson` library.

### 4. DMX Control Module (DmxController & esp_dmx)
*   **Responsibilities:** Takes structured DMX control information (addresses, channel values). Manages the DMX bus timing and sends DMX signals to connected fixtures via a MAX485 transceiver.
*   **Key Technologies:** `esp_dmx` library, custom `DmxController` wrapper, MAX485.
*   **Interfaces/APIs Exposed:** Provides an API to the main application/command processing module to set DMX channel values (e.g., `dmx.setChannel(address, value)` or `dmx.updateFixtures(jsonData)`).
*   **Dependencies:** `esp_dmx` library, MAX485 hardware interface (GPIO pins for TX, DE/RE).

### 5. The Things Network (TTN)
*   **Responsibilities:** Provides the LoRaWAN network server infrastructure. Routes messages to/from the end device. Allows for payload formatting and integration with other services (not detailed in README).
*   **Key Technologies:** LoRaWAN, MQTT (often used for application integration with TTN).

## Data Flow

1.  **Command Origination:** User/External System sends a command (e.g., via TTN console, MQTT, or HTTP integration into TTN).
2.  **TTN Processing:** Command is received by TTN. If a downlink payload formatter is configured (like `ttn_payload_formatter.js`), it may transform the command into the binary format expected by the LoRaWAN downlink.
3.  **LoRaWAN Transmission:** TTN schedules and transmits the command as a LoRaWAN downlink message to the Heltec device.
4.  **Device Reception:** `LoRaManager`/`RadioLib` on the Heltec device receives the LoRaWAN message.
5.  **Payload Extraction:** The main application retrieves the payload from `LoRaManager`.
6.  **JSON Parsing:** `ArduinoJson` parses the payload if it's in JSON format.
7.  **Logic Execution:** The application logic interprets the parsed command (e.g., identify target DMX addresses and channel values).
8.  **DMX Output:** The `DmxController`/`esp_dmx` library is instructed to update the DMX universe with the new channel values.
9.  **Signal Transmission:** The MAX485 transceiver converts the microcontroller's logic-level signals to DMX-standard RS-485 signals, which are sent to the DMX fixtures.

## Key Architectural Decisions

*   **Use of Existing Libraries:** Leveraging well-tested libraries like `RadioLib`, `ArduinoJson`, and `esp_dmx` accelerates development and ensures robust handling of complex protocols.
*   **Custom Wrappers (`LoRaManager`, `DmxController`):** These likely simplify the main application code by providing a higher-level, project-specific API over the more generic libraries. This improves readability and maintainability.
*   **Reliance on TTN for Network Infrastructure:** Offloads the complexity of managing a LoRaWAN network server to a public/community service (or private TTN instance).
*   **JSON as Flexible Command Language:** Allows for complex and extensible commands without needing to redefine a binary protocol for every new feature. 

## Dynamic Light Count Configuration (Config Downlink)

- The system supports a configuration downlink ([0xC0, N]) to set the number of DMX fixtures at runtime.
- This is handled in the downlink callback, which updates the fixture count and re-initializes the DMX controller.
- The codec and firmware are coordinated to support this feature, allowing remote reconfiguration without redeployment. 