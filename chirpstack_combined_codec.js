// ChirpStack Combined Codec for DMX-LoRa
// This codec handles both uplink decoding and downlink encoding for the DMX-LoRa controller
// Compatible with ChirpStack v3 and v4

/********* DECODE FUNCTIONS *********/

// Decode uplink payload from device - main function called by ChirpStack
function decodeUplink(input) {
  return {
    data: Decode(input.fPort, input.bytes)
  };
}

// Original decode function, now used by decodeUplink
function Decode(fPort, bytes) {
  // Convert bytes to a string to check if it's a JSON message
  var bytesString = bytesToString(bytes);
  
  // Try to parse as JSON first (used for status and response messages)
  try {
    var jsonObj = JSON.parse(bytesString);
    return processJsonMessage(jsonObj);
  } catch (e) {
    // If not valid JSON, process as binary data
    return processBinaryMessage(bytes);
  }
}

// Process messages in JSON format (device sends status updates as JSON)
function processJsonMessage(jsonObj) {
  // Initialize the decoded object
  var decoded = {};
  
  // Check for status message
  if (jsonObj.hasOwnProperty('status')) {
    decoded.messageType = 'status';
    decoded.status = jsonObj.status;
    
    // Add other status properties if they exist
    if (jsonObj.hasOwnProperty('dmx')) {
      decoded.dmxInitialized = jsonObj.dmx;
    }
    
    if (jsonObj.hasOwnProperty('class')) {
      decoded.deviceClass = jsonObj.class;
    }
    
    return decoded;
  }
  
  // Check for heartbeat message
  if (jsonObj.hasOwnProperty('hb')) {
    decoded.messageType = 'heartbeat';
    decoded.heartbeat = jsonObj.hb;
    
    if (jsonObj.hasOwnProperty('class')) {
      decoded.deviceClass = jsonObj.class;
    }
    
    return decoded;
  }
  
  // Check for ping response
  if (jsonObj.hasOwnProperty('ping_response')) {
    decoded.messageType = 'pingResponse';
    decoded.response = jsonObj.ping_response;
    
    if (jsonObj.hasOwnProperty('counter')) {
      decoded.counter = jsonObj.counter;
    }
    
    return decoded;
  }
  
  // If we can't determine the specific format, return the JSON as is
  decoded.messageType = 'unknownJson';
  decoded.rawData = jsonObj;
  return decoded;
}

// Process binary messages 
function processBinaryMessage(bytes) {
  // Initialize the decoded object
  var decoded = {
    messageType: 'binary',
    rawBytes: bytesToHexString(bytes),
    length: bytes.length
  };
  
  // If it's just a single byte, it might be a specific command or response
  if (bytes.length === 1) {
    decoded.value = bytes[0];
    
    // Map known single-byte values to their meanings
    switch(bytes[0]) {
      case 0:
        decoded.meaning = 'allOff';
        break;
      case 1:
        decoded.meaning = 'allRed';
        break;
      case 2:
        decoded.meaning = 'allGreen';
        break;
      case 3:
        decoded.meaning = 'allBlue';
        break;
      case 4:
        decoded.meaning = 'allWhite';
        break;
      case 170: // 0xAA
      case 255: // 0xFF
        decoded.meaning = 'testTrigger';
        break;
      default:
        decoded.meaning = 'unknown';
    }
  }
  
  return decoded;
}

/********* ENCODE FUNCTIONS *********/

// Encode downlink payload to device - main function called by ChirpStack
function encodeDownlink(input) {
  return {
    bytes: Encode(input.fPort, input.data)
  };
}

// Original encode function, now used by encodeDownlink
function Encode(fPort, obj) {
    // Handle format where obj might be just the command without nesting
    if (typeof obj === 'string') {
        // Simple text commands like "red", "green", etc.
        return simpleTextCommand(obj);
    }
    
    // Check for proper format
    if (!obj) {
        // Return empty payload if no object provided
        return [];
    }
    
    // Check the command type
    if (!obj.hasOwnProperty('cmd')) {
        // The object itself might be a valid direct command format
        if (obj.hasOwnProperty('lights')) {
            // This is already in the format our device expects
            return encodeJson(obj);
        }
        return encodeJson(obj); // If no cmd specified, encode the entire object as JSON
    }
    
    var cmd = obj.cmd;
    
    // Handle simple commands (single byte)
    if (cmd === 'off' || cmd === 'allOff') {
        return [0];
    } else if (cmd === 'red' || cmd === 'allRed') {
        return [1];
    } else if (cmd === 'green' || cmd === 'allGreen') {
        return [2];
    } else if (cmd === 'blue' || cmd === 'allBlue') {
        return [3];
    } else if (cmd === 'white' || cmd === 'allWhite') {
        return [4];
    } else if (cmd === 'test' || cmd === 'testTrigger') {
        return [170]; // 0xAA
    }
    
    // Handle pattern commands
    if (cmd === 'pattern') {
        return encodePatternCommand(obj);
    }
    
    // Handle lights (DMX fixture) commands
    if (cmd === 'lights') {
        return encodeLightsCommand(obj);
    }
    
    // Default: encode as JSON
    return encodeJson(obj);
}

// Process simple text commands like "red", "green", etc.
function simpleTextCommand(cmd) {
    if (cmd === 'off' || cmd === 'allOff') {
        return [0];
    } else if (cmd === 'red' || cmd === 'allRed') {
        return [1];
    } else if (cmd === 'green' || cmd === 'allGreen') {
        return [2];
    } else if (cmd === 'blue' || cmd === 'allBlue') {
        return [3];
    } else if (cmd === 'white' || cmd === 'allWhite') {
        return [4];
    } else if (cmd === 'test' || cmd === 'testTrigger') {
        return [170]; // 0xAA
    }
    
    // Default: try to encode the string as JSON
    try {
        // Check if it's a JSON string
        var obj = JSON.parse(cmd);
        return encodeJson(obj);
    } catch (e) {
        // Not JSON, return the string as bytes
        return stringToBytes(cmd);
    }
}

// Encode a pattern command
function encodePatternCommand(obj) {
    var patternObj = {};
    
    // Different format for test patterns vs regular patterns
    if (obj.hasOwnProperty('test') || 
        obj.type === 'rainbow' || 
        obj.type === 'strobe' || 
        obj.type === 'continuous' || 
        obj.type === 'ping') {
        
        patternObj.test = {
            pattern: obj.type || obj.pattern
        };
        
        // Add parameters if they exist
        if (obj.hasOwnProperty('cycles')) {
            patternObj.test.cycles = obj.cycles;
        }
        
        if (obj.hasOwnProperty('speed')) {
            patternObj.test.speed = obj.speed;
        }
        
        if (obj.hasOwnProperty('staggered')) {
            patternObj.test.staggered = obj.staggered;
        }
        
        if (obj.hasOwnProperty('enabled')) {
            patternObj.test.enabled = obj.enabled;
        }
        
        if (obj.hasOwnProperty('color')) {
            patternObj.test.color = obj.color;
        }
        
        if (obj.hasOwnProperty('count')) {
            patternObj.test.count = obj.count;
        }
        
        if (obj.hasOwnProperty('onTime')) {
            patternObj.test.onTime = obj.onTime;
        }
        
        if (obj.hasOwnProperty('offTime')) {
            patternObj.test.offTime = obj.offTime;
        }
        
        if (obj.hasOwnProperty('alternate')) {
            patternObj.test.alternate = obj.alternate;
        }
    } else {
        // Regular pattern format
        patternObj.pattern = {};
        
        // Set pattern type
        if (obj.hasOwnProperty('type')) {
            patternObj.pattern.type = obj.type;
        } else if (obj.hasOwnProperty('pattern')) {
            patternObj.pattern = obj.pattern;
        }
        
        // Add parameters if they exist
        if (obj.hasOwnProperty('speed')) {
            patternObj.pattern.speed = obj.speed;
        }
        
        if (obj.hasOwnProperty('cycles')) {
            patternObj.pattern.cycles = obj.cycles;
        }
        
        if (obj.hasOwnProperty('staggered')) {
            patternObj.pattern.staggered = obj.staggered;
        }
        
        if (obj.hasOwnProperty('enabled')) {
            patternObj.pattern.enabled = obj.enabled;
        }
    }
    
    // Convert the pattern object to JSON, then to bytes
    return encodeJson(patternObj);
}

// Encode a lights (DMX fixtures) command
function encodeLightsCommand(obj) {
    var lightsCommand = {
        lights: []
    };
    
    // If obj.fixtures is an array, use it
    if (obj.hasOwnProperty('fixtures') && Array.isArray(obj.fixtures)) {
        obj.fixtures.forEach(function(fixture) {
            var fixtureObj = {};
            
            if (fixture.hasOwnProperty('address')) {
                fixtureObj.address = fixture.address;
            }
            
            if (fixture.hasOwnProperty('channels')) {
                fixtureObj.channels = fixture.channels;
            } else if (fixture.hasOwnProperty('color')) {
                // Convert color to channels array
                var color = fixture.color;
                if (color === 'red') {
                    fixtureObj.channels = [255, 0, 0, 0];
                } else if (color === 'green') {
                    fixtureObj.channels = [0, 255, 0, 0];
                } else if (color === 'blue') {
                    fixtureObj.channels = [0, 0, 255, 0];
                } else if (color === 'white') {
                    fixtureObj.channels = [0, 0, 0, 255];
                } else if (color === 'off') {
                    fixtureObj.channels = [0, 0, 0, 0];
                } else if (color === 'yellow') {
                    fixtureObj.channels = [255, 255, 0, 0];
                } else if (color === 'purple') {
                    fixtureObj.channels = [255, 0, 255, 0];
                } else if (color === 'cyan') {
                    fixtureObj.channels = [0, 255, 255, 0];
                }
            } else if (fixture.hasOwnProperty('r') && fixture.hasOwnProperty('g') && fixture.hasOwnProperty('b')) {
                // RGB or RGBW values specified individually
                var r = fixture.r || 0;
                var g = fixture.g || 0;
                var b = fixture.b || 0;
                var w = fixture.hasOwnProperty('w') ? fixture.w : 0;
                fixtureObj.channels = [r, g, b, w];
            }
            
            lightsCommand.lights.push(fixtureObj);
        });
    }
    
    // Special case for same color on all fixtures
    if (obj.hasOwnProperty('allFixtures') && obj.hasOwnProperty('color')) {
        var addresses = Array.isArray(obj.allFixtures) ? obj.allFixtures : [1, 2, 3, 4]; // Default to first 4 fixtures
        var channels;
        
        // Set channel values based on color
        if (obj.color === 'red') {
            channels = [255, 0, 0, 0];
        } else if (obj.color === 'green') {
            channels = [0, 255, 0, 0];
        } else if (obj.color === 'blue') {
            channels = [0, 0, 255, 0];
        } else if (obj.color === 'white') {
            channels = [0, 0, 0, 255];
        } else if (obj.color === 'off') {
            channels = [0, 0, 0, 0];
        } else if (obj.color === 'yellow') {
            channels = [255, 255, 0, 0];
        } else if (obj.color === 'purple') {
            channels = [255, 0, 255, 0];
        } else if (obj.color === 'cyan') {
            channels = [0, 255, 255, 0];
        }
        
        // Create fixture objects for each address
        addresses.forEach(function(address) {
            lightsCommand.lights.push({
                address: address,
                channels: channels
            });
        });
    }
    
    // Convert the lights object to JSON, then to bytes
    return encodeJson(lightsCommand);
}

/********* HELPER FUNCTIONS *********/

// Helper function to convert bytes to string
function bytesToString(bytes) {
  var result = '';
  for (var i = 0; i < bytes.length; i++) {
    result += String.fromCharCode(bytes[i]);
  }
  return result;
}

// Helper function to convert string to bytes
function stringToBytes(str) {
  var bytes = [];
  for (var i = 0; i < str.length; i++) {
    bytes.push(str.charCodeAt(i));
  }
  return bytes;
}

// Helper function to convert bytes to hex string for display
function bytesToHexString(bytes) {
  var result = '';
  for (var i = 0; i < bytes.length; i++) {
    var hex = bytes[i].toString(16);
    result += (hex.length === 1 ? '0' + hex : hex) + ' ';
  }
  return result.trim();
}

// Helper function to encode an object as JSON
function encodeJson(obj) {
    var jsonString = JSON.stringify(obj);
    var bytes = [];
    
    for (var i = 0; i < jsonString.length; i++) {
        bytes.push(jsonString.charCodeAt(i));
    }
    
    return bytes;
} 