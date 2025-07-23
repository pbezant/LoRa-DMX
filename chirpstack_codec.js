/**
 * Downlink encoder function (application to device)
 */
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
  
  // NEW: Direct raw byte array support
  if (input.data.raw && Array.isArray(input.data.raw)) {
    // Validate and convert to bytes
    var bytes = [];
    for (var i = 0; i < input.data.raw.length; i++) {
      var val = parseInt(input.data.raw[i], 10);
      if (isNaN(val) || val < 0 || val > 255) {
        // Invalid byte value, skip or return error
        continue;
      }
      bytes.push(val);
    }
    
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }

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
  
  // CASE 4: Pattern commands - COMPACT BINARY ENCODING
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
          // Special stop command
          return {
            bytes: [0xF0], // Special pattern stop command
            fPort: input.fPort || 1
          };
      }
    } 
    // Handle full pattern object with parameters
    else if (typeof input.data.pattern === 'object') {
      patternType = input.data.pattern.type || 'colorFade';
      speed = input.data.pattern.speed || 50;
      cycles = input.data.pattern.cycles || 5;
    }
    
    // Compact binary format for patterns:
    // Byte 0: 0xF1 (pattern command marker)
    // Byte 1: Pattern type (0=colorFade, 1=rainbow, 2=strobe, 3=chase, 4=alternate)
    // Byte 2: Speed (lower byte)
    // Byte 3: Speed (upper byte) 
    // Byte 4: Cycles (lower byte)
    // Byte 5: Cycles (upper byte)
    
    bytes.push(0xF1); // Pattern command marker
    
    // Map pattern type to number
    var typeNum = 0;
    switch (patternType) {
      case 'colorFade': typeNum = 0; break;
      case 'rainbow': typeNum = 1; break;
      case 'strobe': typeNum = 2; break;
      case 'chase': typeNum = 3; break;
      case 'alternate': typeNum = 4; break;
      default: typeNum = 0; break;
    }
    bytes.push(typeNum);
    
    // Add speed as 16-bit value (little endian)
    bytes.push(speed & 0xFF);        // Lower byte
    bytes.push((speed >> 8) & 0xFF); // Upper byte
    
    // Add cycles as 16-bit value (little endian)
    bytes.push(cycles & 0xFF);        // Lower byte
    bytes.push((cycles >> 8) & 0xFF); // Upper byte
    
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }
  
    // CASE 5: Lights JSON object - proper DMX control
    if (input.data.lights) {
      // START MODIFICATION FOR COMPACT BYTE ENCODING
  
      if (!Array.isArray(input.data.lights)) {
        // Handle error: 'lights' is not an array
        // You might want to log this or return an error object depending on your error handling strategy
        return {
          bytes: [],
          fPort: input.fPort || 1
        };
      }
  
      let bytes = [];
      bytes.push(input.data.lights.length); // First byte: number of lights
  
      for (let i = 0; i < input.data.lights.length; i++) {
        const light = input.data.lights[i];
  
        // Basic validation for light structure
        if (typeof light.address !== 'number' || !Array.isArray(light.channels) || light.channels.length !== 4) {
          // Handle error: invalid light structure
          console.error("Invalid light structure encountered:", light); // Log the error
          return {
            bytes: [], // Return empty bytes or throw an error to prevent malformed payload
            fPort: input.fPort || 1
          };
        }
  
        // Add address byte
        // Ensure address is within a byte range (0-255)
        bytes.push(light.address & 0xFF);
  
        // Add 4 channel bytes
        for (let j = 0; j < 4; j++) {
          // Ensure channel value is within a byte range (0-255)
          bytes.push(light.channels[j] & 0xFF);
        }
      }
  
      return {
        bytes: bytes,
        fPort: input.fPort || 1
      };
      // END MODIFICATION FOR COMPACT BYTE ENCODING
    }

  // CASE 6: Config downlink for number of lights
  if (input.data.config && typeof input.data.config.numLights === 'number') {
    var n = input.data.config.numLights;
    if (n < 1) n = 1;
    if (n > 25) n = 25;
    return {
      bytes: [0xC0, n],
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

/**
 * Uplink decoder function (device to application)
 */
function decodeUplink(input) {
  // Create a default response structure
  const result = {
    data: {},
    warnings: [],
    errors: []
  };

  try {
    // Check if we have any bytes to process
    if (!input.bytes || input.bytes.length === 0) {
      result.errors.push("Empty payload received");
      return result;
    }

    // Basic status message - first byte indicates message type
    const messageType = input.bytes[0];

    // Simple status message
    if (messageType === 0x01 && input.bytes.length >= 2) {
      result.data.status = input.bytes[1];
      result.data.message = "Status update received";
      return result;
    }

    // Sensor data message
    if (messageType === 0x02 && input.bytes.length >= 3) {
      result.data.sensorType = input.bytes[1];
      result.data.sensorValue = input.bytes[2];
      
      // Additional sensor data if available
      if (input.bytes.length > 3) {
        result.data.additionalData = input.bytes.slice(3);
      }
      
      return result;
    }

    // DMX status report
    if (messageType === 0x03 && input.bytes.length >= 2) {
      const numLights = input.bytes[1];
      result.data.dmxStatus = {
        numberOfLights: numLights
      };
      
      // If we have detailed light status info (for each light: address + 4 channels = 5 bytes)
      if (input.bytes.length >= 2 + (numLights * 5)) {
        result.data.lights = [];
        
        for (let i = 0; i < numLights; i++) {
          const startPos = 2 + (i * 5);
          const lightAddress = input.bytes[startPos];
          const channels = [
            input.bytes[startPos + 1],
            input.bytes[startPos + 2],
            input.bytes[startPos + 3],
            input.bytes[startPos + 4]
          ];
          
          result.data.lights.push({
            address: lightAddress,
            channels: channels
          });
        }
      }
      
      return result;
    }

    // If we can't identify the message type
    result.data.rawBytes = input.bytes;
    result.warnings.push("Unknown message format");
    
  } catch (error) {
    result.errors.push("Decoder error: " + error.message);
  }

  return result;
}