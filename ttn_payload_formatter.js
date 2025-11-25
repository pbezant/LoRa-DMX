// TTN Payload Formatters for DMX LoRa Control System
// Copy and paste these functions into the TTN console under your application settings

// Uplink decoder function (device to application)
function decodeUplink(input) {
  var bytes = input.bytes || [];
  var result = {
    data: {},
    warnings: [],
    errors: []
  };

  if (bytes.length === 0) {
    result.errors.push("Empty payload received");
    return result;
  }

  // Heartbeat/status payload from firmware (4-byte counter, status byte, fixture count)
  if (bytes.length === 6 && (bytes[4] & 0xC0) === 0xC0) {
    var counter = readUint32BE(bytes, 0);
    var statusByte = bytes[4];
    var fixtureCount = bytes[5];
    result.data.heartbeat = {
      uplinkCounter: counter,
      statusByte: statusByte,
      isClassC: statusByte === 0xC5,
      dmxFixtures: fixtureCount
    };
    result.data.raw = bytesToHex(bytes);
    return result;
  }

  // Legacy structured message types (first byte is message type)
  var messageType = bytes[0];
  if (messageType === 0x01 && bytes.length >= 2) {
    result.data.status = bytes[1];
    result.data.message = "Status update";
    return result;
  }
  if (messageType === 0x02 && bytes.length >= 3) {
    result.data.sensorType = bytes[1];
    result.data.sensorValue = bytes[2];
    if (bytes.length > 3) {
      result.data.additionalData = bytes.slice(3);
    }
    return result;
  }
  if (messageType === 0x03 && bytes.length >= 2) {
    var numLights = bytes[1];
    result.data.dmxStatus = { numberOfLights: numLights };
    if (bytes.length >= 2 + numLights * 5) {
      result.data.lights = [];
      for (var i = 0; i < numLights; i++) {
        var start = 2 + i * 5;
        result.data.lights.push({
          address: bytes[start],
          channels: [bytes[start + 1], bytes[start + 2], bytes[start + 3], bytes[start + 4]]
        });
      }
    }
    return result;
  }

  // Attempt JSON parsing only if payload looks like printable ASCII JSON
  if (looksLikePrintableAscii(bytes)) {
    var str = String.fromCharCode.apply(null, bytes);
    var trimmed = str.trim();
    if (trimmed.startsWith("{") || trimmed.startsWith("[")) {
      try {
        result.data = JSON.parse(trimmed);
        return result;
      } catch (error) {
        result.data = {
          raw: bytesToHex(bytes),
          text: sanitizeString(str)
        };
        result.warnings.push("Failed to parse as JSON: " + error.message);
        return result;
      }
    }
    // Printable but not JSON - return as plain text
    result.data = {
      raw: bytesToHex(bytes),
      text: sanitizeString(str)
    };
    return result;
  }

  // Unknown binary format fallback
  result.data = {
    raw: bytesToHex(bytes),
    bytes: bytes
  };
  result.warnings.push("Unknown binary uplink format");
  return result;
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

function looksLikePrintableAscii(bytes) {
  for (var i = 0; i < bytes.length; i++) {
    var b = bytes[i];
    if (b === 0x09 || b === 0x0A || b === 0x0D) {
      continue; // allow common whitespace
    }
    if (b < 0x20 || b > 0x7E) {
      return false;
    }
  }
  return true;
}

function readUint32BE(bytes, offset) {
  return (
    (bytes[offset] * 0x1000000) +
    (bytes[offset + 1] << 16) +
    (bytes[offset + 2] << 8) +
    bytes[offset + 3]
  ) >>> 0;
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
  var bytes = input.bytes || [];
  var result = {
    data: {},
    warnings: [],
    errors: []
  };

  if (bytes.length === 0) {
    result.errors.push("Empty payload");
    return result;
  }

  // Config command [0xC0, numLights]
  if (bytes.length === 2 && bytes[0] === 0xC0) {
    result.data.config = { numLights: bytes[1] };
    return result;
  }

  // Pattern stop command [0xF0]
  if (bytes.length === 1 && bytes[0] === 0xF0) {
    result.data.pattern = { type: 'stop' };
    return result;
  }

  // Pattern command [0xF1, type, speedL, speedH, cyclesL, cyclesH]
  if (bytes.length === 6 && bytes[0] === 0xF1) {
    var patternType = ['colorFade', 'rainbow', 'strobe', 'chase', 'alternate'][bytes[1]] || 'colorFade';
    var speed = bytes[2] | (bytes[3] << 8);
    var cycles = bytes[4] | (bytes[5] << 8);
    result.data.pattern = {
      type: patternType,
      speed: speed,
      cycles: cycles
    };
    return result;
  }

  // Simple single-byte commands (0-4 colors, 0xAA test)
  if (bytes.length === 1) {
    var simpleCommands = {
      0x00: 'off',
      0x01: 'red',
      0x02: 'green',
      0x03: 'blue',
      0x04: 'white',
      0xAA: 'test'
    };
    if (simpleCommands.hasOwnProperty(bytes[0])) {
      result.data.command = simpleCommands[bytes[0]];
      return result;
    }
  }

  // ASCII "go" command
  if (bytes.length === 2 && bytes[0] === 0x67 && bytes[1] === 0x6F) {
    result.data.command = 'go';
    return result;
  }

  // Compact binary lights payload
  if (bytes.length >= 6 && ((bytes.length - 1) % 5 === 0)) {
    var lightCount = bytes[0];
    var expectedSize = 1 + lightCount * 5;
    if (lightCount > 0 && lightCount <= 25 && expectedSize === bytes.length) {
      var lights = [];
      for (var i = 0; i < lightCount; i++) {
        var offset = 1 + i * 5;
        lights.push({
          address: bytes[offset],
          channels: [
            bytes[offset + 1],
            bytes[offset + 2],
            bytes[offset + 3],
            bytes[offset + 4]
          ]
        });
      }
      result.data.lights = lights;
      return result;
    }
  }

  // Printable ASCII payloads (raw JSON commands, etc.)
  if (looksLikePrintableAscii(bytes)) {
    var str = String.fromCharCode.apply(null, bytes).trim();
    if (str) {
      try {
        result.data = JSON.parse(str);
        return result;
      } catch (err) {
        result.data = {
          raw: bytesToHex(bytes),
          text: sanitizeString(str)
        };
        result.warnings.push('Failed to parse as JSON: ' + err.message);
        return result;
      }
    }
  }

  // Fallback - unknown binary payload
  result.data = {
    raw: bytesToHex(bytes)
  };
  result.warnings.push('Unknown downlink format');
  return result;
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