/**
 * DMX LoRa Control System for Heltec LoRa 32 V3
 * 
 * Receives JSON commands over The Things Network (TTN) and translates them
 * into DMX signals to control multiple DMX lighting fixtures.
 * 
 * Created for US915 frequency plan (United States)
 * Uses OTAA activation for TTN
 * 
 * Supported JSON Commands:
 * 
 * 1. Direct DMX Control:
 * {
 *   "lights": [
 *     {
 *       "address": 1,
 *       "channels": [255, 0, 128, 0]
 *     },
 *     {
 *       "address": 5,
 *       "channels": [255, 255, 100, 0]
 *     }
 *   ]
 * }
 * 
 * 2. Rainbow Chase Test Pattern:
 * {
 *   "test": {
 *     "pattern": "rainbow",
 *     "cycles": 3,        // Optional: number of cycles (1-10)
 *     "speed": 50,        // Optional: ms between updates (10-500)
 *     "staggered": true   // Optional: create chase effect across fixtures
 *   }
 * }
 * 
 * 3. Strobe Test Pattern:
 * {
 *   "test": {
 *     "pattern": "strobe", 
 *     "color": 1,         // Optional: 0=white, 1=red, 2=green, 3=blue
 *     "count": 20,        // Optional: number of flashes (1-100)
 *     "onTime": 50,       // Optional: ms for on phase (10-1000)
 *     "offTime": 50,      // Optional: ms for off phase (10-1000)
 *     "alternate": false  // Optional: alternate between fixtures
 *   }
 * }
 * 
 * 4. Continuous Rainbow Mode:
 * {
 *   "test": {
 *     "pattern": "continuous",
 *     "enabled": true,    // Optional: true to enable, false to disable
 *     "speed": 30,        // Optional: ms between updates (5-500)
 *     "staggered": true   // Optional: create chase effect across fixtures
 *   }
 * }
 * 
 * 5. Ping Test (for testing downlink communication):
 * {
 *   "test": {
 *     "pattern": "ping"
 *   }
 * }
 * 
 * Libraries:
 * - LoRaManager: Custom LoRaWAN communication via RadioLib
 * - ArduinoJson: JSON parsing
 * - DmxController: DMX output control
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>  // Include SPI library explicitly
#include "LoRaManager.h"
#include "DmxController.h"
#include <esp_task_wdt.h>  // Watchdog

// Debug output
#define SERIAL_BAUD 115200

// Diagnostic LED pin
#define LED_PIN 35  // Built-in LED on Heltec LoRa 32 V3

// DMX configuration for Heltec LoRa 32 V3
#define DMX_PORT 1
#define DMX_TX_PIN 19  // TX pin for DMX
#define DMX_RX_PIN 20  // RX pin for DMX
#define DMX_DIR_PIN 5  // DIR pin for DMX (connect to both DE and RE on MAX485)

// LoRaWAN TTN Connection Parameters
#define LORA_CS_PIN   8     // Corrected CS pin for Heltec LoRa 32 V3
#define LORA_DIO1_PIN 14    // DIO1 pin
#define LORA_RESET_PIN 12   // Reset pin  
#define LORA_BUSY_PIN 13    // Busy pin

// SPI pins for Heltec LoRa 32 V3
#define LORA_SPI_SCK  9   // SPI clock
#define LORA_SPI_MISO 11  // SPI MISO
#define LORA_SPI_MOSI 10  // SPI MOSI

// LoRaWAN Credentials (can be changed by the user)
uint64_t joinEUI = 0x0000000000000001; // 0000000000000001
uint64_t devEUI = 0x70B3D57ED80041B2;  // 70B3D57ED80041B2
uint8_t appKey[] = {0x45, 0xD3, 0x7B, 0xF3, 0x77, 0x61, 0xA6, 0x1F, 0x9F, 0x07, 0x1F, 0xE1, 0x6D, 0x4F, 0x57, 0x77}; // 45D37BF37761A61F9F071FE16D4F5777
uint8_t nwkKey[] = {0x45, 0xD3, 0x7B, 0xF3, 0x77, 0x61, 0xA6, 0x1F, 0x9F, 0x07, 0x1F, 0xE1, 0x6D, 0x4F, 0x57, 0x77}; // Same as appKey for OTAA

// DMX configuration - we'll use dynamic configuration from JSON
#define MAX_FIXTURES 32           // Maximum number of fixtures supported
#define MAX_CHANNELS_PER_FIXTURE 16 // Maximum channels per fixture
#define MAX_JSON_SIZE 1024        // Maximum size of JSON document

// Global variables
bool dmxInitialized = false;
bool loraInitialized = false;
DmxController* dmx = NULL;
LoRaManager* lora = NULL;

// Forward declaration of the callback function
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port);

// Placeholder for the received data
uint8_t receivedData[MAX_JSON_SIZE];
size_t receivedDataSize = 0;
uint8_t receivedPort = 0;
bool dataReceived = false;  // Legacy flag - using direct callback processing now

// Global variables for continuous rainbow effect
bool runningRainbowDemo = false;  // Controls continuous rainbow effect
unsigned long lastRainbowStep = 0; // Timestamp for last rainbow step
uint32_t rainbowStepCounter = 0;  // Step counter for rainbow effect
int rainbowStepDelay = 30;        // Delay between steps in milliseconds
bool rainbowStaggered = true;     // Whether to stagger colors across fixtures

// Always process in callback for maximum reliability
bool processInCallback = true; // Set to true to process commands immediately in callback

// Set watchdog timeout to 30 seconds
#define WDT_TIMEOUT 30

// Add DMX diagnostic function
void printDmxValues(int startAddr, int numChannels) {
  if (!dmxInitialized || dmx == NULL) {
    Serial.println("DMX not initialized, cannot print values");
    return;
  }
  
  Serial.print("DMX values from address ");
  Serial.print(startAddr);
  Serial.print(" to ");
  Serial.print(startAddr + numChannels - 1);
  Serial.println(":");
  
  for (int i = 0; i < numChannels; i++) {
    Serial.print("CH ");
    Serial.print(startAddr + i);
    Serial.print(": ");
    Serial.print(dmx->getDmxData()[startAddr + i]);
    Serial.print("  ");
    if ((i + 1) % 8 == 0) Serial.println();
  }
  Serial.println();
}

// Improved JSON light processing
bool processLightsJson(JsonArray lightsArray) {
  if (!dmxInitialized || dmx == NULL) {
    Serial.println("DMX not initialized, cannot process lights array");
    return false;
  }
  
  Serial.println("\n===== PROCESSING DOWNLINK LIGHTS COMMAND =====");
  Serial.println("Starting DMX values:");
  // Print current values for first 20 channels
  printDmxValues(1, 20);
  
  bool atLeastOneValid = false;
  
  // Iterate through each light in the array
  for (JsonObject light : lightsArray) {
    // Check if the light has an address field
    if (!light.containsKey("address")) {
      Serial.println("Light missing 'address' field, skipping");
      continue;
    }
    
    // Get the DMX address
    int address = light["address"].as<int>();
    
    // Check if address is valid
    if (address < 1 || address > 512) {
      Serial.print("Invalid DMX address: ");
      Serial.println(address);
      continue;
    }
    
    // Check if the light has a channels array
    if (!light.containsKey("channels")) {
      Serial.println("Light missing 'channels' array, skipping");
      continue;
    }
    
    // Get the channels array
    JsonArray channelsArray = light["channels"];
    
    // Check if it's not empty
    if (channelsArray.size() == 0) {
      Serial.println("Empty channels array, skipping");
      continue;
    }
    
    Serial.print("Setting light at address ");
    Serial.print(address);
    Serial.print(" with ");
    Serial.print(channelsArray.size());
    Serial.println(" channels:");
    
    // Set the channels
    int channelIndex = 0;
    for (JsonVariant channelValue : channelsArray) {
      int value = channelValue.as<int>();
      
      // Validate channel value (0-255)
      value = max(0, min(value, 255));
      
      // DMX channels are 1-based
      int dmxChannel = address + channelIndex;
      
      // Log the channel being set
      Serial.print("  Channel ");
      Serial.print(dmxChannel);
      Serial.print(" = ");
      Serial.println(value);
      
      // Don't exceed DMX_PACKET_SIZE
      if (dmxChannel < DMX_PACKET_SIZE) {
        dmx->getDmxData()[dmxChannel] = value;
        channelIndex++;
      } else {
        Serial.print("DMX channel out of range: ");
        Serial.println(dmxChannel);
        break;
      }
    }
    
    // At least one light was processed successfully
    atLeastOneValid = true;
    
    // Print debug info
    Serial.print("Set DMX address ");
    Serial.print(address);
    Serial.print(" to values: [");
    for (int i = 0; i < channelsArray.size(); i++) {
      if (i > 0) Serial.print(", ");
      Serial.print(dmx->getDmxData()[address + i]);
    }
    Serial.println("]");
  }
  
  // Send data if at least one light was valid
  if (atLeastOneValid) {
    Serial.println("Sending updated DMX values to fixtures...");
    
    // Print final values for verification
    Serial.println("Final DMX values being sent:");
    printDmxValues(1, 20);
    
    dmx->sendData();
    
    // Save settings to persistent storage
    dmx->saveSettings();
    Serial.println("DMX settings saved to persistent storage");
    
    return true;
  }
  
  return false;
}

/**
 * Process JSON payload and control DMX fixtures
 * 
 * Expected JSON format:
 * {
 *   "lights": [
 *     {
 *       "address": 1,
 *       "channels": [255, 0, 128, 0]
 *     },
 *     {
 *       "address": 5,
 *       "channels": [255, 255, 100, 0]
 *     }
 *   ]
 * }
 * 
 * Or for test patterns:
 * {
 *   "test": {
 *     "pattern": "rainbow",
 *     "cycles": 3,
 *     "speed": 50,
 *     "staggered": true
 *   }
 * }
 * 
 * Or:
 * {
 *   "test": {
 *     "pattern": "strobe",
 *     "color": 1,
 *     "count": 20,
 *     "onTime": 50,
 *     "offTime": 50,
 *     "alternate": false
 *   }
 * }
 * 
 * @param jsonString The JSON string to process
 * @return true if processing was successful, false otherwise
 */
bool processJsonPayload(const String& jsonString) {
  Serial.println("Processing JSON payload: " + jsonString);
  
  // Check for special test commands (non-JSON) first
  if (jsonString.length() <= 4 && jsonString.length() > 0) {
    // Try to parse as a simple test command (numeric)
    int testCmd = 0;
    bool isTestCmd = true;
    
    for (size_t i = 0; i < jsonString.length(); i++) {
      if (isdigit(jsonString[i])) {
        testCmd = testCmd * 10 + (jsonString[i] - '0');
      } else {
        isTestCmd = false;
        break;
      }
    }
    
    if (isTestCmd) {
      Serial.print("Detected numeric test command: ");
      Serial.println(testCmd);
      
      // Handle different test codes
      switch (testCmd) {
        case 1: // Set all fixtures to RED
          Serial.println("TEST 1: Setting all fixtures to RED");
          for (int i = 0; i < dmx->getNumFixtures(); i++) {
            dmx->setFixtureColor(i, 255, 0, 0, 0);
          }
          dmx->sendData();
          dmx->saveSettings();
          return true;
          
        case 2: // Set all fixtures to GREEN
          Serial.println("TEST 2: Setting all fixtures to GREEN");
          for (int i = 0; i < dmx->getNumFixtures(); i++) {
            dmx->setFixtureColor(i, 0, 255, 0, 0);
          }
          dmx->sendData();
          dmx->saveSettings();
          return true;
          
        case 3: // Set all fixtures to BLUE
          Serial.println("TEST 3: Setting all fixtures to BLUE");
          for (int i = 0; i < dmx->getNumFixtures(); i++) {
            dmx->setFixtureColor(i, 0, 0, 255, 0);
          }
          dmx->sendData();
          dmx->saveSettings();
          return true;
          
        case 4: // Set all fixtures to WHITE
          Serial.println("TEST 4: Setting all fixtures to WHITE");
          for (int i = 0; i < dmx->getNumFixtures(); i++) {
            dmx->setFixtureColor(i, 0, 0, 0, 255);
          }
          dmx->sendData();
          dmx->saveSettings();
          return true;
          
        case 0: // Turn all fixtures OFF
          Serial.println("TEST 0: Turning all fixtures OFF");
          dmx->clearAllChannels();
          dmx->sendData();
          dmx->saveSettings();
          return true;
          
        default:
          Serial.println("Unknown test command, ignoring");
          break;
      }
    }
  }
  
  // Continue with normal JSON processing
  // Create a JSON document
  StaticJsonDocument<MAX_JSON_SIZE> doc;
  
  // Parse the JSON string
  DeserializationError error = deserializeJson(doc, jsonString);
  
  // Check for parsing errors
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    
    // Try to debug the JSON string
    Serial.print("JSON string length: ");
    Serial.println(jsonString.length());
    Serial.print("First 32 bytes: ");
    for (int i = 0; i < min(32, (int)jsonString.length()); i++) {
      Serial.print(jsonString[i]);
    }
    Serial.println();
    
    // Print hex for debugging
    Serial.print("Hex: ");
    for (int i = 0; i < min(32, (int)jsonString.length()); i++) {
      if (jsonString[i] < 16) Serial.print("0");
      Serial.print(jsonString[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    return false;
  }
  
  // Check if this is a test pattern command
  if (doc.containsKey("test")) {
    // Get the test object
    JsonObject testObj = doc["test"];
    
    // Check if the test object has a pattern field
    if (!testObj.containsKey("pattern")) {
      Serial.println("JSON format error: 'pattern' field not found in test object");
      return false;
    }
    
    // Get the pattern type
    String pattern = testObj["pattern"].as<String>();
    pattern.toLowerCase(); // Convert to lowercase for case-insensitive comparison
    
    Serial.print("Processing test pattern: ");
    Serial.println(pattern);
    
    // Process based on pattern type
    if (pattern == "rainbow") {
      // Get parameters with defaults if not specified
      int cycles = testObj.containsKey("cycles") ? testObj["cycles"].as<int>() : 3;
      int speed = testObj.containsKey("speed") ? testObj["speed"].as<int>() : 50;
      bool staggered = testObj.containsKey("staggered") ? testObj["staggered"].as<bool>() : true;
      
      // Validate parameters
      cycles = max(1, min(cycles, 10)); // Limit cycles between 1 and 10
      speed = max(10, min(speed, 500)); // Limit speed between 10ms and 500ms
      
      Serial.println("Executing rainbow chase pattern via downlink command");
      Serial.print("Cycles: ");
      Serial.print(cycles);
      Serial.print(", Speed: ");
      Serial.print(speed);
      Serial.print("ms, Staggered: ");
      Serial.println(staggered ? "Yes" : "No");
      
      // Configure test fixtures if none exist
      if (dmx->getNumFixtures() == 0) {
        Serial.println("Setting up default test fixtures for rainbow pattern");
        // Initialize 4 test fixtures with 4 channels each (RGBW)
        dmx->initializeFixtures(4, 4);
        
        // Configure fixtures with sequential DMX addresses
        dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
        dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
        dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
      }
      
      // Run the rainbow chase pattern
      dmx->runRainbowChase(cycles, speed, staggered);
      
      // Save the final state after the pattern completes
      dmx->saveSettings();
      
      return true;
    } 
    else if (pattern == "strobe") {
      // Get parameters with defaults if not specified
      int color = testObj.containsKey("color") ? testObj["color"].as<int>() : 0;
      int count = testObj.containsKey("count") ? testObj["count"].as<int>() : 20;
      int onTime = testObj.containsKey("onTime") ? testObj["onTime"].as<int>() : 50;
      int offTime = testObj.containsKey("offTime") ? testObj["offTime"].as<int>() : 50;
      bool alternate = testObj.containsKey("alternate") ? testObj["alternate"].as<bool>() : false;
      
      // Validate parameters
      color = max(0, min(color, 3)); // Limit color between 0 and 3
      count = max(1, min(count, 100)); // Limit count between 1 and 100
      onTime = max(10, min(onTime, 1000)); // Limit onTime between 10ms and 1000ms
      offTime = max(10, min(offTime, 1000)); // Limit offTime between 10ms and 1000ms
      
      Serial.println("Executing strobe test pattern via downlink command");
      Serial.print("Color: ");
      Serial.print(color);
      Serial.print(", Count: ");
      Serial.print(count);
      Serial.print(", On Time: ");
      Serial.print(onTime);
      Serial.print("ms, Off Time: ");
      Serial.print(offTime);
      Serial.print("ms, Alternate: ");
      Serial.println(alternate ? "Yes" : "No");
      
      // Configure test fixtures if none exist
      if (dmx->getNumFixtures() == 0) {
        Serial.println("Setting up default test fixtures for strobe pattern");
        // Initialize 4 test fixtures with 4 channels each (RGBW)
        dmx->initializeFixtures(4, 4);
        
        // Configure fixtures with sequential DMX addresses
        dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
        dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
        dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
      }
      
      // Run the strobe test pattern
      dmx->runStrobeTest(color, count, onTime, offTime, alternate);
      
      // Save the final state after the pattern completes
      dmx->saveSettings();
      
      return true;
    }
    else if (pattern == "continuous") {
      // This pattern controls the continuous rainbow effect in the main loop
      
      // Get parameters with defaults if not specified
      bool enabled = testObj.containsKey("enabled") ? testObj["enabled"].as<bool>() : false;
      int speed = testObj.containsKey("speed") ? testObj["speed"].as<int>() : 30;
      bool staggered = testObj.containsKey("staggered") ? testObj["staggered"].as<bool>() : true;
      
      // Validate parameters
      speed = max(5, min(speed, 500)); // Limit speed between 5ms and 500ms
      
      // Set the continuous rainbow mode
      runningRainbowDemo = enabled;
      rainbowStepDelay = speed;
      rainbowStaggered = staggered;
      
      Serial.print("Continuous rainbow mode: ");
      Serial.print(enabled ? "ENABLED" : "DISABLED");
      Serial.print(", Speed: ");
      Serial.print(speed);
      Serial.print("ms, Staggered: ");
      Serial.println(staggered ? "Yes" : "No");
      
      // Configure test fixtures if none exist
      if (dmx->getNumFixtures() == 0) {
        Serial.println("Setting up default test fixtures for continuous rainbow");
        // Initialize 4 test fixtures with 4 channels each (RGBW)
        dmx->initializeFixtures(4, 4);
        
        // Configure fixtures with sequential DMX addresses
        dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
        dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
        dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
      }
      
      if (enabled) {
        // Note: We don't save settings here as they'll continuously change
        // They'll be saved when the mode is disabled
      } else {
        runningRainbowDemo = false;
        Serial.println("Continuous rainbow mode disabled");
        
        // Save the final state when the continuous mode is disabled
        dmx->saveSettings();
      }
      
      return true;
    }
    else if (pattern == "ping") {
      // Simple ping command for testing downlink connectivity
      Serial.println("=== PING RECEIVED ===");
      Serial.println("Downlink communication is working!");
      
      // Blink the LED in a distinctive pattern to indicate ping received
      for (int i = 0; i < 3; i++) {
        DmxController::blinkLED(LED_PIN, 3, 100);
        delay(500);
      }
      
      // Send a ping response uplink
      if (loraInitialized && lora != NULL) {
        String response = "{\"ping_response\":\"ok\"}";
        if (lora->sendString(response, 1, true)) {
          Serial.println("Ping response sent (confirmed)");
        }
      }
      
      return true;
    }
    else {
      Serial.print("Unknown test pattern: ");
      Serial.println(pattern);
      return false;
    }
  }
  // Check if this is a direct light control command
  else if (doc.containsKey("lights")) {
    // Get the lights array
    JsonArray lightsArray = doc["lights"];
    
    // Process the lights array
    bool success = processLightsJson(lightsArray);
    
    // If processing was successful, send the data
    if (success) {
      // Save settings to persistent storage (already called in processLightsJson)
      return true;
    } else {
      Serial.println("Failed to process lights array");
      return false;
    }
  }
  else {
    Serial.println("JSON format error: missing 'lights' or 'test' object");
    return false;
  }
}

/**
 * Callback function for receiving downlink data from LoRaWAN
 * This function will be called by the LoRaManager when data is received
 * 
 * @param payload The payload data
 * @param size The size of the payload
 * @param port The port on which the data was received
 */
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port) {
  // Enhanced logging
  static uint32_t downlinkCounter = 0;
  downlinkCounter++;
  
  Serial.print("\n\n=== DOWNLINK #");
  Serial.print(downlinkCounter);
  Serial.println(" RECEIVED ===");
  Serial.print("Port: ");
  Serial.print(port);
  Serial.print(", Size: ");
  Serial.println(size);
  
  if (size <= MAX_JSON_SIZE) {
    // Visual indication of downlink reception
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    
    // Print detailed hex dump of all received bytes for debugging
    Serial.println("Raw bytes (hex):");
    for (size_t i = 0; i < size; i++) {
      if (i % 16 == 0) {
        if (i > 0) Serial.println();
        Serial.print(i, HEX);
        Serial.print(": ");
      }
      
      if (payload[i] < 16) Serial.print("0");
      Serial.print(payload[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    // Create a string from the payload for JSON parsing
    String payloadStr = "";
    for (size_t i = 0; i < size; i++) {
      payloadStr += (char)payload[i];
    }
    
    // Debug: Log the received data
    Serial.print("Downlink payload: ");
    Serial.println(payloadStr);
    
    // Verify json format is correct
    if (payloadStr.indexOf("{") != 0 || payloadStr.indexOf("}") != payloadStr.length() - 1) {
      Serial.println("WARNING: Payload doesn't appear to be valid JSON format");
      Serial.println("Attempting to process anyway...");
    }
    
    // Always process the command directly in the callback
    if (dmxInitialized) {
      Serial.println("Processing downlink command immediately");
      
      // Process the JSON payload
      bool success = processJsonPayload(payloadStr);
      
      if (success) {
        Serial.println("Successfully processed downlink");
        // Blink LED to indicate successful processing
        DmxController::blinkLED(LED_PIN, 2, 200);
        
        // Send a confirmation uplink if this is a ping
        if (payloadStr.indexOf("\"ping\"") > 0) {
          Serial.println("Sending ping response");
          // Send a ping response confirmation uplink
          if (loraInitialized && lora != NULL) {
            String response = "{\"ping_response\":\"ok\",\"counter\":" + String(downlinkCounter) + "}";
            if (lora->sendString(response, 1, true)) {
              Serial.println("Ping response sent (confirmed)");
            }
          }
        }
      } else {
        Serial.println("Failed to process downlink");
        // Blink LED rapidly to indicate error
        DmxController::blinkLED(LED_PIN, 5, 100);
      }
    } else {
      Serial.println("ERROR: DMX not initialized, cannot process command");
    }
    
    // For backward compatibility, also store in buffer
    // This is no longer needed but kept for debugging
    memcpy(receivedData, payload, size);
    receivedDataSize = size;
    receivedPort = port;
    
    // But don't set dataReceived flag - we already processed it
    dataReceived = false;
  } else {
    Serial.println("ERROR: Received payload exceeds buffer size");
  }
  
  // Debug current memory state
  Serial.print("Free heap after downlink: ");
  Serial.println(ESP.getFreeHeap());
}

/**
 * Handle incoming downlink data from LoRaWAN
 * 
 * @param payload The payload data
 * @param size The size of the payload
 * @param port The port on which the data was received
 */
void handleDownlink(uint8_t* payload, size_t size, uint8_t port) {
  Serial.print("Received downlink on port ");
  Serial.print(port);
  Serial.print(", size: ");
  Serial.println(size);
  
  // Convert payload to string
  String payloadStr = "";
  for (size_t i = 0; i < size; i++) {
    payloadStr += (char)payload[i];
  }
  
  // Process the payload as JSON
  if (dmxInitialized) {
    bool success = processJsonPayload(payloadStr);
    if (success) {
      // Blink LED to indicate successful processing
      DmxController::blinkLED(LED_PIN, 2, 200);
    } else {
      // Blink LED rapidly to indicate error
      DmxController::blinkLED(LED_PIN, 5, 100);
    }
  } else {
    Serial.println("DMX not initialized, cannot process payload");
    // Blink LED rapidly to indicate error
    DmxController::blinkLED(LED_PIN, 5, 100);
  }
}

void setup() {
  // Initialize Serial for debugging
  Serial.begin(SERIAL_BAUD);
  Serial.println("\n\n=== DMX LoRa Controller ===");
  
  // Initialize diagnostic LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("Starting setup sequence...");
  Serial.print("Free heap at start: ");
  Serial.println(ESP.getFreeHeap());
  
  // Initialize ESP32 Watchdog Timer
  Serial.println("Initializing watchdog timer...");
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL); // Add current task to WDT watch
  Serial.print("Watchdog timeout set to ");
  Serial.print(WDT_TIMEOUT);
  Serial.println(" seconds");
  
  // Initialize LoRaWAN
  try {
    Serial.println("\nInitializing LoRaWAN...");
    lora = new LoRaManager(US915, 2); // US915 band, subband 2
    
    // Set callback for handling downlinks
    lora->setDownlinkCallback(handleDownlinkCallback);
    
    // Initialize LoRaWAN with the provided credentials
    if (lora->begin(LORA_CS_PIN, LORA_DIO1_PIN, LORA_RESET_PIN, LORA_BUSY_PIN)) {
      loraInitialized = true;
      Serial.println("LoRaWAN initialized successfully!");
      
      // Set the credentials
      lora->setCredentials(joinEUI, devEUI, appKey, nwkKey);
      
      // Join the network
      Serial.println("Attempting to join the LoRaWAN network...");
      if (lora->joinNetwork()) {
        Serial.println("Successfully joined the network!");
      } else {
        Serial.println("Failed to join network, will continue attempts in background");
      }
    } else {
      Serial.println("Failed to initialize LoRaWAN!");
      loraInitialized = false;
    }
  } catch (...) {
    Serial.println("ERROR: Exception during LoRaWAN initialization!");
    lora = NULL;
    loraInitialized = false;
    DmxController::blinkLED(LED_PIN, 5, 100);  // Error indicator
  }
  
  // Initialize DMX
  Serial.println("\nInitializing DMX controller...");
  
  try {
    dmx = new DmxController(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_DIR_PIN);
    Serial.println("DMX controller object created");
    
    // Initialize DMX
    dmx->begin();
    dmxInitialized = true;
    Serial.println("DMX controller initialized successfully!");
    
    // Set initial values to zero and send to make sure fixtures are clear
    dmx->clearAllChannels();
    dmx->sendData();
    Serial.println("DMX channels cleared");

    // Setup test fixtures
    Serial.println("Setting up default test fixtures for testing");
    // Initialize 4 test fixtures with 4 channels each (RGBW)
    dmx->initializeFixtures(4, 4);
    
    // Configure fixtures with sequential DMX addresses
    dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
    dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
    dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
    dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
    
    // Print fixture configurations for verification
    dmx->printFixtureValues();
    
    // Set all fixtures to WHITE on device reset
    Serial.println("\n=== SETTING ALL FIXTURES TO WHITE ON STARTUP ===");
    for (int i = 0; i < dmx->getNumFixtures(); i++) {
      dmx->setFixtureColor(i, 0, 0, 0, 255); // White color (R=0, G=0, B=0, W=255)
    }
    dmx->sendData();
    dmx->saveSettings();
    Serial.println("All fixtures set to white");
    
  } catch (...) {
    Serial.println("ERROR: Exception during DMX initialization!");
    dmx = NULL;
    dmxInitialized = false;
    DmxController::blinkLED(LED_PIN, 5, 100);  // Error indicator
  }
  
  Serial.println("\nSetup complete!");
  Serial.print("Free heap after setup: ");
  Serial.println(ESP.getFreeHeap());
  
  // Send an uplink message to confirm device is online
  if (loraInitialized) {
    // Add delay before first transmission to ensure network is ready
    Serial.println("Preparing to send first uplink...");
    delay(2000);  // 2 second delay before first uplink
    
    String message = "{\"status\":\"online\",\"dmx\":" + String(dmxInitialized ? "true" : "false") + "}";
    if (lora->sendString(message, 1, true)) { // Changed to confirmed uplink
      Serial.println("Status uplink sent successfully (confirmed)");
    } else {
      Serial.println("Failed to send status uplink");
    }
  }
  
  // If DMX is initialized, set up fixtures but don't run automatic demos
  if (dmxInitialized) {
    // Load any saved settings from persistent storage
    Serial.println("Loading DMX settings from persistent storage...");
    if (dmx->loadSettings()) {
      Serial.println("DMX settings loaded successfully");
    } else {
      Serial.println("No saved DMX settings found, using defaults");
    }
  }
  
  // Final setup indicator
  digitalWrite(LED_PIN, LOW);
  Serial.println("\n=== SETUP COMPLETE ===");
}

void loop() {
  // Reset the watchdog timer at the beginning of each loop iteration
  esp_task_wdt_reset();
  
  // Ensure all channels are clear at startup
  if (!dmxInitialized) {
    return;
  }
  
  // Handle LoRaWAN events - add extra debugging
  static unsigned long lastEventCheck = 0;
  static unsigned long lastResetCheck = 0;
  unsigned long currentMillis = millis();
  
  // Handle connection monitoring
  if (lora != NULL) {
    // Handle RadioLib events
    lora->handleEvents();
    
    // Check connection health every 2 minutes
    if (currentMillis - lastResetCheck >= 120000) {
      lastResetCheck = currentMillis;
      
      // If we have consistent failures, consider a reset
      Serial.println("Checking connection health...");
      
      if (!lora->isNetworkJoined()) {
        Serial.println("WARNING: Not connected to network. Will attempt to rejoin.");
        
        // Attempt to rejoin if disconnected
        if (lora->joinNetwork()) {
          Serial.println("Successfully reconnected to network!");
        } else {
          Serial.println("Failed to reconnect. Will reset in 30 seconds if no improvement.");
          lastResetCheck = currentMillis - 90000; // Check again in 30 seconds
        }
      } else {
        Serial.println("Connection health check: Good");
      }
    }
    
    // Debug downlink detection every 10 seconds
    if (currentMillis - lastEventCheck >= 10000) {
      lastEventCheck = currentMillis;
      
      // For debugging only - not needed in production
      if (lora->isNetworkJoined()) {
        Serial.print("Network joined, uptime: ");
        Serial.print(currentMillis / 1000);
        Serial.println(" seconds");
      }
    }
  }
  
  // Legacy code - should never actually execute now since we process in callback
  if (dataReceived) {
    Serial.println("WARNING: Data received flag set but we should have processed in callback!");
    
    // Convert payload to string
    String jsonString = "";
    for (size_t i = 0; i < receivedDataSize; i++) {
      jsonString += (char)receivedData[i];
    }
    
    Serial.print("Processing received data from buffer on port ");
    Serial.print(receivedPort);
    Serial.print(", data: ");
    Serial.println(jsonString);
    
    // Process the received data as JSON
    if (jsonString.length() > 0) {
      processJsonPayload(jsonString);
    }
    
    // Reset the data received flag
    dataReceived = false;
    
    // Clear the received data buffer
    memset(receivedData, 0, MAX_JSON_SIZE);
  }

  // Handle continuous rainbow demo mode if enabled
  if (runningRainbowDemo && dmxInitialized && dmx != NULL) {
    if (currentMillis - lastRainbowStep >= rainbowStepDelay) {
      dmx->cycleRainbowStep(rainbowStepCounter++, rainbowStaggered);
      lastRainbowStep = currentMillis;
    }
  }
  
  // Static variables for timing
  static unsigned long lastStatusUpdate = 0;
  static unsigned long lastHeartbeat = 0;
  static unsigned long lastDebugPrint = 0;
  static unsigned long lastDmxRefresh = 0;
  
  // Periodically resend DMX data to ensure fixtures receive it
  if (currentMillis - lastDmxRefresh >= 1000) { // More frequent refresh: every 1 second instead of 5
    lastDmxRefresh = currentMillis;
    
    // Only resend if DMX is initialized
    if (dmxInitialized && dmx != NULL) {
      // IMPORTANT: This only resends the CURRENT DMX values without modifying them
      // This refresh is needed because some DMX fixtures may lose their state if
      // they don't receive DMX data regularly. This ensures consistent light output.
      
      // Send without modifying any values - NO DEBUGGING OUTPUT to minimize timing disruptions
      dmx->sendData();
    }
  }
  
  // Print debug information about RX windows every 60 seconds
  if (currentMillis - lastDebugPrint >= 60000) {
    lastDebugPrint = currentMillis;
    
    // Get RX window timing information
    int rx1Delay = lora->getRx1Delay();
    int rx1Timeout = lora->getRx1Timeout();
    int rx2Timeout = lora->getRx2Timeout();
    
    Serial.println("\n----- LoRaWAN RX Window Info -----");
    Serial.print("RX1 Delay: ");
    Serial.print(rx1Delay);
    Serial.println(" seconds");
    Serial.print("RX1 Window Timeout: ");
    Serial.print(rx1Timeout);
    Serial.println(" ms");
    Serial.print("RX2 Window Timeout: ");
    Serial.print(rx2Timeout);
    Serial.println(" ms");
    Serial.println("----------------------------------\n");
  }
  
  // Send heartbeat ping every 30 seconds to create downlink opportunity
  if (currentMillis - lastHeartbeat >= 30000) {
    lastHeartbeat = currentMillis;
    
    // Only send heartbeat if lora is initialized and joined
    if (lora != NULL && lora->isNetworkJoined()) {
      Serial.println("Sending heartbeat ping (confirmed uplink)...");
      
      // Send a minimal payload as confirmed uplink
      if (lora->sendString("{\"hb\":1}", 1, true)) {
        Serial.println("Heartbeat ping sent successfully!");
      } else {
        Serial.println("Failed to send heartbeat ping.");
      }
    }
  }
  
  // Send full status update every 1 minute (reduced from 2 minutes)
  if (currentMillis - lastStatusUpdate >= 60000) {
    lastStatusUpdate = currentMillis;
    
    // Only send status if lora is initialized and joined
    if (lora != NULL && lora->isNetworkJoined()) {
      // Format a status message
      String message = "{\"status\":\"";
      message += dmxInitialized ? "DMX_OK" : "DMX_ERROR";
      message += "\"}";
      
      Serial.println("Sending status update (confirmed uplink)...");
      
      // Send the status message as a confirmed uplink
      if (lora->sendString(message, 1, true)) {
        Serial.println("Status update sent successfully!");
        // Keep the regular interval of 1 minute for the next update
      } else {
        Serial.println("Failed to send status update. Will retry in 30 seconds.");
        // Adjust the last status update time to retry sooner
        lastStatusUpdate = currentMillis - 30000;
      }
    }
  }
  
  // Blink LED occasionally to show we're alive
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 5000) {  // Every 5 seconds
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    lastBlink = millis();
  }
  
  // Reset the watchdog timer at the end of each loop iteration too
  esp_task_wdt_reset();
  
  // Short delay to prevent hogging the CPU
  delay(10);
}
