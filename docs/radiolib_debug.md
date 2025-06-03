# LoRaWAN Troubleshooting Log

## Initial Errors

The project was experiencing multiple errors:

1. **Multiple definition errors** during linking:
   - Multiple definitions of `heltec_led`, `display`, `radio`, etc.
   - Example: `multiple definition of 'heltec_led(int)'`

2. **LoRaWAN join failure** with error code -28 (RADIOLIB_ERR_INVALID_BANDWIDTH)

## Steps Taken

### 1. Fixed Multiple Definition Errors

The root cause was that `heltec_unofficial.h` was being included in both `src/main.cpp` and `lib/LoRaWANHelper/LoRaWANHelper.cpp`. This header defined global objects like `radio` and `display`, causing the linker to see multiple definitions.

**Solution:**
- Removed `#include <heltec_unofficial.h>` from `LoRaWANHelper.cpp`
- Added explicit `#define DIO1 GPIO_NUM_14` to maintain access to required constants
- Added `#include <Arduino.h>` to ensure ESP32 pin definitions were available
- Removed duplicate function definitions for `internal_downlink_cb` and `lorawan_custom_class_c_isr`

### 2. Addressed Bandwidth Error

RadioLib was reporting error -28 (RADIOLIB_ERR_INVALID_BANDWIDTH) during the LoRaWAN join attempt.

**Solution:**
- Added bandwidth detection and fallback in `lorawan_helper_init()`
- Tested different bandwidths (125 kHz, 250 kHz, 500 kHz, 62.5 kHz, 31.25 kHz, 15.625 kHz, 7.8 kHz)
- Updated error reporting to provide more detailed information

### 3. Improved Join Handling

The device was reporting successful joins but returning a device address of 00000000, which indicates a failed join.

**Solution:**
- Added validation to check for non-zero device address after join
- Added retry mechanism with exponential backoff
- Improved debug output to show detailed join parameters

### 4. Class C Implementation Attempts

Attempted to implement true Class C continuous receive using direct SX1262 radio management:

**Approach:**
- Directly configured the SX1262 radio for continuous reception on RX2 parameters
- Set up a custom ISR on DIO1 for `RX_DONE` events
- Added code to immediately re-arm the radio after every uplink and received packet

**Results:**
- Device successfully transmitted uplinks but didn't properly receive downlinks
- The radio appeared to be in receive mode but wasn't properly processing packets
- Suspected issues with bandwidth compatibility or interrupt handling

### 5. Research on Alternative Approaches

Investigated alternative approaches for implementing true Class C on the Heltec board:

1. **RadioLib-only approach:** Current approach using RadioLib v7.1.2 with direct SX1262 radio management
2. **nopnop2002's SX126x-Arduino library:** ESP-IDF native implementation for SX1262
3. **nopnop2002's SX1262 driver with RadioLib's LoRaWAN stack:** Hybrid approach using nopnop2002's driver for hardware access and RadioLib for LoRaWAN protocol

### 6. Heltec Board Verification

Verified that:
- The gateway is online and other devices can connect to it
- The registration details for this device are correct
- Basic LoRa (non-LoRaWAN) communication works on the Heltec board
- The SX1262 chip is properly connected (soldered to the board)

## Final Solution: nopnop2002 Driver with RadioLib LoRaWAN Stack

After thorough research and testing, we have decided to implement a hybrid approach using nopnop2002's SX1262 driver with RadioLib's LoRaWAN stack.

### Why This Approach?

1. **Hardware Compatibility:** nopnop2002's driver is specifically designed for the SX1262 chip used in our Heltec board and similar modules.

2. **Bandwidth Detection:** The driver includes better bandwidth detection and fallback mechanisms for hardware compatibility.

3. **Interrupt-Driven Reception:** This approach provides a cleaner implementation of interrupt-driven Class C reception using the DIO1 pin.

4. **Proven Success:** Several GitHub repositories demonstrate this approach working reliably with SX1262-based boards.

5. **Modular Design:** We can separate hardware concerns (handled by nopnop2002's driver) from protocol concerns (handled by RadioLib).

### Implementation Plan

1. **Integration Layer Development:**
   - Create a wrapper class that interfaces between nopnop2002's driver and RadioLib's LoRaWAN stack
   - Implement proper DIO1 interrupt handling for Class C reception
   - Maintain the same API as our current LoRaWANHelper for backward compatibility

2. **Hardware Configuration:**
   - Implement bandwidth detection and fallback
   - Configure the SX1262 radio with appropriate settings for the US915 band
   - Set up proper interrupt handling for DIO1

3. **LoRaWAN Protocol Handling:**
   - Use RadioLib's LoRaWAN stack for OTAA join, message encryption, and MIC validation
   - Implement proper MAC command handling
   - Ensure correct FPort handling for application data

4. **Class C Implementation:**
   - Configure continuous reception on RX2 parameters
   - Process downlinks immediately upon reception
   - Re-enable continuous reception after each uplink and downlink

### GitHub Examples and Resources

We've identified the following resources to help with implementation:

1. **nopnop2002/Arduino-LoRa-Ra01S**
   - Direct driver for SX1262 radio
   - Examples of interrupt-based reception
   - Compatible with LoRa Ra-01S modules (which use the same SX1262 chip)

2. **nopnop2002/esp-idf-sx126x**
   - ESP-IDF native implementation
   - Examples of Class A and Class C LoRaWAN
   - Not Arduino-compatible but provides valuable insights

3. **jgromes/RadioLib**
   - Current LoRaWAN stack we're using
   - Well-documented API for LoRaWAN protocol handling
   - Supports US915 band and OTAA activation

## Planned Next Steps

1. Set up a bandwidth testing tool to identify which bandwidths work with our hardware
2. Fork/clone nopnop2002/Arduino-LoRa-Ra01S repository
3. Create the integration layer between nopnop2002's driver and RadioLib
4. Implement and test basic LoRaWAN functionality (join, send, receive)
5. Implement true Class C continuous reception
6. Refactor existing LoRaWANHelper to use the new implementation
7. Test with real LoRaWAN network and validate Class C operation

See tasks.md for detailed task breakdown and status.

## Key Insights and Learnings

1. **RadioLib's Direct Mode vs. LoRaWAN:** RadioLib provides both direct radio control and a LoRaWAN stack, but integrating them for Class C requires careful implementation.

2. **Bandwidth Compatibility:** Different SX1262 boards support different bandwidths, and it's crucial to test and implement fallback options.

3. **Interrupt-Driven vs. Polling:** True Class C requires interrupt-driven reception for immediate processing of downlinks.

4. **Join vs. Activation Success:** A LoRaWAN join can appear successful but fail to provide a valid device address, indicating a network issue or misconfiguration.

5. **Modular Design:** Separating hardware concerns from protocol concerns leads to more maintainable and flexible code.

## References

- [RadioLib GitHub Repository](https://github.com/jgromes/RadioLib)
- [nopnop2002/Arduino-LoRa-Ra01S](https://github.com/nopnop2002/Arduino-LoRa-Ra01S)
- [nopnop2002/esp-idf-sx126x](https://github.com/nopnop2002/esp-idf-sx126x)
- [SX1262 Datasheet](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1262)
- [LoRaWAN 1.0.4 Specification](https://lora-alliance.org/resource_hub/lorawan-104-specification-package/) 

## Decision Confirmation (June 2024)

After thorough evaluation of our options and analysis of the issues encountered, we have officially decided to implement the hybrid approach using nopnop2002's SX1262 driver with RadioLib's LoRaWAN stack. This decision is based on:

1. **Evidence of Hardware Compatibility Issues**: Our testing revealed that the RadioLib-only approach faces persistent bandwidth compatibility issues with our specific Heltec board hardware.

2. **Proven Success of nopnop2002's Driver**: The nopnop2002 driver has demonstrated success with Ra-01S/Ra-01SH modules that use the same SX1262 chip as our Heltec board.

3. **Need for True Class C Operation**: Our requirement for immediate downlink processing necessitates a reliable interrupt-driven approach, which is better achieved with the hybrid implementation.

4. **Bandwidth Detection Capabilities**: The nopnop2002 driver provides better mechanisms for bandwidth detection and fallback, addressing our INVALID_BANDWIDTH errors.

5. **Cleaner Architecture**: The hybrid approach allows for a cleaner separation of hardware and protocol concerns.

The implementation plan has been updated in tasks.md, and work has already begun on creating the bandwidth testing tool as the first step in our implementation. This approach represents the most promising path to achieving reliable Class C operation with our specific hardware.

Next immediate steps:
1. Complete the bandwidth testing tool to identify compatible bandwidths
2. Create the integration layer between nopnop2002's driver and RadioLib
3. Implement proper interrupt handling for continuous downlink reception
4. Develop and test the hybrid Class C implementation 