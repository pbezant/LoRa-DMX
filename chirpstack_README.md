# ChirpStack Payload Codec for DMX-LoRa

This directory contains the payload codec file for integrating the DMX-LoRa controller with ChirpStack LoRaWAN Network Server.

## Files

- `chirpstack_combined_codec.js` - Contains both encoding and decoding functions in a single file

## Installation in ChirpStack

1. Log in to your ChirpStack Application Server
2. Navigate to "Applications" and select your DMX-LoRa application
3. Select "Device-profiles" and edit the profile used by your DMX-LoRa devices
4. In the "Codec" tab, select "Custom JavaScript codec functions"
5. Copy the entire contents of `chirpstack_combined_codec.js` into the "Codec" field
6. Click "Update device-profile" to save changes

## Message Formats

### Uplink Messages (Device to ChirpStack)

The DMX-LoRa device sends the following message types:

1. **Status Messages**
   ```json
   {
     "status": "online",
     "dmx": true,
     "class": "C"
   }
   ```

2. **Heartbeat Messages**
   ```json
   {
     "hb": 1,
     "class": "C"
   }
   ```

3. **Ping Responses**
   ```json
   {
     "ping_response": "ok",
     "counter": 5
   }
   ```

### Downlink Messages (ChirpStack to Device)

The downlink message format matches the one used in your project:

#### Direct DMX Fixture Control

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

You can also use these alternative formats:

#### Simple Commands

```json
{
  "cmd": "green"
}
```

Available commands:
- `off` or `allOff` - Turn all fixtures off
- `red` or `allRed` - Set all fixtures to red
- `green` or `allGreen` - Set all fixtures to green
- `blue` or `allBlue` - Set all fixtures to blue
- `white` or `allWhite` - Set all fixtures to white
- `test` or `testTrigger` - Run the test sequence

#### Pattern Commands

```json
{
  "cmd": "pattern",
  "type": "rainbow",
  "speed": 50,
  "cycles": 3,
  "staggered": true
}
```

Available pattern types:
- `rainbow` - Rainbow chase effect
- `strobe` - Strobe effect
- `continuous` - Continuous rainbow effect
- `ping` - Ping test

Optional parameters:
- `speed` - Speed of the pattern (ms)
- `cycles` - Number of cycles to run
- `staggered` - Create a chase effect (true/false)
- `enabled` - Enable/disable the pattern (true/false)
- `color` - Pattern color (0=white, 1=red, 2=green, 3=blue)
- `count` - Number of iterations
- `onTime` - On time for strobe effect (ms)
- `offTime` - Off time for strobe effect (ms)
- `alternate` - Alternate between fixtures (true/false)

#### Alternative Light Control

With color names:

```json
{
  "cmd": "lights",
  "fixtures": [
    {
      "address": 1,
      "color": "red"
    },
    {
      "address": 2,
      "color": "green"
    }
  ]
}
```

Set all fixtures to the same color:

```json
{
  "cmd": "lights",
  "allFixtures": [1, 2, 3, 4],
  "color": "blue"
}
```

Available colors:
- `red` - Red (255,0,0,0)
- `green` - Green (0,255,0,0)
- `blue` - Blue (0,0,255,0)
- `white` - White (0,0,0,255)
- `yellow` - Yellow (255,255,0,0)
- `purple` - Purple (255,0,255,0)
- `cyan` - Cyan (0,255,255,0)
- `off` - Off (0,0,0,0)

## Testing

After setting up the codec, you can test it by sending downlink commands from the ChirpStack interface:

1. Navigate to your device in ChirpStack
2. Select the "Queue" tab
3. Enter the fPort (use 1 for compatibility with the DMX-LoRa device)
4. In the data field, enter your command in JSON format
5. Click "Enqueue" to send the command

## Sample Test Commands for ChirpStack

Here are some commands you can copy and paste to test your DMX-LoRa device:

### All Fixtures Green (Direct Format)
```json
{"lights":[{"address":1,"channels":[0,255,0,0]},{"address":2,"channels":[0,255,0,0]},{"address":3,"channels":[0,255,0,0]},{"address":4,"channels":[0,255,0,0]}]}
```

### Set All Fixtures to Red (Simple Command)
```json
{"cmd":"red"}
```

### Run Rainbow Pattern
```json
{"cmd":"pattern","type":"rainbow","cycles":3,"speed":50,"staggered":true}
```

### Set Different Colors on Different Fixtures
```json
{"cmd":"lights","fixtures":[{"address":1,"color":"red"},{"address":2,"color":"green"},{"address":3,"color":"blue"},{"address":4,"color":"yellow"}]}
```

### Strobe Effect
```json
{"cmd":"pattern","type":"strobe","color":1,"count":20,"onTime":50,"offTime":50}
```

### Continuous Rainbow Mode
```json
{"cmd":"pattern","type":"continuous","enabled":true,"speed":30,"staggered":true}
```

### Stop Any Running Pattern
```json
{"cmd":"pattern","type":"continuous","enabled":false}
```

## Troubleshooting

- Ensure your device is properly connected to the network
- Check that you're using the correct fPort (1 is recommended)
- Verify that your JSON is properly formatted
- Check the device logs for any error messages

## License

These codec files are released under the MIT License, the same as the DMX-LoRa project. 