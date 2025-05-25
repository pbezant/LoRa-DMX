# System Architecture (2024)

## Overview
- **Device:** Heltec LoRa 32 V3
- **LoRaWAN Stack:** ropg/heltec_esp32_lora_v3 + LoRaWAN_ESP32 + RadioLib
- **Region:** US915
- **Join Mode:** OTAA (credentials in `secrets.h`)
- **Class:** C (continuous receive)
- **Downlink Format:** JSON (see README.md)
- **Uplink:** Periodic status/heartbeat
- **DMX Output:** Full DMX logic, including patterns and direct channel control

## Component Relationships
- **LoRaWAN Stack** handles all radio and network communication (RadioLib, LoRaWAN_ESP32)
- **Credential Management** is handled via `secrets.h` (hex strings, converted at runtime)
- **Downlink Handler** parses JSON payloads and routes commands to DMX logic
- **DMX Controller** (esp_dmx) manages all DMX output, including patterns and direct channel control
- **Uplink Handler** sends periodic status/heartbeat messages

## Data Flow
1. **Join:** Device joins LoRaWAN network using OTAA and US915 region
2. **Downlink:**
    - Receives downlink (Class C, always listening)
    - Parses payload as JSON
    - Routes command to DMX logic
    - DMX controller updates fixtures accordingly
3. **Uplink:**
    - Periodically sends status/heartbeat messages
    - Payload format can include device state, DMX status, etc.

## Deprecated/Abandoned Approaches
- The official Heltec ESP32 Dev-Boards library is no longer used due to upstream issues

## Rationale
- The new stack is actively maintained, modern, and easier to debug/extend
- RadioLib provides robust LoRaWAN Class C support and flexibility

## Next Steps
- Complete migration to new stack (see bookmark.md and tasks.md)
- Implement and test all required features
- Keep documentation up to date 