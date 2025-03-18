// TTN Payload Formatters for DMX LoRa Control System
// Copy and paste these functions into the TTN console under your application settings

// Uplink decoder function (device to application)
function decodeUplink(input) {
  try {
    // Convert the byte array to a string
    var jsonString = String.fromCharCode.apply(null, input.bytes);
    
    // Try to parse it as JSON
    var jsonData = JSON.parse(jsonString);
    
    // Validate the format based on the device's known message patterns
    if (jsonData.status) {
      // Status message
      if (jsonData.status === "online" && "dmx" in jsonData) {
        // Online status message with DMX state
        return {
          data: {
            status: jsonData.status,
            dmx: !!jsonData.dmx, // Ensure boolean
            decoded_payload: jsonString // Include the original for reference
          },
          warnings: [],
          errors: []
        };
      } else if (jsonData.status === "alive" && "uptime" in jsonData) {
        // Alive message with uptime
        return {
          data: {
            status: jsonData.status,
            uptime: parseInt(jsonData.uptime, 10), // Ensure number
            uptime_formatted: formatUptime(jsonData.uptime),
            decoded_payload: jsonString // Include the original for reference
          },
          warnings: [],
          errors: []
        };
      }
    }
    
    // If we get here, the JSON is valid but doesn't match expected patterns
    // Return the parsed JSON data anyway as it might be a custom message
    return {
      data: {
        ...jsonData,
        decoded_payload: jsonString
      },
      warnings: ["Message format doesn't match expected patterns, but JSON is valid"],
      errors: []
    };
  } catch (error) {
    // If we can't parse as JSON, return the raw data as a string
    return {
      data: {
        raw: bytesToHex(input.bytes),
        text: sanitizeString(String.fromCharCode.apply(null, input.bytes))
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
    hex.push((bytes[i] < 16 ? "0" : "") + bytes[i].toString(16));
  }
  return hex.join("");
}

// Helper function to sanitize strings, removing control characters
function sanitizeString(str) {
  return str.replace(/[\x00-\x1F\x7F-\x9F]/g, '');
}

// Downlink encoder function (application to device)
function encodeDownlink(input) {
  try {
    // Convert input to JSON string if it's not already
    var jsonString;
    if (typeof input.data === 'string') {
      jsonString = input.data;
    } else {
      jsonString = JSON.stringify(input.data);
    }
    
    // Parse and validate the JSON
    var jsonData = JSON.parse(jsonString);
    
    // Check if it's a test pattern command
    if (jsonData.test && typeof jsonData.test === 'object') {
      if (!jsonData.test.pattern) {
        return {
          bytes: [],
          warnings: [],
          errors: ["Invalid test pattern format. 'pattern' field is required."]
        };
      }
      
      // Convert the JSON string directly to bytes
      var bytes = [];
      for (var i = 0; i < jsonString.length; i++) {
        bytes.push(jsonString.charCodeAt(i));
      }
      
      return {
        bytes: bytes,
        fPort: input.fPort || 1,
        warnings: [],
        errors: []
      };
    }
    
    // Check if it's a DMX control message
    if (jsonData.lights && Array.isArray(jsonData.lights)) {
      // Validate each light has the correct format
      for (var i = 0; i < jsonData.lights.length; i++) {
        var light = jsonData.lights[i];
        if (!light.address || !light.channels || !Array.isArray(light.channels)) {
          return {
            bytes: [],
            warnings: [],
            errors: ["Invalid light format at index " + i + ". Each light must have 'address' and 'channels' array."]
          };
        }
      }
      
      // Convert the JSON string directly to bytes
      var bytes = [];
      for (var i = 0; i < jsonString.length; i++) {
        bytes.push(jsonString.charCodeAt(i));
      }
      
      return {
        bytes: bytes,
        fPort: input.fPort || 1,
        warnings: [],
        errors: []
      };
    }
    
    // If we get here, the message format is not recognized
    return {
      bytes: [],
      warnings: [],
      errors: ["Invalid message format. Must be either a DMX control message or a test pattern command."]
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