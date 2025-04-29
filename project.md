# DMX-LoRa Project Technical Blueprint

## Core Architecture

### Hardware Components
- **ESP32-based Microcontroller**: Heltec WiFi LoRa 32 V3 with ESP32S3 processor
- **LoRa Radio Module**: SX1262 for long-range radio communication
- **DMX Interface**: Hardware interface for DMX512 protocol

### Software Architecture
- **Main Application**: Central control loop managing DMX and LoRa operations
- **LoRaWAN Communication**: Class A, B, and C device operation modes
- **DMX Controller**: Handles DMX512 protocol for lighting control
- **Command Processing**: JSON-based command processing system

## Tech Stack

### Framework
- **Arduino Framework**: Used on top of ESP-IDF for ESP32
- **PlatformIO**: Development ecosystem for embedded systems

### Libraries
- **RadioLib (v7.1.2.0)**: Core radio communication library
- **esp_dmx**: DMX512 communication library
- **ArduinoJSON**: JSON parsing/generation for command processing
- **FreeRTOS**: Used for multitasking, mutexes, and task management

### Custom Libraries
- **LoRaManager**: Wrapper around RadioLib for simplified LoRaWAN operations
- **DmxController**: Wrapper around esp_dmx for controlling DMX fixtures

## API Patterns

### LoRaWAN Communication
- **OTAA Activation**: Over-the-air activation for secure network joining
- **Downlink Processing**: Callback-based approach for handling downlink messages
- **Class C Operation**: Continuous reception mode for real-time control
- **Packet Format**: Binary commands or JSON-based command structure

### DMX Control
- **Universe Control**: Single DMX universe (512 channels)
- **Lighting Patterns**: Pre-programmed patterns (rainbow, solid colors, etc.)
- **Remote Control**: Commands received via LoRaWAN to control DMX outputs

### Command Structure
- **JSON Commands**: Structure like `{"cmd": "command_type", "params": {...}}`
- **Binary Commands**: Compact format for bandwidth-constrained situations
- **Test Patterns**: Special commands for running test sequences

## Communication Protocols

### LoRaWAN
- **Frequency Bands**: US915 (default), EU868 supported
- **Data Rates**: DR0-DR4 supported, DR1 used for reliability
- **Ports**: Various ports used for different command types
- **Confirmed Messages**: Used for critical commands with retry mechanism

### DMX512
- **Channels**: Full 512-channel support
- **Refresh Rate**: ~40Hz update rate for smooth transitions
- **RDM Support**: Remote Device Management for bidirectional communication

## State Management
- **Global State Variables**: Track system status, modes, and settings
- **Persistent Storage**: Uses ESP32 preferences for configuration persistence
- **Mode Transitions**: State machine for handling operational modes (normal, test, demo) 