# Technical Specifications and Patterns

## LoRaWAN Stack (2024)
- **Library:** [ropg/heltec_esp32_lora_v3](https://github.com/ropg/heltec_esp32_lora_v3)
- **LoRaWAN Layer:** [LoRaWAN_ESP32](https://github.com/ropg/LoRaWAN_ESP32)
- **Radio Layer:** [RadioLib](https://github.com/jgromes/RadioLib) v7.1.2
- **Region:** US915
- **Join Mode:** OTAA (credentials in `secrets.h`)
- **Class:** C (continuous receive)
- **Downlink Format:** JSON (see README.md)
- **Uplink:** Periodic status/heartbeat
- **DMX Output:** Full DMX logic, including patterns and direct channel control

## Rationale for Library Choice
- The official Heltec library is deprecated for this project due to upstream build issues and unreliable Class C support for V3 boards.
- The new stack is actively maintained, modern, and easier to debug/extend.
- RadioLib provides robust LoRaWAN Class C support and flexibility.

## Key Technical Decisions
- **Credentials:** Managed in `secrets.h` as hex strings, converted to byte arrays at runtime.
- **LoRaWAN Join:** OTAA only, using US915 region.
- **Downlink Handling (True Class C):** All downlinks are expected as JSON. True asynchronous Class C is achieved by:
    - Directly managing the `SX1262` radio for continuous receive on RX2 parameters.
    - Using a custom ISR on DIO1 for `RX_DONE` to flag incoming packets.
    - `LoRaWANHelper::loop()` processes these flags, reads directly from the radio, then uses `RadioLib::LoRaWANNode::parseDownlink()` and `getDownlinkData()`.
    - The radio is immediately re-armed for continuous receive after an uplink and after processing a received packet.
- **RadioLib v7.x API Migration:** The `LoRaWANHelper.cpp` has been significantly refactored to align with the breaking changes in RadioLib v7.1.2, particularly for node initialization, credential passing (OTAA), and uplink mechanisms.
- **DMX Implementation:** Complete rewrite of DMXHelper.cpp to use esp_dmx library directly:
    - Configured with appropriate pins for the Heltec hardware (TX: GPIO17, RX: GPIO16, ENABLE: GPIO21).
    - Uses a buffer-based approach with DMX_PACKET_SIZE array.
    - DMX functions like dmx_driver_install(), dmx_set_pin(), dmx_write(), and dmx_send_num() for direct control.
    - Handles DMX patterns (color fade, rainbow, strobe, etc.) with the appropriate timing and buffer management.
    - Robust JSON command parsing with ArduinoJson for dynamic control.
- **Periodic Uplinks:** Added to main.cpp with configurable interval (default: 5 minutes).
- **Uplink Format:** Simple JSON status message: `{"status":"alive"}`, extensible for future needs.
- **DMX Logic:** Follows the structure and command formats in README.md, including support for patterns and direct channel control.
- **Error Handling:** Robust error handling for join, downlink, and DMX operations.

## Current Technical Challenges
- **LoRaWANHelper Compilation Issues:** The LoRaWANHelper.cpp file is experiencing compilation errors with RadioLib v7.1.2:
    - Incomplete type errors for RadioLib::LoRaWANNode and RadioLib::LoRaWANBand.
    - Missing LORAWAN_DEVICE_EUI and related constants from secrets.h.
    - Issues with forward declarations and include paths.
    - Work in progress to resolve these issues (see LORA-006 in tasks.md).

## Patterns
- **Separation of Concerns:** LoRaWAN, DMX, and JSON logic are modular and decoupled.
- **Runtime Credential Conversion:** No hardcoded byte arrays; all credentials are converted at runtime from hex strings.
- **Documentation:** All changes and rationale are documented in the docs/ folder.
- **Direct Hardware Library Usage:** For DMX, moved from a wrapper approach to direct esp_dmx library usage for better maintainability.

## Deprecated/Abandoned Approaches
- The official Heltec ESP32 Dev-Boards library is no longer used due to upstream issues.
- DmxController.h wrapper has been removed in favor of direct esp_dmx library usage.

## Next Steps
- Resolve LoRaWANHelper compilation issues with RadioLib v7.1.2
- Complete the testing phase
- Keep documentation up to date

# Technical Specifications and Established Patterns

This document provides technical details about the LoRa-DMX project, including its technology stack, patterns, hardware specifications, and implementation notes.

## Technology Stack

### Hardware
- **Board:** Heltec WiFi LoRa 32 V3.2
- **Processor:** ESP32-S3
- **LoRa Module:** SX1262 on-board chip
- **DMX Interface:** Custom DMX transceiver circuit (output only)

### Software and Libraries
- **Framework:** Arduino for ESP32
- **Build System:** PlatformIO
- **LoRaWAN Stack:** RadioLib v7.1.2 + nopnop2002's SX1262 driver
- **DMX Library:** esp_dmx
- **JSON Parsing:** ArduinoJson
- **LoRaWAN Network Server:** Chirpstack/TTN

## Library Versions and Dependencies

- **RadioLib:** v7.1.2
  - Used for LoRaWAN protocol handling
  - Required for uplink/downlink support, OTAA joins
  - Breaking changes in v7.x from previous versions required refactoring

- **nopnop2002 SX1262 Driver:**
  - Used for direct hardware control of the SX1262 radio chip
  - Provides better bandwidth compatibility and hardware access
  - Integrated with RadioLib's LoRaWAN stack for a hybrid solution

- **ArduinoJson:** v6.x
  - Used for parsing JSON commands from LoRaWAN downlinks
  - Formats data for uplinks

- **esp_dmx:**
  - Used for controlling DMX fixtures
  - Handles DMX protocol timing and electrical specifications
  - Replaces the deprecated DmxController library

## Architecture and Implementation Patterns

### Overall Architecture
The system follows a modular design with clear separation of concerns:

1. **Main Application (src/main.cpp):**
   - Initializes hardware components (LoRa, DMX)
   - Sets up LoRaWAN connection
   - Handles downlink command processing
   - Manages periodic uplinks
   - Coordinates between LoRaWAN and DMX subsystems

2. **LoRaWAN Helper (lib/LoRaWANHelper):**
   - Abstracts LoRaWAN connectivity details
   - Handles device EUI and key management
   - Manages OTAA join process
   - Provides send/receive functions
   - Implements Class C continuous reception

3. **DMX Helper (lib/DMXHelper):**
   - Abstracts DMX protocol details
   - Manages DMX universe and fixtures
   - Handles DMX data buffer and transmission
   - Provides fixture control functions
   - Implements lighting patterns and effects

### LoRaWAN Implementation Architecture

#### Current Approach (To Be Replaced)
```
[RadioLib] <-- [LoRaWANHelper] <-- [main.cpp]
   ^
   |
[SX1262 Hardware]
```

#### New Hybrid Approach with nopnop2002 Driver (Adopted June 2024)
```
            [RadioLib LoRaWAN Stack]
                     ^
                     |
[nopnop2002 SX1262 Driver] <-- [Integration Layer] <-- [LoRaWANHelper] <-- [main.cpp]
            ^
            |
    [SX1262 Hardware]
```

This hybrid approach, which we have officially adopted after extensive evaluation:
1. Uses nopnop2002's driver for direct hardware access to the SX1262 chip
2. Uses RadioLib for LoRaWAN protocol handling
3. Creates a clean integration layer that maintains API compatibility
4. Implements proper interrupt handling for true Class C operation
5. Provides bandwidth detection and fallback mechanisms for hardware compatibility

Key advantages of this approach:
- Better hardware compatibility with our specific SX1262 chip implementation
- More reliable interrupt-driven Class C downlink reception
- Direct control over radio parameters for continuous reception
- Cleaner separation of hardware and protocol concerns
- Proven success with the same hardware in other projects

### Class C Implementation
The system is designed to operate as a LoRaWAN Class C device, which means:

1. **Continuous Receive:** The device listens continuously for downlinks when not transmitting
2. **Interrupt-Driven Reception:** Uses hardware interrupts on DIO1 pin to detect incoming packets
3. **Low Latency:** Processes downlink commands immediately upon reception
4. **High Power Consumption:** Requires continuous power (non-battery operation)

The Class C implementation with nopnop2002's driver will follow these steps:
1. Initialize the SX1262 radio using nopnop2002's driver
2. Configure RadioLib's LoRaWAN stack with the initialized radio
3. Perform OTAA join using device credentials
4. Configure continuous reception using nopnop2002's direct radio functions
5. Set up an interrupt handler for the DIO1 pin
6. Process packets in the interrupt context or flag them for main loop processing
7. Re-enable continuous reception after each uplink and received packet

### DMX Implementation
The DMX subsystem:

1. Initializes the DMX port and pins using esp_dmx
2. Maintains a buffer with the current state of all DMX channels (512 max)
3. Updates fixtures by writing to the appropriate channels in the buffer
4. Transmits the entire DMX universe at regular intervals
5. Implements patterns and effects by modifying fixture channel values over time

### JSON Command Format
Commands received via LoRaWAN downlinks use JSON format:

```json
{
  "cmd": "set_fixture",
  "fixture": 1,
  "address": 1,
  "channels": [255, 0, 0, 0]
}
```

Supported commands:
- `set_fixture`: Set specific fixture channels
- `set_dmx`: Set raw DMX channel values
- `run_pattern`: Activate a pre-defined pattern
- `stop_pattern`: Stop the current pattern

## Pin Assignments

### LoRa (SX1262) Pins
- NSS: GPIO_NUM_8
- DIO1: GPIO_NUM_14
- NRST: GPIO_NUM_12
- BUSY: GPIO_NUM_13

### DMX Pins
- DMX_TX: GPIO_NUM_17
- DMX_RX: GPIO_NUM_18 (not used in this application)
- DMX_DE: GPIO_NUM_16 (data enable)

## File Structure

```
LoRa-DMX/
├── include/
│   └── secrets.h            # LoRaWAN credentials (not in repo)
├── lib/
│   ├── DMXHelper/           # DMX control library
│   │   ├── DMXHelper.cpp
│   │   └── DMXHelper.h
│   ├── LEDHelper/           # LED indicator library
│   │   ├── LEDHelper.cpp
│   │   └── LEDHelper.h
│   └── LoRaWANHelper/       # LoRaWAN connectivity library
│       ├── LoRaWANHelper.cpp
│       └── LoRaWANHelper.h
├── src/
│   └── main.cpp             # Main application
└── platformio.ini           # PlatformIO configuration
```

## Implementation Patterns

### Callback-Based Architecture
The project uses callbacks to handle asynchronous events:

```cpp
// Register a callback for LoRaWAN downlinks
lorawan_helper_init(radio, &Serial, 300000, process_downlink_callback);

// Downlink callback implementation
void process_downlink_callback(uint8_t* buffer, uint8_t length, int16_t rssi, float snr) {
  // Process the received data
}
```

### Hardware Abstraction
Each subsystem is abstracted behind a helper library with a clean interface:

```cpp
// LoRaWAN abstraction
lorawan_helper_init(...);
lorawan_helper_join();
lorawan_helper_send(...);
lorawan_helper_loop();

// DMX abstraction
dmx_helper_init(...);
dmx_helper_set_fixture_channels(...);
dmx_helper_run_pattern(...);
```

## Planned Implementation for nopnop2002 Driver + RadioLib Integration

### Integration Layer Design

The integration between nopnop2002's SX1262 driver and RadioLib's LoRaWAN stack will follow this architecture:

```cpp
// Radio hardware layer (nopnop2002 driver)
class SX1262Radio {
private:
  // nopnop2002 driver instance
  SX1262* radio;
  // Hardware pins and configuration
  
public:
  // Initialize radio hardware
  bool begin();
  // Low-level radio functions (used by RadioLib)
  int16_t transmit(uint8_t* data, size_t len);
  int16_t receive(uint8_t* data, size_t len);
  int16_t startReceive();
  // Interrupt handling
  void setDio1Action(void (*func)(void));
  // Radio configuration
  int16_t setBandwidth(float bw);
  int16_t setFrequency(float freq);
  // Other hardware-specific functions
};

// Protocol layer (RadioLib LoRaWAN)
class LoRaWANProtocol {
private:
  // RadioLib LoRaWANNode instance
  RadioLib::LoRaWANNode* node;
  // SX1262Radio instance for hardware control
  SX1262Radio* radio;
  
public:
  // Initialize LoRaWAN stack with our hardware abstraction
  bool begin(SX1262Radio* radio);
  // LoRaWAN protocol functions
  bool joinOTAA(uint64_t joinEUI, uint64_t devEUI, uint8_t* nwkKey, uint8_t* appKey);
  bool send(uint8_t* data, size_t len, bool confirmed = false);
  // Class C management
  bool enableClassC();
  void processDownlink();
};

// Public API (LoRaWANHelper)
// This maintains compatibility with existing application code
```

### Class C Implementation Details

The true Class C implementation with nopnop2002's driver will include:

1. **Hardware Interrupt Handling:**
   ```cpp
   // Set up DIO1 interrupt
   pinMode(DIO1, INPUT);
   attachInterrupt(digitalPinToInterrupt(DIO1), dio1_interrupt_handler, RISING);
   
   // Interrupt handler
   void dio1_interrupt_handler(void) {
     packet_received = true;
   }
   ```

2. **Continuous Reception Configuration:**
    ```cpp
   void enable_class_c_receive() {
     // Configure radio for RX2 parameters
     radio->setFrequency(RX2_FREQUENCY);
     radio->setSpreadingFactor(RX2_SF);
     radio->setBandwidth(RX2_BANDWIDTH);
     
     // Start continuous reception
     radio->startReceive();
   }
   ```

3. **Downlink Processing:**
   ```cpp
   void loop() {
     if (packet_received) {
       packet_received = false;
       
       // Read packet data
       uint8_t data[256];
       size_t len = radio->readData(data, 256);
       
       // Process with RadioLib LoRaWAN stack
       node->processDownlink(data, len);
       
       // Get application data
       uint8_t appData[256];
       size_t appLen = node->getAppData(appData, 256);
       
       // Call user callback if data available
       if (appLen > 0 && downlink_callback) {
         downlink_callback(appData, appLen, radio->getRSSI(), radio->getSNR());
       }
       
       // Re-enable continuous reception
       enable_class_c_receive();
     }
   }
   ```

### Bandwidth Compatibility

The implementation will include bandwidth detection and fallback:

```cpp
bool test_bandwidth(float bw) {
  int16_t state = radio->setBandwidth(bw);
  return (state == RADIOLIB_ERR_NONE);
}

bool initialize_radio() {
  // Try different bandwidths in order of preference
  float bandwidths[] = {125.0, 250.0, 500.0, 62.5, 31.25, 15.625, 7.8};
  for (int i = 0; i < 7; i++) {
    if (test_bandwidth(bandwidths[i])) {
      Serial.printf("Bandwidth %.3f kHz is supported\n", bandwidths[i]);
      supported_bandwidths.push_back(bandwidths[i]);
    }
  }
  
  // Configure with best supported bandwidth
  if (supported_bandwidths.size() > 0) {
    radio->setBandwidth(supported_bandwidths[0]);
    return true;
  }
  
  return false;
}
```

### Bandwidth Testing Results (June 2024)

Based on our comprehensive testing using the custom bandwidth testing tool (`examples/bandwidth_test/`), we have determined:

1. **Supported Bandwidths on Our SX1262**:
   - 7.8 kHz
   - 15.6 kHz
   - 31.25 kHz
   - 62.5 kHz
   - 125.0 kHz (critical for standard US915 operation)
   - 250.0 kHz
   - 500.0 kHz

2. **Unsupported Bandwidths**:
   - 10.4 kHz
   - 20.8 kHz
   - 41.7 kHz

3. **Implementation Implications**:
   - We will only attempt to use the supported bandwidths listed above
   - For RX2 continuous reception in Class C, we'll use 125.0 kHz as it's both supported and standard
   - Our bandwidth fallback mechanism will be refined to only try these specific values

See `bandwidth_test_results.md` for detailed testing methodology and complete results.

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