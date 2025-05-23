# Current Development Bookmark

## Current State
We are implementing LoRaWAN communication using the official Heltec LoRaWAN library for the Heltec LoRa 32 V3 board.

## Active Issues

### 1. Radio Driver Integration
Location: `src/radio_driver.cpp`
- Need to properly integrate with Heltec's SX126x driver
- Key areas to address:
  ```8:8:src/radio_driver.cpp
  #define STDBY_RC        0x01  // Need to use Heltec's RadioStandbyModes_t
  ```
  ```10:10:src/radio_driver.cpp
  #define PACKET_TYPE_LORA 0x01  // Need to use Heltec's RadioPacketTypes_t
  ```
  ```18:18:src/radio_driver.cpp
  #define RADIO_RAMP_40_US 0x06  // Need to use Heltec's RadioRampTimes_t
  ```

### 2. Main Application Issues
Location: `src/main.cpp`
- Missing declarations:
  - `processJsonPayload`
  - `loraDevice`
  - `loraCallbacks`
  - `loraWanInitialized`
- Vector template issues:
  ```393:396:src/main.cpp
  struct PendingMessage {
      String payload; uint8_t port; bool confirmed; uint8_t priority; uint32_t timestamp;
  };
  std::vector<PendingMessage> messageQueue;
  ```

### 3. LoRaWAN Configuration ✓
- LoRaWAN credentials are defined in `include/secrets.h`:
  - `DEVEUI`: "522b03a2a79b1d87"
  - `APPEUI`: "27ae8d0509c0aa88"
  - `APPKEY`: "2d6fb7d54929b790c52e66758678fb2e"
  - `NWKKEY`: Same as APPKEY (LoRaWAN 1.0.x configuration)

## Related Documentation
- [Technical Details](technical.md) - Contains radio configuration specifics
- [Memory Decisions](memory.md) - Documents implementation choices
- [Project Overview](project.md) - Core architecture decisions
- [LoRaWAN Join Troubleshooting](lorawan_join_troubleshooting.md) - Debugging steps

## Next Steps
1. Update radio driver to use Heltec's type definitions:
   - Replace custom enums with Heltec's RadioStandbyModes_t, RadioPacketTypes_t, etc.
   - Use proper type casting when calling Heltec's SX126x functions
   - Ensure compatibility with Heltec's radio driver

2. Address main.cpp issues:
   - Add missing declarations
   - Fix vector template usage
   - Implement proper LoRaWAN initialization using Heltec's example code

3. Implement proper error handling:
   - Add retry mechanism with exponential backoff for join failures
   - Add detailed logging of radio parameters
   - Implement watchdog reset for persistent join failures

## Build Configuration
Current build flags and configuration can be found in:
```1:55:platformio.ini
[env:heltec_wifi_lora_32_V3]
// ... build configuration ...
```

## Notes
- Using official Heltec LoRaWAN library for optimal hardware compatibility
- Need to follow Heltec's initialization sequence and state machine model
- Consider implementing additional debug output for troubleshooting 