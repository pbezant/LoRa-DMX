# tasks/tasks.md

## Current Tasks
- [x] Refactor LoRaWAN logic into modular LoRaWANManager
- [x] Move all pin config to config.h
- [x] Keep secrets.h for keys only
- [x] Update main.cpp to use new manager and config
- [x] Stub out required documentation
- [x] Test downlink reception and serial output
- [x] Migrate from RadioLib to MCCI LMIC for true Class C support
- [ ] Test Class C continuous reception
- [ ] Implement and test JSON command processing
- [ ] Test with actual DMX fixtures

## Requirements
- Modular, chipset-agnostic LoRaWAN logic
- True Class C support (continuous reception)
- Pin config in config.h
- Keys in secrets.h
- JSON command structure as in README
- Serial logging for all events 