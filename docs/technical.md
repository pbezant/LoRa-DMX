# docs/technical.md

## Technical Specifications
- LoRaWAN: OTAA, Class C (fully supported)
- Library: MCCI LMIC (replaces RadioLib)
- Radio: SX1262 (Heltec V3), future: other chipsets
- DMX: MAX485, 512 channels, up to 32 fixtures
- JSON: ArduinoJson, see README for command structure
- Logging: Serial, verbose and clean

## Established Patterns
- Modular hardware abstraction via config.h
- LoRaWAN logic in LoRaWANManager (lib/)
- Event-based architecture with LMIC
- True Class C continuous reception
- Downlink callback for all incoming LoRaWAN messages
- No runtime class switching, no provisioning dialogs 