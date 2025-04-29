# DMX-LoRa Project Memory Log

## RadioLib Upgrade (7.x Compatibility)

### Implementation Decisions

#### 2023-10-15: RadioLib 7.x Migration
- Updated the `SX1262` constructor to use a `Module` object instead of direct pin parameters
- Changed the instantiation pattern to:
  ```cpp
  Module* module = new Module(pinCS, pinDIO1, pinReset, pinBusy);
  radio = new SX1262(module);
  ```
- Simplified the `handleEvents()` method as polling-based approach was removed
- Removed deprecated error code `RADIOLIB_ERR_INVALID_ADR_ACK_LIMIT`
- Updated to use the new `LoRaWANEvent_t` structure with `fPort` instead of `port`
- Modified Class C mode handling to align with the current RadioLib API
- Updated `sendData` and `sendReceive` methods to use the new event-based pattern

### Problems Solved

#### Class C Implementation
- **Problem**: Class C implementation in RadioLib 7.x works differently from 6.x
- **Solution**: Rewrote the Class C handling to open RX2 window continuously after uplink
- **Approach**: Used the new sendReceive pattern with event structures

#### Channel Configuration
- **Problem**: Channel mask configuration API changed in RadioLib 7.x
- **Solution**: Removed manual channel configuration as it's now handled automatically when setting the subband during node initialization
- **Details**: The subBand parameter is now passed directly to the LoRaWANNode constructor

### Edge Cases Handled

#### Join Retries
- Implemented exponential backoff for join attempts
- Try different subbands when initial join attempts fail
- Added proper error reporting for different failure scenarios

#### Transmission Failures
- Track consecutive transmission errors and trigger rejoin when threshold reached
- Implemented retry mechanism for confirmed messages

### Approaches Rejected

#### Manual Module Configuration
- **Rejected Approach**: Manually configuring radio parameters (txPower, spreading factor, etc.)
- **Reason**: RadioLib 7.x manages these parameters internally based on regional parameters
- **Better Solution**: Let RadioLib handle regional compliance

#### Polling for Downlinks
- **Rejected Approach**: Continuously polling for new downlink data in the loop
- **Reason**: RadioLib 7.x uses an event-based approach rather than polling
- **Better Solution**: Use the provided event structures in sendReceive method

## DMX Implementation

### Implementation Decisions

#### Task-based DMX Updates
- Created a dedicated FreeRTOS task for DMX updates
- Used mutexes to protect DMX data access from multiple tasks
- Implemented non-blocking pattern updates

### Problems Solved

#### DMX Update Rate
- **Problem**: Slow update rate when trying to handle both LoRa and DMX in the main loop
- **Solution**: Moved DMX handling to a separate high-priority task
- **Outcome**: Achieved stable ~40Hz update rate for smooth lighting transitions

### Edge Cases Handled

#### DMX Initialization Failures
- Added retry logic for DMX initialization
- Implemented fallback patterns when DMX hardware fails

### Approaches Rejected

#### Interrupt-based DMX Updates
- **Rejected Approach**: Using interrupts for DMX timing
- **Reason**: Conflicts with LoRa radio interrupt handling
- **Better Solution**: FreeRTOS task-based approach with appropriate priority 