# LoRaManager Technical Blueprint

## Core Architecture Decisions

- **Modular Design**: Library structured with clear separation between core LoRa functionality and protocol-specific implementations
- **OOP Approach**: Class-based design for LoRa devices, connections, and protocols
- **Event-Driven**: Event callbacks for handling asynchronous LoRa communications
- **Hardware Abstraction**: Designed to work with different LoRa hardware implementations

## Tech Stack Details

- **Language**: C++ for Arduino and ESP32 compatibility
- **Platform**: PlatformIO library format
- **Hardware Support**: 
  - ESP32 microcontrollers
  - LoRa radio modules (SX127x)
  - Additional MCU platforms as needed

## API Patterns

- **Builder Pattern**: For configuring LoRa devices with chains of method calls
- **Callback Registration**: For event-driven message handling
- **State Management**: Internal state tracking with getters for device status
- **Connection Profiles**: Reusable connection configurations

## Communication Protocol

- **LoRaWAN Classes**:
  - Class A: Bidirectional with downlink following uplink
  - Class B: Scheduled downlink slots
  - Class C: Continuous receive capabilities
- **Message Types**:
  - Confirmed messages (requiring ACK)
  - Unconfirmed messages
  - MAC commands

## Library Structure

- **Core Components**:
  - Device management
  - Connection handling
  - Protocol implementation
  - Payload formatting
- **Utilities**:
  - Encryption helpers
  - Debugging tools
  - Configuration storage 