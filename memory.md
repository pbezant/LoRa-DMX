# memory.md

## Implementation Decisions
- Migrated from RadioLib to MCCI LMIC for true Class C support.
- Modularized LoRaWAN logic into LoRaWANManager for reuse and future chipset support.
- Pin configuration separated into config.h for easy board swaps.
- Always use keys from secrets.h, never from serial or provisioning dialogs.

## Edge Cases Handled
- Full implementation of Class C continuous reception (not limited to RX windows).
- Event-based architecture using LMIC callbacks.
- Handles always-on operation (no deep sleep).

## Problems Solved
- Achieved true Class C continuous reception (not possible with RadioLib).
- Decoupled hardware-specific pinout from LoRaWAN logic.
- Made LoRaWAN logic reusable for other projects.

## Approaches Rejected
- RadioLib (lacks proper Class C support).
- No runtime class switching (compile/startup only).
- No provisioning dialogs or serial entry for keys (security, simplicity). 