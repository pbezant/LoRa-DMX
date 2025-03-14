// TTN Payload Formatters for DMX LoRa Control System
// Copy and paste these functions into the TTN console under your application settings

// Uplink decoder function (device to application)
function decodeUplink(input) {
  try {
    // Convert the byte array to a string
    var jsonString = String.fromCharCode.apply(null, input.bytes);
    
    // Try to parse it as JSON
    var jsonData = JSON.parse(jsonString);
    
    // Return the parsed JSON data
    return {
      data: jsonData,
      warnings: [],
      errors: []
    };
  } catch (error) {
    // If we can't parse as JSON, return the raw data as a string
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

// Helper function to convert bytes to hex for display
function bytesToHex(bytes) {
  var hex = [];
  for (var i = 0; i < bytes.length; i++) {
    hex.push((bytes[i] < 16 ? "0" : "") + bytes[i].toString(16));
  }
  return hex.join("");
}

// Downlink encoder function (application to device)
function encodeDownlink(input) {
  try {
    // If input is already a string, use it directly
    var jsonString;
    if (typeof input.data === 'string') {
      jsonString = input.data;
    } else {
      // Otherwise, convert to JSON string
      jsonString = JSON.stringify(input.data);
    }
    
    // Convert JSON string to byte array
    var bytes = [];
    for (var i = 0; i < jsonString.length; i++) {
      bytes.push(jsonString.charCodeAt(i));
    }
    
    return {
      bytes: bytes,
      fPort: input.fPort || 1, // Use provided port or default to 1
      warnings: [],
      errors: []
    };
  } catch (error) {
    return {
      bytes: [],
      warnings: [],
      errors: ["Failed to encode downlink: " + error]
    };
  }
}

// Downlink decoder function (for debugging in console)
function decodeDownlink(input) {
  try {
    // Convert byte array to string
    var jsonString = String.fromCharCode.apply(null, input.bytes);
    
    // Try to parse as JSON for validation
    JSON.parse(jsonString);
    
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
        text: String.fromCharCode.apply(null, input.bytes)
      },
      warnings: ["Failed to validate as JSON: " + error],
      errors: []
    };
  }
}

// Example usage:
// 
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
// Uplink status messages from the device will be in this format:
// {
//   "status": "online",
//   "dmx": true
// }
// 
// or
//
// {
//   "status": "alive",
//   "uptime": 3600
// } 