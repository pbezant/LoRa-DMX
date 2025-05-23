# Technical Specifications and Established Patterns

This document details technical specifications for various aspects of the project and outlines established coding patterns and practices.

## Coding Standards

*   **Language Version(s):** C++ (Arduino flavor, typically based on a recent C++ standard like C++11 or C++17 depending on ESP-IDF version used by PlatformIO for ESP32)
*   **Linting & Formatting:** (Not explicitly defined in `platformio.ini`, but PlatformIO can integrate with tools like ClangFormat. Assume default or user-configured.)
*   **Naming Conventions:** (Likely Arduino/C++ conventions, e.g., camelCase for functions/variables, PascalCase for classes/structs, UPPER_CASE for constants/macros. To be confirmed by inspecting source code.)

## Established Patterns

### Pattern A: Wrapper Libraries for Hardware Abstraction

*   **Description:** Custom libraries (`LoRaManager`, `DmxController` mentioned in README) are used to simplify interaction with underlying hardware/communication libraries (`RadioLib`, `esp_dmx`). This provides a cleaner API for the main application logic and encapsulates hardware-specific details.
*   **When to Use:** When interacting with complex external libraries or hardware peripherals to provide a simplified and project-specific interface.
*   **Example (or link to example code):**
    ```cpp
    // Conceptual example based on README description
    // LoRaManager lora;
    // lora.joinNetwork(joinEUI, devEUI, appKey);
    // if (lora.messageReceived()) { /* process message */ }

    // DmxController dmx;
    // dmx.setChannel(1, 255);
    ```

### Pattern B: JSON for Dynamic Configuration/Commands

*   **Description:** JSON payloads received over LoRaWAN are parsed to dynamically control DMX fixtures. This avoids hardcoding fixture settings and allows flexible remote configuration.
*   **When to Use:** For sending structured data or commands to the device, especially when the parameters can vary.
*   **Example (or link to example code):** (See JSON command format in `README.md`)

## API Specifications

*   **Primary Interface:** LoRaWAN Downlink Messages via The Things Network (TTN)
*   **Payload Format:** JSON (see `README.md` for structure). Also supports simplified text/numeric commands via TTN payload formatter.
*   **Authentication:** OTAA (Over-The-Air Activation) for LoRaWAN network join. AppKey provides device authentication.
*   **TTN Payload Formatter:** `ttn_payload_formatter.js` (JavaScript) used in TTN console to decode uplink and encode downlink messages.

## Security Considerations

*   **LoRaWAN Security:** Relies on standard LoRaWAN security mechanisms (AES-128 encryption for network and application sessions).
*   **Credential Management:** `joinEUI`, `devEUI`, and `appKey` are critical for network access. `README.md` indicates these are in `main.cpp` and should be user-configured.
*   **Input Sanitization:** (Assumed to be handled by `ArduinoJson` during parsing, but complex or malformed JSON could still be a concern. Further inspection of parsing logic in source code needed.)

## Performance Guidelines

*   **Real-time DMX:** DMX requires consistent timing. `esp_dmx` library likely handles this at a low level.
*   **LoRaWAN Duty Cycle:** Adherence to LoRaWAN regional duty cycle limitations is critical (managed by `RadioLib` and TTN).
*   **JSON Parsing:** `ArduinoJson` is generally efficient, but very large/complex JSON payloads could impact performance or memory on the microcontroller. Payload size is also limited by LoRaWAN constraints.
*   **Power Consumption:** For battery-powered applications (if any), deep sleep and minimizing radio transmission time are important. (Heltec LoRa 32 V3 has Wi-Fi/Bluetooth which should be disabled if not used to save power).

## LoRaWAN (MCCI LoRaWAN LMIC library)

The project uses the MCCI LoRaWAN LMIC library (v5.0.1 or similar) for LoRaWAN communication.

### Pin Configuration for Heltec LoRa 32 V3 (SX1262)

Proper pin configuration is critical. It involves both build flags in `platformio.ini` and a global `lmic_pins` structure definition in `src/main.cpp`.

**1. `platformio.ini` Build Flags:**

Ensure the following build flags are set for the Heltec LoRa 32 V3 (SX1262):
```ini
build_flags =
    -D ARDUINO_LMIC_PROJECT_CONFIG_H_SUPPRESS
    -D CFG_us915=1          ; Region (US915 in this case)
    -D CFG_sx126x_board=1   ; Specifies an SX126x radio
    ; Pin definitions for MCCI LMIC with SX1262
    -D LMIC_NSS=8
    -D LMIC_RST=12
    -D LMIC_DIO0=4               ; Must be a defined GPIO, even if not actively used for SX126x interrupts (e.g., GPIO4)
    -D LMIC_DIO1=14              ; Critical for SX126x
    -D LMIC_DIO2=LMIC_UNUSED_PIN ; Or specific pin if used for RF switch
    -D LMIC_BUSY_LINE=13         ; Busy line for SX126x
    ; -D LMIC_CLOCK_ERROR_PPM=0 ; Optional: clock error adjustment
    ; -D LMIC_SPI_CLOCK=1000000 ; Optional: SPI clock speed
```

**2. Global `lmic_pins` Structure (`src/main.cpp`):**

A global `const lmic_pinmap lmic_pins` structure *must* be defined in `src/main.cpp`. The fields of this structure must exactly match the `Arduino_LMIC::HalPinmap_t` definition for the MCCI LMIC library version being used (v5.0.1 in this case).

For MCCI LMIC v5.0.1 and SX126x (Heltec LoRa 32 V3):
```cpp
// Located in src/main.cpp
#include <lmic.h> // For lmic_pinmap and LMIC_UNUSED_PIN

// Required by MCCI LMIC library for SX126x
// Pin mapping for Heltec LoRa 32 V3 (SX1262)
const lmic_pinmap lmic_pins = {
    .nss = 8,      // LoRa Chip Select
    .rst = 12,     // LoRa Reset
    .dio = { /* DIO0 */ 4, /* DIO1 */ 14, /* DIO2 */ LMIC_UNUSED_PIN }, // DIO0 must be a defined GPIO (e.g., GPIO4)
    // The HalPinmap_t for MCCI LMIC v5.0.1 does NOT include a '.busy' member directly.
    // Busy line is handled by other mechanisms (e.g., build flags, internal DIO usage).
    // Other fields like .rxtx, .rxtx_rx_active, .rssi_cal, .spi_freq will use defaults
    // if not specified or can be set to LMIC_UNUSED_PIN / 0 if not applicable.
};
```
**Important Note on `dio[0]`:** Even for SX126x radios where DIO1 is the primary interrupt line, the MCCI LMIC HAL (`hal.cpp`) expects `plmic_pins->dio[0]` to be a valid, defined GPIO pin (not `LMIC_UNUSED_PIN`). It performs an `ASSERT(plmic_pins->dio[0] != LMIC_UNUSED_PIN)` during initialization. If this pin is set to `LMIC_UNUSED_PIN`, the assertion will fail, leading to a HAL failure message (e.g., `FAILURE ...hal.cpp:35` or similar) and a subsequent watchdog timeout. Therefore, assign an unused GPIO to `dio[0]` (e.g., GPIO4) in both the `lmic_pins` struct and the `LMIC_DIO0` build flag.

**Important Note on `.busy` field:** The `HalPinmap_t` structure for MCCI LMIC v5.0.1 (found in `src/arduino_lmic_hal_configuration.h` within the library) defines the available fields. It does *not* include a `.busy` member. The busy line functionality for SX126x is managed through other mechanisms by the library when `CFG_sx126x_board=1` and `LMIC_BUSY_LINE` are defined.

### LMIC Callbacks and Wrapper Class

When using a C++ wrapper class for the MCCI LMIC library (e.g., `McciLmicWrapper`):
*   The global C-style callback functions required by LMIC (`os_getArtEui`, `os_getDevEui`, `os_getDevKey`, and the global `onEvent`) are defined outside the class.
*   If these global functions need to access data or methods of the wrapper class instance (e.g., to retrieve EUIs/Keys or call an event handler method), they typically do so via a static public pointer to the class instance (e.g., `McciLmicWrapper::instance`).
*   Similarly, any static data members within the wrapper class that these global C functions need to read (e.g., static arrays storing EUIs/Keys like `McciLmicWrapper::s_appEui`) **must be declared `public static`** in the wrapper class's header file.

### Checking Join Status

To check if the device has successfully joined the network:
*   Use `LMIC.devaddr != 0` within the wrapper class or application code.
*   The internal LMIC flag `OP_JOINED` (e.g., `(LMIC.opmode & OP_JOINED) != 0`) might not be reliably defined or accessible across all LMIC versions or configurations and can lead to compilation errors. `LMIC.devaddr != 0` is a more portable check. 