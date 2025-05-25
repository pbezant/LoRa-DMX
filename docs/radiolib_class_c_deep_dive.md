# RadioLib Class C Deep Dive for LoRaWANNode

## Objective
Investigate the RadioLib library (specifically `SX1262.h/.cpp`, `Radio.h/.cpp`, and `protocols/LoRaWAN/LoRaWAN.h/.cpp`) to determine how to implement true, event-driven Class C downlink reception with the `LoRaWANNode` class. The goal is to achieve unsolicited downlink reception without relying on periodic uplinks (polling `sendReceive`).

This document will log findings, relevant code snippets, and potential strategies for extending or patching `LoRaWANNode` or creating a wrapper/derived class to support this functionality.

## Key Investigation Areas

1.  **RadioLib Core Receive Mechanisms (SX1262 & Generic Radio):
    *   How is continuous receive mode initiated on the SX1262 (e.g., `startReceive()` with specific parameters)?
    *   How are radio events (e.g., packet received, CRC error, header valid) signaled (DIO pins, status registers)?
    *   How does RadioLib handle these events internally (interrupt service routines, polling mechanisms)?
    *   What methods are available to read received packet data, RSSI, SNR, etc., at the raw radio level?

2.  **LoRaWANNode Interaction with RadioLib:
    *   How does `LoRaWANNode::sendReceive()` utilize the underlying radio's send and receive capabilities?
    *   How does `LoRaWANNode::activateOTAA()` manage the radio state during and after join?
    *   Are there any existing (perhaps private or protected) methods within `LoRaWANNode` that hint at asynchronous event handling or a different receive path?

3.  **LoRaWAN MAC Layer Processing in RadioLib:
    *   Once a packet is physically received by the radio, how does `LoRaWANNode` (or its internal helpers) perform MAC layer processing (decryption, MIC check, frame counter validation, port handling)?
    *   Can these MAC processing steps be invoked independently of a `sendReceive()` call if we can get a raw packet from a continuous receive mode?

4.  **Potential Strategies for True Class C with LoRaWANNode:
    *   **Strategy A: External Continuous Receive + Internal MAC Processing**
        *   Use low-level RadioLib calls to put the SX1262 into continuous receive after a successful LoRaWAN join.
        *   On radio interrupt/event indicating a packet, read the raw packet.
        *   Find a way to feed this raw packet into the `LoRaWANNode`'s existing MAC processing logic to get a validated and decrypted LoRaWAN downlink payload.
        *   Trigger a user-defined callback with this payload.
    *   **Strategy B: Extending/Modifying LoRaWANNode**
        *   Identify if `LoRaWANNode` can be subclassed and have its receive logic overridden or extended.
        *   Consider modifications to `LoRaWANNode` itself to add a `startContinuousReceive(callback)` method that manages the radio state and internal processing for Class C.
    *   **Strategy C: Direct RadioLib LoRaWAN Stack Usage (If LoRaWANNode is too restrictive)**
        *   If `LoRaWANNode` proves too difficult to adapt, explore using the LoRaWAN protocol implementation within RadioLib more directly, though this would lose the convenience of `LoRaWAN_ESP32` for provisioning if not carefully integrated.

## Log & Findings

*(This section will be populated as research progresses)*

---
Date: 2024-07-29
Focus: Initial review of `RadioLib/src/modules/SX126x/SX126x.h` for core receive mechanisms.

Findings:

*   **Continuous Receive Initiation:** The method `startReceive(uint32_t timeout, RadioLibIrqFlags_t irqFlags, RadioLibIrqFlags_t irqMask, size_t len)` (line ~716) appears to be the primary function to put the radio into receive mode.
    *   Setting `timeout` to `RADIOLIB_SX126X_RX_TIMEOUT_INF` (defined as `0xFFFFFF` in constants related to `RADIOLIB_SX126X_CMD_SET_RX`) enables continuous receive mode. This is key for Class C.
*   **Interrupt Handling (DIO1):**
    *   `setDio1Action(void (*func)(void))` (line ~637): Attaches a user-defined ISR to the DIO1 pin.
    *   This ISR will be triggered when radio events (configured via `irqFlags` in `startReceive`) occur.
*   **Reading Received Data:**
    *   `readData(uint8_t* data, size_t len)` (line ~756): Called after an `RxDone` interrupt to retrieve the packet from the radio's buffer.
    *   `getPacketLength(bool update = true)` (line ~1008): Used to determine the length of the received packet.
*   **IRQ Management:**
    *   `getIrqStatus()` or `getIrqFlags()` (line ~1050, needs clarification on which is preferred for SX126x): Called within the ISR to determine the specific radio event(s) that triggered the interrupt (e.g., `RxDone`, `RxTimeout`, `CrcError`).
    *   `clearIrqStatus(uint16_t clearIrqParams)` (line ~1246): **Crucial.** Must be called within the ISR after processing an event to clear the radio's IRQ flags and allow further interrupts.
    *   `setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, ...)` (line ~1245): Configures which internal SX126x IRQ sources are routed to the DIO pins. `startReceive` likely uses this to set up DIO1 for `RxDone`, `PreambleDetected`, etc.
*   **Relevant IRQ Flags (Constants to be confirmed, likely `RADIOLIB_SX126X_IRQ_...`):
    *   `RxDone`: Packet successfully received with good CRC.
    *   `PreambleDetected`: Preamble detected (can be used for early wake-up or activity detection).
    *   `RxTimeout`: Configured receive timeout occurred before packet reception.
    *   `CrcError`: Packet received, but CRC check failed.

Relevant Code Snippets (`SX126x.h`):
```cpp
// To start continuous receive (conceptual)
// radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF, RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED, ...);

// Inside DIO1 ISR
// void dio1_isr() {
//   uint16_t irqFlags = radio.getIrqStatus(); // or getIrqFlags()
//   radio.clearIrqStatus(irqFlags);
//   if(irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
//     size_t len = radio.getPacketLength();
//     uint8_t buffer[len];
//     radio.readData(buffer, len);
//     // Process buffer (this is where LoRaWAN MAC processing needs to happen)
//   }
// }
```

Potential Next Steps:
1.  Examine `SX126x.cpp` to see the SPI command sequences for `startReceive` (especially with `RX_TIMEOUT_INF`) and `setDioIrqParams`.
2.  Investigate how `LoRaWANNode` uses `startReceive` for its RX1/RX2 windows to understand its expectations of radio configuration.
3.  Determine the exact names and values for `RADIOLIB_SX126X_IRQ_...` flags.
4.  Start formulating how to pass raw received packets to the LoRaWAN MAC processing layers.

---
Date: 2024-07-29
Focus: Bridging low-level SX126x receive to `LoRaWANNode` MAC processing.

Key LoRaWAN.cpp functions:
*   `LoRaWANNode::sendReceive(...)`: Main public API for uplink/downlink. Calls `receiveCommon`.
*   `LoRaWANNode::receiveCommon(...)`: Manages RX1/RX2 window timing and radio setup for these windows. Uses a static ISR (`LoRaWANNodeOnDownlinkAction`) to set a flag (`downlinkAction`) if any radio IRQ occurs during a window. It *does not* read or parse the packet itself.
*   `LoRaWANNode::parseDownlink(uint8_t* data, size_t* len, LoRaWANEvent_t* event)`: This is the core MAC processing function.
    *   It calls `this->phyLayer->getPacketLength()` to get the size of the packet in the radio FIFO.
    *   It calls `this->phyLayer->readData(...)` to read the raw bytes from the radio FIFO.
    *   It then performs all LoRaWAN MAC operations: DevAddr check, FCnt validation, MIC check, decryption (NwkSKey/AppSKey), and extracts FPort, FOpts, and application payload.
    *   Crucially, `parseDownlink` appears self-contained and expects the radio to have a packet ready in its FIFO. It does not depend on `receiveCommon`'s timing logic directly for its parsing actions.

This confirms that if we can put the radio into continuous receive mode and an interrupt signals a packet, we should be able to call `node->parseDownlink()` to process it.

## Grand Plan for True Class C Implementation

1.  **Initial LoRaWAN Join:**
    *   Use `node->activateOTAA()` as normal to join the network. This establishes session keys, DevAddr, FCnts, and RX2 settings within the `node` object.

2.  **Switch to Continuous Class C Receive Mode (e.g., in a `lorawan_helper_enable_class_c_receive()` function):**
    *   Retrieve the `SX1262` radio object (global `radio` from `heltec_unofficial`).
    *   Get RX2 channel parameters (`node->channels[RADIOLIB_LORAWAN_DIR_RX2]`) and convert data rate to SF/BW/CR.
    *   Configure the `radio` directly with these RX2 LoRa parameters (frequency, SF, BW, CR, LoRaWAN sync word, preamble length).
    *   Set a custom DIO1 ISR: `radio->setDio1Action(our_custom_class_c_isr);`
    *   Start continuous receive on the radio: `radio->startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF, RADIOLIB_SX126X_IRQ_RX_DONE, RADIOLIB_SX126X_IRQ_RX_DONE);`
    *   Set a state flag, e.g., `is_class_c_mode_active = true;`

3.  **Custom ISR (`our_custom_class_c_isr()`):**
    *   `irqFlags = radio.getIrqFlags();`
    *   `radio.clearIrqStatus(irqFlags);`
    *   If `(irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE)`:
        *   Set a global flag: `pending_lorawan_packet_for_processing = true;`
        *   Do NOT read the packet from the radio here.

4.  **Main Loop (`loop()` in `main.cpp`):**
    *   If `pending_lorawan_packet_for_processing` is true:
        *   `pending_lorawan_packet_for_processing = false;`
        *   Call a processing function, e.g., `lorawan_helper_process_class_c_downlink()`.
        *   **Inside `lorawan_helper_process_class_c_downlink()`:**
            *   Declare buffers for `app_payload`, `app_payload_len`, `event_data`.
            *   Call `int16_t parse_status = node->parseDownlink(app_payload, &app_payload_len, &event_data);`
            *   If `parse_status == RADIOLIB_ERR_NONE` and `app_payload_len > 0`, pass the payload to the user's registered downlink callback.
            *   Handle any errors from `parse_status`.
            *   **Crucially, re-arm continuous receive:** `radio->startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF, RADIOLIB_SX126X_IRQ_RX_DONE, RADIOLIB_SX126X_IRQ_RX_DONE);` (if `is_class_c_mode_active` is still true). This is because `parseDownlink` (via `phyLayer->readData`) will have cleared the radio's FIFO and likely taken it out of RX mode.

This approach leverages `LoRaWANNode` for MAC complexities while managing the radio directly for continuous Class C reception. 