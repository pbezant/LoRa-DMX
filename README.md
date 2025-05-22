# LoRa-DMX Modular LoRaWAN DMX Controller

## Overview
This project is a modular DMX lighting controller for ESP32 (and future chipsets) using LoRaWAN for remote control. It supports true Class C operation for continuous reception, always-on mode, and is designed for easy extension to other hardware.

## Features
- LoRaWAN (OTAA) with modular LoRaWANManager
- **True Class C support** with MCCI LMIC
- DMX output for lighting fixtures
- JSON-based command/control (see below)
- Pin configuration in `config.h`, keys in `secrets.h`
- Designed for future chipset support (e.g., RakWireless)

## Implementation Notes
- We've transitioned from RadioLib to MCCI LMIC for true Class C support
- The LoRaWANManager is a complete wrapper around MCCI LMIC
- The implementation uses event-based architecture with LMIC callbacks
- LoRaWAN credentials are stored in secrets.h
- Pin definitions are in config.h with support for different boards

## Setup
1. Clone the repo and install PlatformIO.
2. Install MCCI LMIC: `pio lib install "mcci-catena/MCCI LoRaWAN LMIC library@^4.1.1"`
3. Edit `include/secrets.h` with your LoRaWAN keys (do NOT commit real keys).
4. Edit `include/config.h` for your board's pinout if not using Heltec V3.
5. Build and upload to your board.
6. Send downlink JSON commands via your LoRaWAN network.

## LoRaWAN Class C
This project uses true Class C operation, which means:
- The device is **continuously listening** for downlink messages
- Very responsive to commands from the network server
- **Not suitable for battery operation** due to constant radio reception
- Ideal for mains-powered lighting controllers

## Patterns & JSON
See the project root README or `README.md` for supported JSON commands and patterns.

## Tech Stack
- ESP32 (Heltec LoRa 32 V3, others)
- MCCI LMIC (LoRaWAN with Class C)
- ArduinoJson
- Custom LoRaWANManager (lib/LoRaWANManager)
- PlatformIO

---
See `docs/architecture.md` and `docs/technical.md` for more details.

## Hardware Requirements

- Heltec LoRa 32 V3 microcontroller
- MAX485 transceiver for DMX output
- 120 ohm terminating resistor for the DMX line
- DMX lighting fixtures

## Wiring Diagram

Connect the MAX485 transceiver to the Heltec LoRa 32 V3 as follows:

| Heltec Pin | MAX485 Pin | Function |
|------------|------------|----------|
| 19 (TX)    | DI         | Data Input (transmit data to DMX) |
| 20 (RX)    | RO         | Data Output (receive data from DMX if needed) |
| 5          | DE & RE    | Direction control (connect to both DE and RE) |
| 3.3V       | VCC        | Power supply |
| GND        | GND        | Ground |

Connect the DMX output from the MAX485 as follows:

| MAX485 Pin | DMX Pin   | Notes |
|------------|-----------|-------|
| A          | DMX+ (3)  | Data+ |
| B          | DMX- (2)  | Data- |
| GND        | GND (1)   | Ground (optional depending on setup) |

Don't forget to add a 120 ohm resistor between A and B at the end of the DMX line for proper termination.

## Software Setup

### Required Libraries

This project uses the following libraries:
- MCCI LMIC (for LoRaWAN communication)
- ArduinoJson (for JSON parsing)
- esp_dmx (for DMX control)

Plus the custom libraries included in the project:
- LoRaManager (a wrapper around MCCI LMIC for easier LoRaWAN management)
- DmxController (a wrapper around esp_dmx for easier DMX control)

### PlatformIO Configuration

The project uses PlatformIO for dependency management and building. The configuration is in the `platformio.ini` file.

### TTN Configuration

1. Create an application in The Things Network Console
2. Register your device in the application (using OTAA)
3. Update the credentials in the code:
   - joinEUI (Application EUI)
   - devEUI (Device EUI)
   - appKey (Application Key)

### Modifying LoRaWAN Credentials

You can edit the LoRaWAN credentials in the main.cpp file:

```cpp
// LoRaWAN Credentials (can be changed by the user)
uint64_t joinEUI = 0x0000000000000001; // Replace with your Application EUI
uint64_t devEUI = 0x70B3D57ED80041B2;  // Replace with your Device EUI
uint8_t appKey[] = {0x45, 0xD3, 0x7B, 0xF3, 0x77, 0x61, 0xA6, 0x1F, 0x9F, 0x07, 0x1F, 0xE1, 0x6D, 0x4F, 0x57, 0x77}; // Replace with your Application Key
```

## JSON Command Format

The system expects JSON commands in the following format:

```json 
{
  "lights": [
    {
      "address": 1, 
      "channels": [0, 255, 0, 0]
    },
    {
      "address": 2,
      "channels": [0, 255, 0, 0]
    },
    {
      "address": 3,
      "channels": [0, 255, 0, 0]
    },
    {
      "address": 4,
      "channels": [0, 255, 0, 0]
    }
  ]
}
```

Where:
- `address`: The DMX start address of the fixture (1-512)
- `channels`: An array of DMX channel values (0-255) for each channel, relative to the start address

## TTN Payload Formatter

The included payload formatter (`ttn_payload_formatter.js`) supports multiple command types:

1. **JSON commands** - Complex control of multiple fixtures
2. **Simple color commands** - Quick color changes for all fixtures
3. **Special test commands** - For debugging and testing

Add the following JavaScript code to your TTN application's payload formatters section:

```javascript
// Copy the contents of ttn_payload_formatter.js here
// Or use the file included in this repository
```

## Simple Command Format

For quick testing, the payload formatter supports simplified commands:

### Numeric Commands (Single byte)
- `0` - Turn all fixtures OFF
- `1` - Set all fixtures to RED
- `2` - Set all fixtures to GREEN
- `3` - Set all fixtures to BLUE
- `4` - Set all fixtures to WHITE

### Text Commands
- `go` - Process the built-in example JSON (sets fixtures to green)
- `{"command": "test"}` - Run a special test mode (sets all fixtures to green)
- `{"command": "red"}` - Set all fixtures to red
- `{"command": "green"}` - Set all fixtures to green
- `{"command": "blue"}` - Set all fixtures to blue
- `{"command": "white"}` - Set all fixtures to white
- `{"command": "off"}` - Turn all fixtures off

## Light Pattern Commands

The system supports dynamic light pattern effects through downlink commands:

### Simple Pattern Commands (Easy to use)
- `{"pattern": "colorFade"}` - Smoothly transitions through all colors
- `{"pattern": "rainbow"}` - Creates a rainbow effect across multiple fixtures
- `{"pattern": "strobe"}` - Flashes all fixtures on and off
- `{"pattern": "chase"}` - Creates a chasing light effect
- `{"pattern": "alternate"}` - Alternates between fixtures
- `{"pattern": "stop"}` - Stops any currently running pattern

### Advanced Pattern Control
For more control over pattern behavior, you can specify parameters:

```json
{
  "pattern": {
    "type": "colorFade",
    "speed": 30,
    "cycles": 10
  }
}
```

#### Parameters:
- `type`: Pattern type (colorFade, rainbow, strobe, chase, alternate)
- `speed`: Update speed in milliseconds (lower = faster)
- `cycles`: Number of cycles to run before stopping (0 = infinite)

#### Default values:
- colorFade: speed=50ms, cycles=5
- rainbow: speed=50ms, cycles=3
- strobe: speed=100ms, cycles=10
- chase: speed=200ms, cycles=3
- alternate: speed=300ms, cycles=5

## Example Commands

1. **Green Fixtures (All addresses 1-4)**
   ```json
   {
     "lights": [
       {
         "address": 1, 
         "channels": [0, 255, 0, 0]
       },
       {
         "address": 2,
         "channels": [0, 255, 0, 0]
       },
       {
         "address": 3,
         "channels": [0, 255, 0, 0]
       },
       {
         "address": 4,
         "channels": [0, 255, 0, 0]
       }
     ]
   }
   ```
   - Purpose: Set all fixtures to green (RGBW format)
   - Device Action: Updates DMX channels for all fixtures

2. **Different Colors for Different Fixtures**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [255, 0, 0, 0]    // Red
       },
       {
         "address": 2,
         "channels": [0, 255, 0, 0]    // Green
       },
       {
         "address": 3,
         "channels": [0, 0, 255, 0]    // Blue
       },
       {
         "address": 4,
         "channels": [255, 255, 0, 0]  // Yellow
       }
     ]
   }
   ```
   - Purpose: Set each fixture to a different color
   - Device Action: Updates DMX channels with different colors for each fixture

3. **Blue and Red Fixtures**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [0, 0, 255, 0]
       },
       {
         "address": 5,
         "channels": [255, 0, 0, 0]
       }
     ]
   }
   ```
   - Purpose: Set fixture 1 to blue and fixture 5 to red
   - Device Action: Updates DMX channels for the specified fixtures

4. **Rainbow Effect**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [255, 0, 0, 0]     // Red
       },
       {
         "address": 2,
         "channels": [255, 165, 0, 0]   // Orange
       },
       {
         "address": 3,
         "channels": [255, 255, 0, 0]   // Yellow
       },
       {
         "address": 4,
         "channels": [0, 255, 0, 0]     // Green
       },
       {
         "address": 5,
         "channels": [0, 0, 255, 0]     // Blue
       },
       {
         "address": 6,
         "channels": [75, 0, 130, 0]    // Indigo
       },
       {
         "address": 7,
         "channels": [143, 0, 255, 0]   // Violet
       }
     ]
   }
   ```
   - Purpose: Create a rainbow effect across multiple fixtures
   - Device Action: Sets each fixture to a different color in the rainbow spectrum

5. **Rainbow Pattern (Dynamic)**
   ```json
   {
     "pattern": "rainbow"
   }
   ```
   - Purpose: Create an animated rainbow effect that cycles through all fixtures
   - Device Action: Continuously updates fixtures with different colors in a rainbow pattern

6. **Color Fade Pattern**
   ```json
   {
     "pattern": {
       "type": "colorFade",
       "speed": 30,
       "cycles": 10
     }
   }
   ```
   - Purpose: Create a smooth transition through all colors
   - Device Action: Gradually fades all fixtures through the color spectrum

7. **Strobe Effect**
   ```json
   {
     "pattern": {
       "type": "strobe",
       "speed": 100,
       "cycles": 20
     }
   }
   ```
   - Purpose: Create a strobe light effect
   - Device Action: Rapidly flashes all fixtures on and off

8. **Chase Pattern**
   ```json
   {
     "pattern": "chase"
   }
   ```
   - Purpose: Create a "chasing lights" effect
   - Device Action: Lights up one fixture at a time in sequence

9. **Stop Patterns**
   ```json
   {
     "pattern": "stop"
   }
   ```
   - Purpose: Stop any currently running pattern
   - Device Action: Stops pattern execution and leaves fixtures in their current state

## Troubleshooting

### LED Blink Codes

The onboard LED will blink to indicate different states:
- 2 blinks (slow): Device started successfully
- 3 blinks: Successfully joined TTN
- 4 blinks: Failed to join TTN
- 5 blinks (rapid): Error in initialization or JSON processing

### Serial Output

Connect to the serial monitor at 115200 baud to see detailed diagnostic information.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Credits

- Heltec for the LoRa 32 V3 hardware
- The Things Network for LoRaWAN infrastructure
- Contributors to the used libraries 