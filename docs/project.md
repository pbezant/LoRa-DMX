# Core Architecture, Tech Stack, API Patterns, and Database Schema

This document outlines the core architectural decisions, the primary technologies used, established API patterns, and an overview of the database schema.

## Core Architecture Decisions

*   **Microcontroller-centric:** The Heltec LoRa 32 V3 is the central processing unit, handling LoRaWAN communication, DMX signal generation, and command parsing.
*   **LoRaWAN for Remote Control:** Utilizes The Things Network (TTN) for long-range, low-power wireless command reception.
*   **JSON for Command Structure:** Employs JSON as the data format for DMX control commands, allowing for flexible and structured data.
*   **Modular Libraries:** Leverages dedicated libraries for distinct functionalities (LoRaWAN via RadioLib/LoRaManager, DMX via esp_dmx, JSON via ArduinoJson) to promote separation of concerns.

## Technology Stack

*   **Microcontroller:** Heltec LoRa 32 V3 (ESP32-based)
*   **Framework:** Arduino
*   **Primary Language:** C++ (Arduino)
*   **Key Libraries:**
    *   `RadioLib` (for LoRa/LoRaWAN communication)
    *   `LoRaManager` (custom wrapper for RadioLib)
    *   `ArduinoJson` (for JSON parsing)
    *   `esp_dmx` (for DMX512 control)
*   **Communication Protocol:** LoRaWAN (via The Things Network - TTN)
*   **Command Data Format:** JSON
*   **Development Environment:** PlatformIO

## API Patterns

*   **Downlink Commands via TTN:** The primary "API" is through LoRaWAN downlink messages formatted in JSON, sent via The Things Network.
*   **Payload Formatters:** TTN payload formatters (JavaScript) are used to decode/encode messages between the network server and the device, supporting both rich JSON and simplified command structures.
*   **Error Handling:** Relies on serial monitor debugging and potentially TTN event logs. (Further details on in-device error handling might be found in source code).

## Database Schema Overview

*   **N/A:** This project is an embedded system and does not directly involve a traditional database. Configuration (like DMX fixture addresses) is managed by the incoming JSON commands. LoRaWAN credentials (joinEUI, devEUI, appKey) are hardcoded or configured at deployment. 