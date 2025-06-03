# Hybrid LoRaWAN Class C Implementation

This example demonstrates our hybrid approach that combines nopnop2002's SX1262 driver with RadioLib's LoRaWAN stack to achieve true Class C operation on the Heltec WiFi LoRa 32 V3.2 board.

## Overview

The key components of this implementation:

1. **SX1262Radio** - A wrapper around nopnop2002's SX1262 driver that provides compatibility with RadioLib.
2. **LoRaWANAdapter** - An adapter between SX1262Radio and RadioLib's LoRaWAN stack for true Class C operation.
3. **LoRaWANHelper** - The existing public API that uses our new components to provide a consistent interface to the main application.

## Features

- **True Class C Operation** - Continuous downlink reception using interrupt-driven packet detection.
- **Bandwidth Compatibility** - Automatically detects and uses supported bandwidths based on previous testing.
- **Clean API** - Maintains the existing LoRaWANHelper API for easy integration.
- **Robust Uplink/Downlink** - Handles both uplink and downlink messages with proper error reporting.
- **JSON Support** - Uses ArduinoJson to format uplink messages and could be used to parse JSON downlinks.

## Hardware Configuration

- **Board**: Heltec WiFi LoRa 32 V3.2
- **SX1262 Pins**:
  - NSS: GPIO_NUM_8
  - DIO1: GPIO_NUM_14
  - NRST: GPIO_NUM_12
  - BUSY: GPIO_NUM_13

## Requirements

- PlatformIO with ESP32 support
- RadioLib library
- nopnop2002's SX1262 driver (Arduino-LoRa-Ra01S)
- ArduinoJson library
- ropg/heltec_esp32_lora_v3 library

## Configuration

Update the `include/secrets.h` file with your LoRaWAN credentials:

```cpp
#define DEVEUI "your-device-eui-here"
#define APPEUI "your-app-eui-here"
#define APPKEY "your-app-key-here"
#define NWKKEY "your-network-key-here" // Same as APPKEY for LoRaWAN 1.0.x
```

## Building and Running

1. Open the project in PlatformIO
2. Build the project
3. Upload to your Heltec WiFi LoRa 32 V3.2 board
4. Monitor the serial output for debug information

## Implementation Details

This example implements:

1. **Initialization**: Sets up the SX1262Radio and LoRaWANAdapter components.
2. **OTAA Join**: Joins the LoRaWAN network using OTAA with credentials from secrets.h.
3. **Class C Operation**: Enables true Class C operation for continuous downlink reception.
4. **Periodic Uplinks**: Sends a simple JSON status message every minute.
5. **Downlink Processing**: Processes downlinks, displaying them on the OLED and printing them to serial.

## Next Steps

- Customize the uplink/downlink payload format for your application
- Implement more sophisticated downlink handling (e.g., commands)
- Add error recovery mechanisms
- Integrate with other sensors or actuators 