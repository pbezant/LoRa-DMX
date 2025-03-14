# DmxController Library

This library provides a simple interface for controlling DMX fixtures using an ESP32 with the esp_dmx library.

## Compatibility Notes

### esp_dmx Library Version Compatibility

The DmxController library has been tested with the following esp_dmx library versions:

- **esp_dmx v2.0.3**: Fully compatible with the current implementation
- **esp_dmx v3.0.0 and above**: Requires modifications to function signatures

If you're experiencing compilation errors like:

```
too many arguments to function 'bool dmx_driver_install(dmx_port_t, const dmx_config_t*, int)'
```

or

```
'dmx_send_num' was not declared in this scope
```

You have two options:

1. **Downgrade the esp_dmx library** to version 2.0.3 (recommended)
   - In platformio.ini, specify: `someweisguy/esp_dmx @ 2.0.3`

2. **Modify the DmxController.cpp file** to match the newer API:
   - For `dmx_driver_install`, remove the third parameter or use the correct signature
   - For `dmx_send`, use the correct signature with the size parameter

## Usage

```cpp
#include "DmxController.h"

// Create a DMX controller instance
// Parameters: dmxPort, txPin, rxPin, dirPin
DmxController dmx(1, 43, 44, 8);

void setup() {
  Serial.begin(115200);
  
  // Initialize the DMX controller
  dmx.begin();
  
  // Clear all DMX channels
  dmx.clearAllChannels();
  dmx.sendData();
}

void loop() {
  // Set a fixture's color (RGB)
  Color color = {255, 0, 0}; // Red
  dmx.setFixtureColor(1, color); // Set fixture at address 1
  
  // Send the DMX data
  dmx.sendData();
  
  delay(1000);
}
```

## API Reference

See the header file for a complete API reference.

## License

This library is released under the MIT License. 