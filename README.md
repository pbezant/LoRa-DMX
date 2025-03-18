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
      "channels": [255, 0, 128, 0]
    },
    {
      "address": 5,
      "channels": [255, 255, 100, 0]
    }
  ]
}
```

Where:
- `address`: The DMX start address of the fixture (1-512)
- `channels`: An array of DMX channel values (0-255) for each channel, relative to the start address

## TTN Payload Formatter

Add the following JavaScript decoder in your TTN application to format the downlink payload as a JSON string:

```javascript
function decodeDownlink(input) {
  // For downlink, we simply pass through the JSON string
  try {
    var jsonString = String.fromCharCode.apply(null, input.bytes);
    return {
      data: {
        jsonData: jsonString
      },
      warnings: [],
      errors: []
    };
  } catch (error) {
    return {
      data: {},
      warnings: [],
      errors: ["Failed to process downlink: " + error]
    };
  }
}

function encodeDownlink(input) {
  // Convert the input JSON object to a string
  var jsonString = JSON.stringify(input.data);
  
  // Convert the string to an array of bytes
  var bytes = [];
  for (var i = 0; i < jsonString.length; i++) {
    bytes.push(jsonString.charCodeAt(i));
  }
  
  return {
    bytes: bytes,
    fPort: 1
  };
}
```

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

### Example Commands

1. **Ping Command**
   ```json
   {
     "test": {
       "pattern": "ping"
     }
   }
   ```
   - Purpose: Test downlink communication
   - Expected Response: `{"ping_response":"ok"}`
   - Device Action: Blinks LED 3 times

2. **DMX Control - Full Brightness**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [255, 255, 255, 255, 255, 255, 255, 255]
       }
     ]
   }
   ```
   - Purpose: Set all channels of a light fixture to maximum brightness
   - Expected Response: `{"status":"DMX_OK"}`
   - Device Action: Sends DMX data to the specified fixture

3. **DMX Control - Individual Channels**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [255, 0, 255, 0, 255, 0, 255, 0]
       }
     ]
   }
   ```
   - Purpose: Set alternating channels to full brightness and off
   - Expected Response: `{"status":"DMX_OK"}`
   - Device Action: Sends DMX data to the specified fixture

4. **DMX Control - Multiple Fixtures**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [255, 255, 255, 0, 0, 0, 0, 0]
       },
       {
         "address": 2,
         "channels": [0, 0, 0, 255, 255, 255, 0, 0]
       }
     ]
   }
   ```
   - Purpose: Control multiple fixtures with different channel values
   - Expected Response: `{"status":"DMX_OK"}`
   - Device Action: Sends DMX data to all specified fixtures 