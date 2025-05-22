# project.md

## Core Architecture
- Entry point: src/main.cpp
- LoRaWAN logic: lib/LoRaWANManager/LoRaWANManager.{h,cpp}
- Pin config: include/config.h
- Keys: include/secrets.h
- DMX logic: lib/DmxController/

## Tech Stack
- ESP32 (Heltec LoRa 32 V3, future: RakWireless, etc.)
- PlatformIO
- MCCI LMIC (LoRaWAN with Class C support)
- ArduinoJson

## API Patterns
- LoRaWANManager exposes begin(), joinNetwork(), sendData(), sendString(), setDownlinkCallback(), handleEvents(), getLastRssi(), getLastSnr(), getDeviceClass().
- Event-based architecture with LMIC callbacks.
- Downlink callback is used for all incoming LoRaWAN messages.

## Database Schema
- N/A (no persistent database, only device-side storage) 