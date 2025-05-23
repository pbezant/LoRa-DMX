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

# LoRa-DMX Project Memory & Learnings

This document records significant implementation decisions, edge cases handled, problems solved, and approaches rejected (and why) during the development of the LoRa-DMX project.

## Core LoRaWAN Refactor (RadioLib to MCCI LMIC)

**Goal:** Switch from RadioLib + custom LoRaManager to MCCI LoRaWAN LMIC library to enable Class C operations for immediate downlink command response.

**Chosen Approach:** Create a new C++ wrapper library `McciLmicWrapper` for the MCCI LMIC library, keeping the old `LoRaManager` for reference initially.

### Problems Solved & Lessons Learned during MCCI LMIC Integration:

1.  **MCCI LMIC Library Version:** Identified `mcci-catena/MCCI LoRaWAN LMIC library` version `^5.0.1` as suitable for PlatformIO.

2.  **Build Flags (`platformio.ini`):**
    *   Essential flags: `-D ARDUINO_LMIC_PROJECT_CONFIG_H_SUPPRESS`, `-D CFG_us915=1`, `-D CFG_sx126x_board=1`.
    *   Pin definitions via build flags are necessary (e.g., `-D LMIC_NSS=8`, `-D LMIC_DIO1=14`, `-D LMIC_BUSY_LINE=13`, etc.).

3.  **Global `lmic_pins` Structure (Critical for MCCI LMIC v5.0.1):
    *   **Problem:** Initial builds failed with an `undefined reference to 'lmic_pins'` linker error.
    *   **Solution:** The MCCI LMIC library (v5.0.1) requires a global `const lmic_pinmap lmic_pins` structure to be explicitly defined in the main application code (`src/main.cpp`). This structure provides pin mappings to the HAL.
    *   **Gotcha 1:** The exact fields of this `lmic_pinmap` (which is an alias for `Arduino_LMIC::HalPinmap_t`) are defined in the library's `src/arduino_lmic_hal_configuration.h`. For v5.0.1, this structure does **not** include a `.busy` member. Including it caused a compiler error: `'const lmic_pinmap' has no non-static data member named 'busy'`.
    *   **Resolution:** The `lmic_pins` definition in `src/main.cpp` was corrected to only include valid members for `HalPinmap_t` (e.g., `.nss`, `.rst`, `.dio`). The busy line for SX126x is handled by other mechanisms (like the `LMIC_BUSY_LINE` build flag and internal library logic) when `CFG_sx126x_board=1` is set.

4.  **LMIC Callback Functions and C++ Wrapper Interaction:**
    *   **Problem:** Compiler errors related to private access when global C-style LMIC callback functions (`os_getArtEui`, `os_getDevEui`, `os_getDevKey`, `onEvent`) tried to access members of the `McciLmicWrapper` C++ class.
    *   **Solution:**
        *   A public static pointer `McciLmicWrapper* McciLmicWrapper::instance` was used in the wrapper class to allow the global C functions to call instance methods (like `handleEvent`).
        *   Static data members within the wrapper class used by these C functions to retrieve EUIs/Keys (e.g., `s_appEui`, `s_devEui`, `s_appKey`, `s_otaaCredsSet`) **had to be declared `public static`** in the wrapper's header file. Initially, they were private, leading to 'is private within this context' errors from the global C functions.

5.  **Checking LoRaWAN Join Status (`isJoined()`):
    *   **Problem:** The initial implementation of `McciLmicWrapper::isJoined()` used `(LMIC.opmode & OP_JOINED) != 0`. This resulted in a compiler error: `'OP_JOINED' was not declared in this scope`.
    *   **Solution:** Changed the implementation to `LMIC.devaddr != 0`. This checks if a device address has been assigned by the network, which is a reliable indicator of a successful join and is more portable across LMIC versions/configurations.

6.  **Persistent Linter/Compiler Errors due to Caching/Stale Files:**
    *   **Problem:** Throughout the refactoring, the PlatformIO build system and/or linter frequently seemed to be working with cached/stale versions of header files (`.h`). This led to persistent errors (e.g., members reported as private when they were public in the current file version).
    *   **Solution/Workaround:**
        *   Performing a full clean (`~/.platformio/penv/bin/pio run --target clean`).
        *   In one instance, manually replacing the entire content of the `.cpp` file (rather than relying on incremental edits from the AI) seemed to force the build system to recognize the latest header changes.
        *   Restarting the IDE or build environment might also be necessary in such cases.

7.  **Undefined Reference to Debug Function (`printDmxValues`):
    *   **Problem:** Linker error `undefined reference to 'printDmxValues(int, int)'` because the function was forward-declared in `src/main.cpp` and called, but no implementation was provided.
    *   **Solution (Temporary):** Commented out the calls to `printDmxValues` to allow the build to succeed. The function can be implemented later if its debugging output is needed.


## Other Notable Decisions/Learnings:

*   (Placeholder for future learnings) 