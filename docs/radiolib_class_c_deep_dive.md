# RadioLib Class C Deep Dive

This document explores implementing LoRaWAN Class C devices using RadioLib and alternative libraries, with a focus on the Heltec WiFi LoRa 32 v3.2 board with SX1262 radio.

## Background on Class C

LoRaWAN has three device classes:

1. **Class A**: Most power-efficient. Device only listens for downlinks briefly after sending an uplink.
2. **Class B**: Medium power. Device listens periodically based on synchronized beacons.
3. **Class C**: Highest power consumption. Device listens continuously when not transmitting.

Class C devices are ideal for applications that need to receive commands or data from the server at any time, with minimal latency.

## RadioLib's Class C Implementation

### How RadioLib Handles Class C

RadioLib v7.x implements Class C in `LoRaWANNode` by:

1. Setting device class via `setDeviceClass(RADIOLIB_LORAWAN_CLASS_C)`
2. Using continuous receive mode after join completion
3. Handling downlinks through the `sendReceive()` method

The primary challenge is that RadioLib's API doesn't provide a native asynchronous callback mechanism for downlinks in Class C mode. Instead, the device periodically checks for messages.

### RadioLib Class C Limitations

1. **No Asynchronous Event Model**: No direct way to register a callback for downlinks
2. **Polling Required**: Must periodically call `sendReceive()` to check for downlinks
3. **No Interrupt Support**: Doesn't use interrupts for downlink detection in the public API
4. **Limited Documentation**: Few examples showing proper Class C implementation

## Most Promising GitHub Examples

We've identified the following repositories as the most promising for Class C implementation:

### 1. nopnop2002/Arduino-LoRa-Ra01S

**URL**: https://github.com/nopnop2002/Arduino-LoRa-Ra01S  
**Relevance**: ★★★★★

This library is specifically designed for Ra-01S/Ra-01SH modules, which use the same SX1262/1268 chips as the Heltec board. Key features:

- Designed for the exact same radio chip (SX1262)
- Compatible with RadioLib for communication
- Includes examples for Arduino/ESP32
- Has correct pin configurations for Heltec-like boards

**Implementation Approach**:  
The library doesn't directly implement LoRaWAN Class C, but it provides a solid foundation for controlling the SX1262 correctly. We could combine this with the RadioLib LoRaWAN stack to build a Class C device.

```cpp
// Example modification to use with Heltec board
SX126x lora(5,     // NSS
            6,     // RESET
            7,     // BUSY
            8,     // TXEN (may not be needed)
            9      // RXEN (may not be needed)
           );

// Initialize with proper frequency
int16_t ret = lora.begin(915.0,        // frequency in MHz (US915)
                         20,           // tx power in dBm
                         3.3,          // use TCXO voltage if needed
                         true);        // use DCDC converter
```

### 2. GereZoltan/LoRaWAN

**URL**: https://github.com/GereZoltan/LoRaWAN  
**Relevance**: ★★★★☆

A complete MicroPython LoRaWAN implementation for SX1262 that includes Class C functionality:

- Full interrupt-driven Class C implementation
- Uses callback mechanism for downlink processing
- Works with SX1262 chip
- Well-structured, modular code

While in MicroPython, the architecture and approach could be ported to C++ for our project.

**Key Class C Implementation Concepts**:
```python
# From GereZoltan/LoRaWAN
def process_downlink(self, data):
    # Process downlink data when received
    # This is called from an interrupt handler

def enable_class_c(self):
    # Configure continuous receive mode
    self.radio.set_rx_continuous()
    # Set up interrupt handler
    self.radio.on_rx_done(self._handle_rx_done)
```

### 3. nopnop2002/esp-idf-sx126x

**URL**: https://github.com/nopnop2002/esp-idf-sx126x  
**Relevance**: ★★★★☆

ESP-IDF native driver for SX1262/SX1268/LLCC68:

- Native ESP32 implementation
- Well-documented SX1262 configuration
- Includes ping-pong and socket examples
- Handles low-level radio configuration properly

This repository shows how to correctly configure the SX1262 on ESP32, especially regarding bandwidth settings, which could help with our INVALID_BANDWIDTH error.

**Relevant Code Snippet**:
```c
// From nopnop2002/esp-idf-sx126x
// Try different bandwidths to find one that works
float test_bandwidths[] = {125.0, 250.0, 500.0, 62.5, 31.25, 41.7, 20.8};
bool found_valid_bw = false;
float working_bw = 0;

for (int i = 0; i < 7; i++) {
    ESP_LOGI(TAG, "Testing bandwidth %.2f kHz...", test_bandwidths[i]);
    state = sx1262_radio->setBandwidth(test_bandwidths[i]);
    if (state == RADIOLIB_ERR_NONE) {
        found_valid_bw = true;
        working_bw = test_bandwidths[i];
        ESP_LOGI(TAG, "Success! Radio supports %.2f kHz bandwidth.", working_bw);
        break;
    }
}
```

### 4. ropg/heltec_esp32_lora_v3 + LoRaWAN_ESP32

**URL**: https://github.com/ropg/heltec_esp32_lora_v3  
**Relevance**: ★★★★☆

Unofficial library specifically for Heltec ESP32 LoRa v3 boards:

- Targets the exact same Heltec v3 board
- Uses RadioLib for LoRa functionality
- Includes companion LoRaWAN_ESP32 library for provisioning and session state

This library is designed for our exact hardware, though its Class C support relies on the underlying RadioLib implementation.

## Recommended Implementation Approach

Based on our research, here's the recommended approach for implementing a Class C device:

### 1. Try nopnop2002/Arduino-LoRa-Ra01S First

This library is most likely to work with our hardware since it's specifically designed for the same SX1262 chip. Steps to try:

1. Clone the repository
2. Install it as an Arduino library
3. Try the basic examples to verify radio functionality
4. Adapt their initialization code to our project
5. Layer on RadioLib's LoRaWAN stack for Class C functionality

### 2. Custom Class C Implementation

If using RadioLib directly, implement a custom Class C solution:

```cpp
// Class C implementation using RadioLib
void setup_class_c() {
    // Join the network first
    join_lorawan_network();
    
    // Configure for Class C
    lora_node.setDeviceClass(RADIOLIB_LORAWAN_CLASS_C);
    
    // Start a background task for checking downlinks
    xTaskCreate(downlink_task, "downlink_task", 4096, NULL, 5, NULL);
}

void downlink_task(void* pvParameters) {
    uint8_t downlink_buffer[256];
    size_t downlink_len = 0;
    
    while(1) {
        // Check for downlinks periodically
        int state = lora_node.receivePacket(downlink_buffer, &downlink_len);
        if(state == RADIOLIB_ERR_NONE && downlink_len > 0) {
            // Process downlink
            process_downlink(downlink_buffer, downlink_len);
        }
        
        // Short delay to prevent hogging CPU
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
```

### 3. Alternative Frequency/Bandwidth Settings

If the US915 band with standard bandwidth settings doesn't work, try:

1. Using a different bandwidth within the US915 band
2. Testing with the bandwidth detection code from nopnop2002/esp-idf-sx126x
3. Creating a custom band definition with compatible bandwidth settings

```cpp
// Test different bandwidths
float bandwidths[] = {125.0, 250.0, 500.0, 62.5, 31.25, 41.7, 20.8};
for (int i = 0; i < sizeof(bandwidths)/sizeof(float); i++) {
    Serial.printf("Testing bandwidth %.1f kHz... ", bandwidths[i]);
    int state = radio.setBandwidth(bandwidths[i]);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("SUCCESS");
    } else {
        Serial.printf("FAILED (Error: %d)\n", state);
    }
}
```

## Conclusion

Class C implementation is possible with RadioLib and SX1262, but may require custom code to handle the continuous receive mode properly. The nopnop2002/Arduino-LoRa-Ra01S library appears to be the most promising starting point due to its specific support for our hardware.

For bandwidth issues, testing different values and implementing fallback options as shown in nopnop2002/esp-idf-sx126x should help overcome the INVALID_BANDWIDTH error. 