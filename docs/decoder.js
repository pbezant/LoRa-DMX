// TTN Payload Formatters for DMX LoRa Control System
// Copy and paste these functions into the TTN console under your application settings

// [ADDED FEATURE] The device now supports a config downlink ([0xC0, N]) to set the number of DMX lights at runtime (default 25). See README and technical.md for details.

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
    // CASE 1: Special command strings
    if (input.data.command === "go") {
      return {
        bytes: [0x67, 0x6F], // ASCII "go"
        fPort: input.fPort || 1
      };
    }
    
    // CASE 2: Special numeric commands
    if (input.data.command === "green" || input.data.command === 2) {
      return {
        bytes: [0x02], // Command code for green
        fPort: input.fPort || 1
      };
    }
    if (input.data.command === "red" || input.data.command === 1) {
      return {
        bytes: [0x01], // Command code for red
        fPort: input.fPort || 1
      };
    }
    if (input.data.command === "blue" || input.data.command === 3) {
      return {
        bytes: [0x03], // Command code for blue
        fPort: input.fPort || 1
      };
    }
    if (input.data.command === "white" || input.data.command === 4) {
      return {
        bytes: [0x04], // Command code for white
        fPort: input.fPort || 1
      };
    }
    if (input.data.command === "off" || input.data.command === 0) {
      return {
        bytes: [0x00], // Command code for off
        fPort: input.fPort || 1
      };
    }
    
    // CASE 3: Direct test mode
    if (input.data.command === "test") {
      return {
        bytes: [0xAA], // Special test trigger
        fPort: input.fPort || 1
      };
    }
    
    // CASE 4: Pattern commands
    if (input.data.pattern) {
      // Create a proper pattern command object
      var patternObj = {pattern: {}};
      
      // Handle direct pattern names as shortcuts
      if (typeof input.data.pattern === 'string') {
        patternObj.pattern.type = input.data.pattern;
        
        // Apply default values for each pattern type
        switch (input.data.pattern) {
          case 'colorFade':
            patternObj.pattern.speed = 50;
            patternObj.pattern.cycles = 5;
            break;
          case 'rainbow':
            patternObj.pattern.speed = 50;
            patternObj.pattern.cycles = 3;
            break;
          case 'strobe':
            patternObj.pattern.speed = 100;
            patternObj.pattern.cycles = 10;
            break;
          case 'chase':
            patternObj.pattern.speed = 200;
            patternObj.pattern.cycles = 3;
            break;
          case 'alternate':
            patternObj.pattern.speed = 300;
            patternObj.pattern.cycles = 5;
            break;
          case 'stop':
            // Just stop the current pattern
            patternObj.pattern.type = 'stop';
            break;
        }
      } 
      // Handle full pattern object with parameters
      else if (typeof input.data.pattern === 'object') {
        patternObj.pattern = input.data.pattern;
        
        // Ensure the type is specified
        if (!patternObj.pattern.type) {
          return {
            bytes: [],
            fPort: input.fPort || 1
          };
        }
      }
      
      // Convert to JSON string and then to bytes
      var jsonString = JSON.stringify(patternObj);
      var bytes = [];
      for (var i = 0; i < jsonString.length; i++) {
        bytes.push(jsonString.charCodeAt(i));
      }
      
      return {
        bytes: bytes,
        fPort: input.fPort || 1
      };
    }
    
    // CASE 5: Lights JSON object - proper DMX control
    if (input.data.lights) {
      // Convert the JSON object to a string
      var jsonString = JSON.stringify({lights: input.data.lights});
      
      // Convert the string to bytes
      var bytes = [];
      for (var i = 0; i < jsonString.length; i++) {
        bytes.push(jsonString.charCodeAt(i));
      }
      
      return {
        bytes: bytes,
        fPort: input.fPort || 1
      };
    }
    
    // Fallback - any other data is converted to a string and sent
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