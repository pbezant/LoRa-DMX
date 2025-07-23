// TTN Payload Formatters for DMX LoRa Control System
// Copy and paste these functions into the TTN console under your application settings

// Uplink decoder function (device to application)
function decodeUplink(input) {
  // Try to parse as JSON first
  try {
    // Convert bytes to string
    var str = String.fromCharCode.apply(null, input.bytes);
    var data = JSON.parse(str);
    
    // Return the parsed JSON and add metadata
    return {
      data: data,
      warnings: [],
      errors: []
    };
  } catch (error) {
    // If it's not valid JSON, return raw data
    return {
      data: {
        raw: bytesToHex(input.bytes),
        text: String.fromCharCode.apply(null, input.bytes)
      },
      warnings: ["Failed to parse as JSON: " + error.message],
      errors: []
    };
  }
}

// Helper function to format uptime in a human-readable way
function formatUptime(seconds) {
  seconds = parseInt(seconds, 10);
  var days = Math.floor(seconds / 86400);
  seconds %= 86400;
  var hours = Math.floor(seconds / 3600);
  seconds %= 3600;
  var minutes = Math.floor(seconds / 60);
  seconds %= 60;
  
  var result = "";
  if (days > 0) result += days + "d ";
  if (hours > 0 || days > 0) result += hours + "h ";
  if (minutes > 0 || hours > 0 || days > 0) result += minutes + "m ";
  result += seconds + "s";
  
  return result;
}

// Helper function to convert bytes to hex for display
function bytesToHex(bytes) {
  var hex = [];
  for (var i = 0; i < bytes.length; i++) {
    var current = bytes[i] < 0x10 ? "0" + bytes[i].toString(16) : bytes[i].toString(16);
    hex.push(current);
  }
  return hex.join("");
}

// Helper function to sanitize strings, removing control characters
function sanitizeString(str) {
  return str.replace(/[\x00-\x1F\x7F-\x9F]/g, '');
}

// Downlink encoder function (application to device)
function encodeDownlink(input) {
  // NEW: Direct hex string support
  if (input.data.hex && typeof input.data.hex === 'string') {
    // Convert hex string to bytes
    var hexStr = input.data.hex.replace(/[^0-9A-Fa-f]/g, ''); // Remove any non-hex characters
    var bytes = [];
    
    // Ensure even number of characters
    if (hexStr.length % 2 !== 0) {
      hexStr = '0' + hexStr; // Pad with leading zero if odd length
    }
    
    // Convert hex pairs to bytes
    for (var i = 0; i < hexStr.length; i += 2) {
      bytes.push(parseInt(hexStr.substr(i, 2), 16));
    }
    
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }
  
  // NEW: Raw byte array support
  if (input.data.raw && Array.isArray(input.data.raw)) {
    var bytes = [];
    for (var i = 0; i < input.data.raw.length; i++) {
      var val = input.data.raw[i];
      if (typeof val === 'number' && val >= 0 && val <= 255) {
        bytes.push(Math.floor(val));
      }
    }
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }

  // CASE 1: Config downlink for number of lights
  if (input.data.config && typeof input.data.config.numLights === 'number') {
    var n = input.data.config.numLights;
    if (n < 1) n = 1;
    if (n > 25) n = 25;
    return {
      bytes: [0xC0, n],
      fPort: input.fPort || 1
    };
  }

  // CASE 2: Simple command strings - COMPACT BINARY
  if (input.data.command === "off" || input.data.command === 0) {
    return { bytes: [0x00], fPort: input.fPort || 1 };
  }
  if (input.data.command === "red" || input.data.command === 1) {
    return { bytes: [0x01], fPort: input.fPort || 1 };
  }
  if (input.data.command === "green" || input.data.command === 2) {
    return { bytes: [0x02], fPort: input.fPort || 1 };
  }
  if (input.data.command === "blue" || input.data.command === 3) {
    return { bytes: [0x03], fPort: input.fPort || 1 };
  }
  if (input.data.command === "white" || input.data.command === 4) {
    return { bytes: [0x04], fPort: input.fPort || 1 };
  }
  if (input.data.command === "test") {
    return { bytes: [0xAA], fPort: input.fPort || 1 };
  }
  if (input.data.command === "go") {
    return { bytes: [0x67, 0x6F], fPort: input.fPort || 1 }; // ASCII "go"
  }

  // CASE 3: Pattern commands - COMPACT BINARY ENCODING
  if (input.data.pattern) {
    var bytes = [];
    var patternType = '';
    var speed = 50;
    var cycles = 5;
    
    // Handle direct pattern names as shortcuts
    if (typeof input.data.pattern === 'string') {
      patternType = input.data.pattern;
      
      // Apply default values for each pattern type
      switch (input.data.pattern) {
        case 'colorFade':
          speed = 50;
          cycles = 5;
          break;
        case 'rainbow':
          speed = 50;
          cycles = 3;
          break;
        case 'strobe':
          speed = 100;
          cycles = 10;
          break;
        case 'chase':
          speed = 200;
          cycles = 3;
          break;
        case 'alternate':
          speed = 300;
          cycles = 5;
          break;
        case 'stop':
          return { bytes: [0xF0], fPort: input.fPort || 1 }; // Pattern stop command
      }
    } 
    // Handle full pattern object with parameters
    else if (typeof input.data.pattern === 'object') {
      patternType = input.data.pattern.type || 'colorFade';
      speed = input.data.pattern.speed || 50;
      cycles = input.data.pattern.cycles || 5;
      
      if (patternType === 'stop') {
        return { bytes: [0xF0], fPort: input.fPort || 1 }; // Pattern stop command
      }
    }
    
    // Map pattern types to enum values
    var patternEnum = 0; // Default to colorFade
    switch (patternType) {
      case 'colorFade': patternEnum = 0; break;
      case 'rainbow': patternEnum = 1; break;
      case 'strobe': patternEnum = 2; break;
      case 'chase': patternEnum = 3; break;
      case 'alternate': patternEnum = 4; break;
      default: patternEnum = 0; break;
    }
    
    // Ensure speed and cycles are within valid ranges
    if (speed < 1) speed = 1;
    if (speed > 65535) speed = 65535;
    if (cycles < 1) cycles = 1;
    if (cycles > 65535) cycles = 65535;
    
    // Encode as: [0xF1, patternEnum, speed_low, speed_high, cycles_low, cycles_high]
    bytes = [
      0xF1,                    // Pattern command marker
      patternEnum,             // Pattern type (0-4)
      speed & 0xFF,            // Speed low byte
      (speed >> 8) & 0xFF,     // Speed high byte
      cycles & 0xFF,           // Cycles low byte
      (cycles >> 8) & 0xFF     // Cycles high byte
    ];
    
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }

  // CASE 4: Lights array - COMPACT BINARY ENCODING
  if (input.data.lights && Array.isArray(input.data.lights)) {
    var bytes = [];
    var validLights = [];
    
    // Filter and validate lights
    for (var i = 0; i < input.data.lights.length; i++) {
      var light = input.data.lights[i];
      if (light && typeof light.address === 'number' && Array.isArray(light.channels) && light.channels.length === 4) {
        // Validate address (1-512) and channels (0-255)
        if (light.address >= 1 && light.address <= 512) {
          var validChannels = true;
          for (var j = 0; j < 4; j++) {
            if (typeof light.channels[j] !== 'number' || light.channels[j] < 0 || light.channels[j] > 255) {
              validChannels = false;
              break;
            }
          }
          if (validChannels) {
            validLights.push(light);
          }
        }
      }
    }
    
    if (validLights.length === 0) {
      return { bytes: [], fPort: input.fPort || 1 };
    }
    
    // Check payload size limit (assume max ~50 bytes for LoRaWAN)
    var maxLights = Math.floor((50 - 1) / 5); // 1 byte for count + 5 bytes per light
    if (validLights.length > maxLights) {
      validLights = validLights.slice(0, maxLights);
    }
    
    // Encode as compact binary: [numLights, address1, ch1, ch2, ch3, ch4, ...]
    bytes.push(validLights.length);
    
    for (var i = 0; i < validLights.length; i++) {
      var light = validLights[i];
      bytes.push(light.address);
      bytes.push(Math.floor(light.channels[0]));
      bytes.push(Math.floor(light.channels[1]));
      bytes.push(Math.floor(light.channels[2]));
      bytes.push(Math.floor(light.channels[3]));
    }
    
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }
  
  // Fallback - any other data is converted to a string and sent as JSON
  if (typeof input.data === 'object') {
    var jsonString = JSON.stringify(input.data);
    var bytes = [];
    for (var i = 0; i < jsonString.length; i++) {
      bytes.push(jsonString.charCodeAt(i));
    }
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }
  
  // Return empty if nothing matched
  return {
    bytes: [],
    fPort: input.fPort || 1
  };
}

// Downlink decoder function (for debugging in console)
function decodeDownlink(input) {
  try {
    // Convert byte array to string
    var jsonString = String.fromCharCode.apply(null, input.bytes);
    
    // Try to parse as JSON for validation
    var jsonData = JSON.parse(jsonString);
    
    // Check if it's a DMX control message
    if (jsonData.lights && Array.isArray(jsonData.lights)) {
      return {
        data: {
          jsonData: jsonString,
          lights: jsonData.lights.length,
          summary: jsonData.lights.map(function(light) {
            return "Address: " + light.address + ", Channels: [" + light.channels.join(", ") + "]";
          }).join(" | ")
        },
        warnings: [],
        errors: []
      };
    }
    
    // Otherwise just return the JSON data
    return {
      data: {
        jsonData: jsonString
      },
      warnings: [],
      errors: []
    };
  } catch (error) {
    return {
      data: {
        raw: bytesToHex(input.bytes),
        text: sanitizeString(String.fromCharCode.apply(null, input.bytes))
      },
      warnings: ["Failed to validate as JSON: " + error],
      errors: []
    };
  }
}

// Example usage:
// 
// DOWNLINK EXAMPLES (TTN -> Device):
// To send a command to control DMX fixtures:
// {
//   "lights": [
//     {
//       "address": 1,
//       "channels": [255, 0, 128, 0]
//     },
//     {
//       "address": 5,
//       "channels": [255, 255, 100, 0]
//     }
//   ]
// }
//
// UPLINK EXAMPLES (Device -> TTN):
// Status message when the device is online:
// {
//   "status": "online",
//   "dmx": true
// }
// 
// Periodic alive message with uptime:
// {
//   "status": "alive",
//   "uptime": 3600
// } 