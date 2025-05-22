# docs/architecture.md

## System Architecture

- **main.cpp**: Application entry point, high-level logic, uses LoRaWANManager and DmxController.
- **lib/LoRaWANManager/**: Modular LoRaWAN logic using MCCI LMIC, hardware-agnostic, true Class C support.
- **lib/DmxController/**: DMX output logic.
- **include/config.h**: Pin configuration for board abstraction, LMIC pin mapping.
- **include/secrets.h**: LoRaWAN keys only.

## Component Relationships
- main.cpp depends on LoRaWANManager and DmxController.
- LoRaWANManager uses MCCI LMIC for LoRaWAN stack with Class C support.
- DmxController handles DMX output.
- config.h and secrets.h provide board and security config. 