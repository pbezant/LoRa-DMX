# Implementation Decisions, Edge Cases, Problems Solved, and Rejected Approaches

This document records key implementation decisions, how edge cases were handled, significant problems overcome, and approaches that were considered but ultimately rejected (along with the reasoning).

## Implementation Decisions

*   **Choice of Heltec LoRa 32 V3:** Selected for its integrated ESP32, LoRa capabilities, and Arduino compatibility, providing a good balance of processing power and connectivity for this type of application.
*   **OTAA for LoRaWAN Activation:** Chosen for better security compared to ABP (Activation By Personalization).
*   **Use of `RadioLib`:** A comprehensive library for various radio modules, offering robust LoRaWAN support.
*   **Custom `LoRaManager` Wrapper:** Likely created to simplify `RadioLib` usage, encapsulate TTN credentials and connection logic, and provide a cleaner interface for the main application.
*   **Use of `esp_dmx`:** A dedicated library for ESP32 to handle DMX512 protocol complexities and timing.
*   **Custom `DmxController` Wrapper:** Similar to `LoRaManager`, likely developed to abstract `esp_dmx` functionalities for easier use in the main application (e.g., managing multiple fixtures from a JSON payload).
*   **`ArduinoJson` for Payload Parsing:** A standard and efficient library for handling JSON on microcontrollers, suitable for parsing commands.
*   **PlatformIO for Build System:** Chosen for its advanced features, library management, and board support, especially for ESP32 projects.

## Edge Cases Handled

*   **(Potentially) LoRaWAN Join Failures:** The `LoRaManager` likely includes retry logic or error reporting for failed TTN joins.
*   **(Potentially) Malformed JSON Payloads:** `ArduinoJson` provides error codes on parsing failure; the application logic should ideally handle these gracefully (e.g., ignore command, log error).
*   **(Potentially) DMX Universe Limits:** DMX512 supports 512 channels. The code needs to ensure commands don't try to address channels beyond this or overlap fixtures incorrectly (though `esp_dmx` might handle some of this).
*   **(Potentially) LoRaWAN Downlink Timing/Missed Messages:** LoRaWAN Class A devices only receive downlinks after an uplink. The application design must account for potential delays or missed commands.

## Problems Solved

*   **Remote DMX Control:** Achieved wireless control of DMX fixtures over a long-range, low-power network.
*   **Dynamic Fixture Configuration:** Using JSON allows changing DMX addresses and channel counts without reflashing firmware.
*   **Simplified LoRaWAN & DMX Integration:** Achieved through wrapper libraries, making the main application logic cleaner.

## Rejected Approaches

*   **(Possibly) ABP for LoRaWAN:** OTAA was chosen, likely for better security and key management.
*   **(Possibly) Custom Binary Command Protocol:** JSON was chosen, likely for its flexibility and human-readability, despite a slightly larger payload size.
*   **(Possibly) Other LoRa Libraries:** `RadioLib` was selected, reasons could include feature set, community support, or prior experience.
*   **(Possibly) Bit-banging DMX:** Using a dedicated library like `esp_dmx` is more reliable and less CPU-intensive than implementing the DMX protocol manually. 