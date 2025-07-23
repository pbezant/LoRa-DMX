# DMX LoRa Control System

This project implements a DMX lighting controller using a Heltec LoRa 32 V3 microcontroller that receives control commands over The Things Network (TTN) via LoRaWAN. It allows remote control of DMX lighting fixtures through JSON-formatted messages.

## Features

- Connects to The Things Network (TTN) using LoRaWAN (US915 frequency plan)
- Uses OTAA (Over-The-Air Activation) for secure network joining
- Receives JSON-formatted commands for controlling DMX fixtures
- Processes JSON payloads to control multiple DMX fixtures at different addresses
- Includes comprehensive error handling and debugging features
- Supports dynamic fixture configuration without hardcoded settings

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
- RadioLib (for LoRaWAN communication)
- ArduinoJson (for JSON parsing)
- esp_dmx (for DMX control)

Plus the custom libraries included in the project:
- LoRaManager (a wrapper around RadioLib for easier LoRaWAN management)
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

## Configuration Downlink: Set Number of Lights

By default, the device is configured to control **25 lights** (the maximum supported in a single downlink). You can change this at any time by sending a special configuration downlink.

### How to Set Number of Lights

Send a downlink command in this format:

```json
{
  "config": { "numLights": 12 }
}
```

- This will set the device to expect/control 12 lights.
- The value is capped between 1 and 25.
- The device will re-initialize its fixture configuration and print debug info on the serial monitor.
- The onboard LED will blink 4 times to confirm the change.

#### Payload Format (ChirpStack/TTN)
- The config downlink is encoded as: `[0xC0, N]` where `N` is the number of lights (1–25).

#### Example
To set the device to 8 lights:
```json
{
  "config": { "numLights": 8 }
}
```

---

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

## ChirpStack Payload Codec (chirpstack_codec.js)

This project now includes a dedicated ChirpStack-compatible payload codec: `chirpstack_codec.js`.

### Purpose
- **encodeDownlink(input):** Encodes application JSON commands into a compact byte array for LoRaWAN downlink to the device.
- **decodeUplink(input):** Decodes uplink byte payloads from the device into structured JSON for ChirpStack.

### Usage
- Upload `chirpstack_codec.js` to your ChirpStack Application's Payload Codec section.
- ChirpStack will automatically use `encodeDownlink` for downlinks and `decodeUplink` for uplinks.

### Downlink Encoding (encodeDownlink)
- Supports complex DMX control via JSON:
  ```json
  {
    "lights": [
      { "address": 1, "channels": [255, 0, 0, 0] },
      { "address": 5, "channels": [0, 255, 0, 0] }
    ]
  }
  ```
- Encodes the number of lights, each address, and 4 channel values per light into a compact byte array.
- Handles errors (e.g., malformed input) gracefully, returning an empty payload if invalid.

### Uplink Decoding (decodeUplink)
- Decodes device status, sensor, or DMX state reports from bytes to JSON.
- Example decoded uplink:
  ```json
  {
    "data": {
      "dmxStatus": { "numberOfLights": 2 },
      "lights": [
        { "address": 1, "channels": [255, 0, 0, 0] },
        { "address": 5, "channels": [0, 255, 0, 0] }
      ]
    }
  }
  ```
- Handles unknown or malformed payloads with warnings and error fields.

### Error Handling
- Both functions provide robust error handling and will not crash ChirpStack if given unexpected input.

---

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
         "address": 5,
         "channels": [0, 255, 0, 0]
       },
       {
         "address": 9,
         "channels": [0, 255, 0, 0]
       },
       {
         "address": 13,
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
         "address": 5,
         "channels": [0, 255, 0, 0]    // Green
       },
       {
         "address": 9,
         "channels": [0, 0, 255, 0]    // Blue
       },
       {
         "address": 13,
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
         "address": 5,
         "channels": [255, 165, 0, 0]   // Orange
       },
       {
         "address": 9,
         "channels": [255, 255, 0, 0]   // Yellow
       },
       {
         "address": 13,
         "channels": [0, 255, 0, 0]     // Green
       },
       {
         "address": 17,
         "channels": [0, 0, 255, 0]     // Blue
       },
       {
         "address": 21,
         "channels": [75, 0, 130, 0]    // Indigo
       },
       {
         "address": 25,
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