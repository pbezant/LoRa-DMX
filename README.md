# DMX LoRa Controller

This project enables control of DMX lighting fixtures using JSON commands received over The Things Network (TTN) LoRaWAN network. It uses a Heltec LoRa 32 V3 board to receive commands wirelessly and output DMX512 signals to control lighting fixtures.

## Features

- **LoRaWAN Communication**: Connects to TTN using the US915 frequency plan (for United States)
- **OTAA Activation**: Secure Over-The-Air Activation for LoRaWAN
- **JSON Command Parsing**: Processes JSON-formatted lighting commands
- **DMX512 Output**: Controls multiple DMX fixtures with different channel configurations
- **Easy Configuration**: Simple credential setup for TTN connectivity
- **Error Handling**: Robust error handling for LoRaWAN, JSON, and DMX operations
- **Low Power**: Optimized for battery operation

## Hardware Requirements

- **Heltec LoRa 32 V3** board
- **MAX485 transceiver** for DMX output (or equivalent RS-485 transceiver)
- **DMX lighting fixtures**
- **120 ohm terminating resistor** for the DMX line

## Wiring Diagram

```
Heltec LoRa 32 V3     MAX485
-----------------     -------
TX Pin (43)    -----> DI (Data In)
RX Pin (44)    -----> RO (Receive Out)
DIR Pin (8)    -----> DE/RE (Direction Control)
3.3V           -----> VCC
GND            -----> GND

MAX485                DMX Fixtures
-------               ------------
A                -----|---+---+--- (to fixtures)
B                -----|---+---+---
                      |
                      120Ω resistor
                      |
                     GND
```

## Software Setup

### Prerequisites

- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- Required libraries (automatically installed with PlatformIO):
  - ArduinoJson
  - esp_dmx
  - RadioLib

### Configuration

1. **LoRaWAN Credentials**: Edit the following definitions in `src/main.cpp` with your TTN credentials:

```cpp
#define DEVEUI_MSB "0000000000000000"  // Replace with your Device EUI (MSB format)
#define APPEUI_MSB "0000000000000000"  // Replace with your Application EUI (MSB format)
#define APPKEY_MSB "00000000000000000000000000000000"  // Replace with your App Key (MSB format)
```

2. **Pin Configuration**: If needed, adjust the pin definitions in `src/main.cpp`:

```cpp
// LoRa pins for Heltec LoRa 32 V3
#define LORA_CS   8
#define LORA_DIO1 14
#define LORA_RST  12
#define LORA_BUSY 13

// DMX configuration
#define DMX_PORT 1
#define DMX_TX_PIN 43
#define DMX_RX_PIN 44
#define DMX_DIR_PIN 8
```

## TTN Setup

1. Create a new application in TTN Console
2. Register your device using OTAA activation
3. Configure the payload formatter to pass through the JSON data

### Payload Format

The system expects JSON data in the following format:

```json
{
  "lights": [
    {
      "address": 1,
      "channels": [255, 0, 128]
    },
    {
      "address": 5,
      "channels": [0, 255, 255, 100]
    }
  ]
}
```

Where:
- `address`: The DMX start address of the fixture
- `channels`: Array of DMX channel values (0-255)

## Building and Uploading

### With PlatformIO

1. Open the project in PlatformIO
2. Connect your Heltec LoRa 32 V3 board
3. Click the Upload button

### With Arduino IDE

1. Install all required libraries
2. Open `src/main.cpp` as your sketch
3. Select "Heltec WiFi LoRa 32 V3" as your board
4. Upload the sketch

## Troubleshooting

- **LoRa Connection Issues**: Verify your TTN credentials and ensure you're in range of a TTN gateway
- **DMX Not Working**: Check your wiring, particularly the MAX485 connections
- **JSON Parsing Errors**: Ensure your downlink payload follows the expected JSON format
- **Board Not Detected**: Verify you have the correct USB drivers installed

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Heltec for their ESP32 LoRa boards
- The Things Network community
- All the open-source libraries that made this project possible 