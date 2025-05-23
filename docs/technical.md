# Technical Specifications and Established Patterns

This document details technical specifications for various aspects of the project and outlines established coding patterns and practices.

## Coding Standards

*   **Language Version(s):** C++ (Arduino flavor, typically based on a recent C++ standard like C++11 or C++17 depending on ESP-IDF version used by PlatformIO for ESP32)
*   **Linting & Formatting:** (Not explicitly defined in `platformio.ini`, but PlatformIO can integrate with tools like ClangFormat. Assume default or user-configured.)
*   **Naming Conventions:** (Likely Arduino/C++ conventions, e.g., camelCase for functions/variables, PascalCase for classes/structs, UPPER_CASE for constants/macros. To be confirmed by inspecting source code.)

## Established Patterns

### Pattern A: Wrapper Libraries for Hardware Abstraction

*   **Description:** Custom libraries (`LoRaWrapper`, `DmxController`) are used to simplify interaction with underlying hardware/communication libraries. The `LoRaWrapper` provides an abstract interface (`ILoRaWanDevice`) with concrete implementations like `HeltecLoRaWan`. The `DmxController` implements a custom UART-based DMX512 protocol optimized for ESP32S3. This provides a cleaner API for the main application logic and encapsulates hardware-specific details.
*   **When to Use:** When interacting with complex external libraries or hardware peripherals to provide a simplified and project-specific interface.
*   **Example (or link to example code):**
    ```cpp
    // LoRaWrapper usage with interface
    ILoRaWanDevice* loraDevice = new HeltecLoRaWan();
    loraDevice->init(CLASS_C, LORAMAC_REGION_US915, &callbacks);
    loraDevice->join();

    // DmxController usage with custom UART implementation
    DmxController dmx;
    dmx.begin(TX_PIN, RX_PIN, DIR_PIN, 2); // Serial port 2
    dmx.setChannel(1, 255);
    dmx.setFixtureChannels(1, fixtureData); // Multi-channel fixture
    dmx.update(); // Send DMX frame
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
*   **Credential Management:** 
    - LoRaWAN credentials are stored in `include/secrets.h`:
      ```cpp
      // Device EUI (8 bytes)
      #define DEVEUI "522b03a2a79b1d87"
      
      // Application EUI (8 bytes)
      #define APPEUI "27ae8d0509c0aa88"
      
      // Application Key (16 bytes)
      #define APPKEY "2d6fb7d54929b790c52e66758678fb2e"
      
      // Network Key (16 bytes, same as APPKEY for LoRaWAN 1.0.x)
      #define NWKKEY "2d6fb7d54929b790c52e66758678fb2e"
      ```
    - The `secrets.h` file should be in `.gitignore` to prevent credential exposure
    - Credentials are converted from hex strings to byte arrays during initialization
*   **Input Sanitization:** (Assumed to be handled by `ArduinoJson` during parsing, but complex or malformed JSON could still be a concern. Further inspection of parsing logic in source code needed.)

## Performance Guidelines

*   **Real-time DMX:** DMX requires consistent timing. Our custom UART-based implementation uses FreeRTOS tasks and precise timing control to ensure proper DMX512 break (100μs) and mark-after-break (12μs) timing requirements.
*   **LoRaWAN Duty Cycle:** Adherence to LoRaWAN regional duty cycle limitations is critical (managed by `LoRaWrapper` and TTN).
*   **JSON Parsing:** `ArduinoJson` is generally efficient, but very large/complex JSON payloads could impact performance or memory on the microcontroller. Payload size is also limited by LoRaWAN constraints.
*   **Power Consumption:** For battery-powered applications (if any), deep sleep and minimizing radio transmission time are important. (Heltec LoRa 32 V3 has Wi-Fi/Bluetooth which should be disabled if not used to save power).

## Using the Heltec LoRaWAN Library

The project now uses the official Heltec LoRaWAN library directly for LoRaWAN communication, which provides native support for the Heltec LoRa 32 V3 board and its SX126x radio module.

### Integration Steps:

1.  **Include Headers:**
    In your `main.cpp`, include the necessary Heltec headers:
    ```cpp
    #include "Arduino.h"
    #include "LoRaWan_APP.h"
    #include "HT_SSD1306Wire.h"
    ```

2.  **Define LoRaWAN Parameters:**
    ```cpp
    // LoRaWAN parameters are defined in lora_globals.cpp
    extern uint8_t devEui[];
    extern uint8_t appEui[];
    extern uint8_t appKey[];
    extern uint8_t nwkSKey[];
    extern uint8_t appSKey[];
    extern uint32_t devAddr;
    extern uint8_t loraWanClass;
    extern bool overTheAirActivation;
    extern LoRaMacRegion_t loraWanRegion;
    ```

3.  **Implement Callback Functions:**
    ```cpp
    // Join callback
    void joinCallback(void) {
        if (deviceState == DEVICE_STATE_JOIN_FAILED) {
            Serial.println("LoRaWAN Join Failed!");
            // Handle join failure
        } else {
            Serial.println("LoRaWAN Joined Successfully!");
            // Proceed with application logic
        }
    }

    // Receive callback
    void downLinkDataHandle(McpsIndication_t *mcpsIndication) {
        Serial.printf("Received LoRaWAN data on port %d\n", mcpsIndication->Port);
        // Process received data
        for (uint8_t i = 0; i < mcpsIndication->BufferSize; i++) {
            Serial.printf("%02X", mcpsIndication->Buffer[i]);
        }
        Serial.println();
    }
    ```

4.  **Initialize in `setup()`:**
    ```cpp
    void setup() {
        Serial.begin(115200);
        
        // Initialize Heltec hardware
        Mcu.begin();
        
        // Initialize LoRaWAN device state
        deviceState = DEVICE_STATE_INIT;
        
        // Start LoRaWAN join process
        LoRaWAN.init(loraWanClass, loraWanRegion);
        
        // For debugging
        Serial.printf("LoRaWAN Class: %d\n", loraWanClass);
        Serial.printf("Region: %d\n", loraWanRegion);
    }
    ```

5.  **Process in `loop()`:**
    ```cpp
    void loop() {
        switch (deviceState) {
            case DEVICE_STATE_INIT: {
                // Initialize LoRaWAN stack
                LoRaWAN.init(loraWanClass, loraWanRegion);
                deviceState = DEVICE_STATE_JOIN;
                break;
            }
            case DEVICE_STATE_JOIN: {
                // Start join process
                LoRaWAN.join();
                break;
            }
            case DEVICE_STATE_SEND: {
                // Example: Send data
                uint8_t payload[] = "Hello";
                LoRaWAN.send(sizeof(payload), payload, 1, false);
                deviceState = DEVICE_STATE_CYCLE;
                break;
            }
            case DEVICE_STATE_CYCLE: {
                // Wait for next transmission
                deviceState = DEVICE_STATE_SLEEP;
                break;
            }
            case DEVICE_STATE_SLEEP: {
                // Handle sleep/wake timing
                LoRaWAN.sleep(loraWanClass);
                break;
            }
            default: {
                deviceState = DEVICE_STATE_INIT;
                break;
            }
        }
    }
    ```

### Key Considerations:

*   **Board Initialization:** Always call `Mcu.begin()` before initializing LoRaWAN.
*   **Global Variables:** Required LoRaWAN parameters must be defined in `lora_globals.cpp`.
*   **State Machine:** The Heltec library uses a state machine approach for device management.
*   **Callbacks:** Use the library's native callback functions for event handling.
*   **Power Management:** The library provides sleep functionality for power optimization.
*   **Error Handling:** Monitor `deviceState` for error conditions and handle appropriately.

## Custom DMX Implementation

Due to ESP32S3 compatibility issues with existing DMX libraries, a custom UART-based DMX512 implementation was developed. This section details the technical implementation.

### DMX Controller Architecture

*   **UART-Based Communication:** Uses ESP32S3's HardwareSerial for precise timing control and RS-485 signal generation.
*   **FreeRTOS Task Management:** Dedicated task for continuous DMX frame transmission with proper timing.
*   **Thread-Safe Operation:** Mutex protection for DMX data buffer updates.
*   **Multi-Channel Fixture Support:** JSON parsing for complex fixture configurations.

### Key Technical Details

*   **DMX Timing Compliance:**
    *   Break: 100μs minimum (configurable)
    *   Mark After Break: 12μs minimum (configurable)
    *   Data transmission: 250,000 baud (4μs per bit)
*   **Hardware Requirements:**
    *   TX Pin: DMX data output
    *   RX Pin: Optional (for future DMX input support)
    *   Direction Pin: MAX485 DE/RE control for half-duplex operation
*   **Memory Usage:**
    *   513-byte DMX buffer (512 channels + start code)
    *   FreeRTOS task stack
    *   Mutex overhead

### Usage Pattern

```cpp
// Initialize DMX controller
DmxController dmx;
if (dmx.begin(TX_PIN, RX_PIN, DIR_PIN, 2)) {
    dmx.start(); // Begin continuous transmission
    
    // Set individual channels
    dmx.setChannel(1, 255);
    
    // Set multi-channel fixtures via JSON
    StaticJsonDocument<256> fixture;
    fixture["channels"] = JsonArray();
    fixture["channels"][0] = 255; // Red
    fixture["channels"][1] = 128; // Green  
    fixture["channels"][2] = 64;  // Blue
    dmx.setFixtureChannels(10, fixture);
    
    // Update DMX output
    dmx.update();
}
```

## LoRaWAN Global Variables Setup

The Heltec LoRaWAN library requires specific global variables to be defined. These are centralized in `src/lora_globals.cpp`:

### Required Global Variables

*   **Credentials:** `devEui[]`, `appEui[]`, `appKey[]` for OTAA
*   **ABP Variables:** `nwkSKey[]`, `appSKey[]`, `devAddr` (required for compilation even if using OTAA)
*   **Configuration:** `loraWanRegion`, `loraWanClass`, `userChannelsMask[]`
*   **Radio Structure:** `const struct Radio_s Radio` with function pointers (can be null for Heltec library)

### Implementation Notes

*   All global variables must be defined in the global scope
*   The `Radio` structure requires specific function pointer layout
*   Channel masks are region-specific (US915 example uses subband 2)
*   Timing parameters like `appTxDutyCycle` control transmission intervals 