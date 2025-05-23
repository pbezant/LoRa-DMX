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
*   **Credential Management:** Store LoRaWAN credentials (DevEUI, AppEUI, AppKey) in a separate `secrets.h` file (which should be in `.gitignore`) and include it in your main application file. Convert string-based credentials from `secrets.h` to byte arrays as needed by the LoRaWAN library.
*   **Input Sanitization:** (Assumed to be handled by `ArduinoJson` during parsing, but complex or malformed JSON could still be a concern. Further inspection of parsing logic in source code needed.)

## Performance Guidelines

*   **Real-time DMX:** DMX requires consistent timing. `esp_dmx` library likely handles this at a low level.
*   **LoRaWAN Duty Cycle:** Adherence to LoRaWAN regional duty cycle limitations is critical (managed by `RadioLib` and TTN).
*   **JSON Parsing:** `ArduinoJson` is generally efficient, but very large/complex JSON payloads could impact performance or memory on the microcontroller. Payload size is also limited by LoRaWAN constraints.
*   **Power Consumption:** For battery-powered applications (if any), deep sleep and minimizing radio transmission time are important. (Heltec LoRa 32 V3 has Wi-Fi/Bluetooth which should be disabled if not used to save power).

## Using the LoRaWrapper for LoRaWAN Communication

The `LoRaWrapper` library provides a standardized interface for LoRaWAN communication, abstracting the specifics of the underlying LoRaWAN modem/library. The initial concrete implementation is `HeltecLoRaWan` for Heltec ESP32 based boards using their official `Heltec_ESP32` library.

### Integration Steps:

1.  **Include Headers:**
    In your `main.cpp` (or equivalent application file), include the necessary headers:
    ```cpp
    #include "LoRaWrapper/ILoRaWanDevice.h"
    #include "LoRaWrapper/HeltecLoRaWan.h" // Or other future implementations
    #include <Arduino.h> 
    // Other project-specific includes
    ```

2.  **Implement `ILoRaWanCallbacks`:**
    Create a class in your application that inherits from `ILoRaWanCallbacks` to handle LoRaWAN events.
    ```cpp
    class MyAppLoRaCallbacks : public ILoRaWanCallbacks {
    public:
        void onJoined() override {
            Serial.println("MyApp: LoRaWAN Joined!");
            // Application logic after join
        }

        void onJoinFailed() override {
            Serial.println("MyApp: LoRaWAN Join Failed.");
            // Application logic for join failure
        }

        void onDataReceived(const uint8_t* data, uint8_t len, uint8_t port, int16_t rssi, int8_t snr) override {
            Serial.printf("MyApp: Data Received - Port: %d, RSSI: %d, SNR: %d, Len: %d\n", port, rssi, snr, len);
            // Process received application data
        }

        void onSendConfirmed(bool success) override {
            // Note: For HeltecLoRaWan (non-intrusive), 'success' for confirmed messages means the TX cycle completed,
            // not necessarily that an ACK was received from the server.
            Serial.printf("MyApp: LoRaWAN Send Confirmed/Completed: %s\n", success ? "true" : "false");
        }

        void onMacCommand(uint8_t cmd, uint8_t* payload, uint8_t len) override {
            Serial.printf("MyApp: MAC Command Received: CMD=0x%02X, Length=%d\n", cmd, len);
        }
    };
    ```

3.  **Instantiate and Configure in `setup()`:**
    ```cpp
    MyAppLoRaCallbacks myCallbacks;
    ILoRaWanDevice* loraDevice;

    // Define your LoRaWAN credentials (DevEUI, AppEUI, AppKey)
    // const uint8_t DEV_EUI[] = { ... };
    // const uint8_t APP_EUI[] = { ... };
    // const uint8_t APP_KEY[] = { ... };
    // Credentials are now typically managed in a separate secrets.h file
    #include "secrets.h" // Ensure this file defines DEVEUI_BYTES, APPEUI_BYTES, APPKEY_BYTES

    // Helper function to convert hex string from secrets.h to byte array
    // Ensure this function is defined or adapt as necessary.
    void hexStringToByteArray(const char* hexString, uint8_t* byteArray, int byteLen) {
        for (int i = 0; i < byteLen; i++) {
            char byteChars[3] = {hexString[i*2], hexString[i*2+1], 0};
            byteArray[i] = strtol(byteChars, nullptr, 16);
        }
    }

    uint8_t actual_dev_eui[8];
    uint8_t actual_app_eui[8];
    uint8_t actual_app_key[16];

    void setup() {
        Serial.begin(115200);
        // ... other serial setup ...

        // Convert hex string credentials from secrets.h to byte arrays
        // Assumes DEVEUI, APPEUI, APPKEY are defined as strings in secrets.h
        hexStringToByteArray(DEVEUI, actual_dev_eui, 8);
        hexStringToByteArray(APPEUI, actual_app_eui, 8);
        hexStringToByteArray(APPKEY, actual_app_key, 16);

        // CRITICAL: Initialize board/MCU first (specific to Heltec boards)
        Mcu.begin(); // Or relevant Mcu.begin(BOARD_TYPE, CLOCK_TYPE);

        loraDevice = new HeltecLoRaWan(); // Instantiate the concrete implementation

        // Set credentials and parameters
        loraDevice->setDevEui(actual_dev_eui);
        loraDevice->setAppEui(actual_app_eui);
        loraDevice->setAppKey(actual_app_key);
        loraDevice->setActivationType(true); // true for OTAA
        loraDevice->setAdr(true); // Enable Adaptive Data Rate
        
        // Example: Initialize for Class C, US915 region
        // Adjust DeviceClass_t (CLASS_A, CLASS_C) and LoRaMacRegion_t as needed.
        if (loraDevice->init(CLASS_C, LORAMAC_REGION_US915, &myCallbacks)) {
            Serial.println("LoRaWAN device initialized. Attempting join...");
            loraDevice->join();
        } else {
            Serial.println("FATAL: LoRaWAN device initialization failed.");
            // Handle critical failure
            while(1);
        }
    }
    ```

4.  **Process in `loop()`:**
    The `loraDevice->process()` method **must** be called continuously in the main application loop.
    ```cpp
    void loop() {
        loraDevice->process(); // Drives the LoRaWAN stack and wrapper logic

        // Application logic: e.g., send data periodically
        if (loraDevice->isJoined()) {
            // Example: send unconfirmed data on port 1
            // uint8_t myPayload[] = "Hello";
            // loraDevice->send(myPayload, sizeof(myPayload), 1, false);
        }

        // Other application tasks
    }
    ```

### Key Considerations:
*   **Board Initialization:** The `Mcu.begin()` call (or its equivalent for the specific board) is vital for Heltec devices and must precede `loraDevice->init()`.
*   **LoRaWAN Region:** Ensure the correct `LoRaMacRegion_t` is used during initialization.
*   **`process()` Call:** The `loraDevice->process()` in the main `loop()` is mandatory for the wrapper and underlying stack to function.
*   **Class C Power:** Be mindful of power consumption when using `CLASS_C`.
*   **Error Handling:** Implement robust error handling for initialization and join failures.
*   **`platformio.ini`:** Configure the correct board and any necessary build flags (e.g., for LoRaWAN region if required by the Heltec library). 