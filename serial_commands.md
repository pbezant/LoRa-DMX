# Serial Commands for Testing DMX-LoRa Lights

You can use the following serial commands to test if your DMX fixtures are working correctly. These commands can be sent through the Serial Monitor in Arduino IDE or PlatformIO at a baud rate of 115200.

## Simple Numeric Commands

Send a single digit (0-4) to control all fixtures:

| Command | Effect |
|---------|--------|
| `0` | Turn all fixtures OFF |
| `1` | Set all fixtures to RED |
| `2` | Set all fixtures to GREEN |
| `3` | Set all fixtures to BLUE |
| `4` | Set all fixtures to WHITE |

## JSON Commands for Advanced Control

### Direct Light Control

Control individual fixtures with their RGB(W) values:

```json
{
  "lights": [
    {
      "address": 1, 
      "channels": [255, 0, 0, 0]
    },
    {
      "address": 2,
      "channels": [0, 255, 0, 0]
    },
    {
      "address": 3,
      "channels": [0, 0, 255, 0]
    },
    {
      "address": 4,
      "channels": [0, 0, 0, 255]
    }
  ]
}
```

### Test Patterns

Run test patterns on your fixtures:

#### Rainbow Chase Pattern
```json
{
  "test": {
    "pattern": "rainbow",
    "cycles": 3,
    "speed": 50,
    "staggered": true
  }
}
```

Parameters:
- `cycles`: Number of cycles to run (1-10, default: 3)
- `speed`: Speed in milliseconds (10-500, default: 50)
- `staggered`: Create chase effect (true/false, default: true)

#### Strobe Effect
```json
{
  "test": {
    "pattern": "strobe",
    "color": 1,
    "count": 20,
    "onTime": 50,
    "offTime": 50,
    "alternate": false
  }
}
```

Parameters:
- `color`: 0=white, 1=red, 2=green, 3=blue (default: 0)
- `count`: Number of flashes (1-100, default: 20)
- `onTime`: On time in milliseconds (10-1000, default: 50)
- `offTime`: Off time in milliseconds (10-1000, default: 50)
- `alternate`: Alternate between fixtures (true/false, default: false)

#### Continuous Rainbow Effect
```json
{
  "test": {
    "pattern": "continuous",
    "enabled": true,
    "speed": 30,
    "staggered": true
  }
}
```

Parameters:
- `enabled`: Enable/disable the effect (true/false, default: true)
- `speed`: Speed in milliseconds (5-500, default: 30)
- `staggered`: Create staggered effect (true/false, default: true)

To stop the continuous rainbow effect:
```json
{
  "test": {
    "pattern": "continuous",
    "enabled": false
  }
}
```

## How to Use

1. Connect your DMX-LoRa device to your computer via USB
2. Open Serial Monitor (baud rate: 115200)
3. Type or paste one of the commands above and press Enter
4. The device will execute the command and your fixtures should respond accordingly

## Troubleshooting

If your fixtures don't respond to serial commands:

1. Make sure your DMX fixtures are powered on and connected to the DMX-LoRa controller
2. Check that your fixtures are set to the correct DMX addresses
3. Verify that the DMX controller is initialized (you should see "DMX initialized successfully" in the serial output at startup)
4. Try a simple command like "2" (green) first to verify basic functionality
5. Ensure your serial monitor is set to the correct baud rate (115200) 