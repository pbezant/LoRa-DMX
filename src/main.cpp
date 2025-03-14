/**
 * DMX LoRa Control System for Heltec LoRa 32 V3
 * 
 * Receives JSON commands over The Things Network (TTN) and translates them
 * into DMX signals to control multiple DMX lighting fixtures.
 * 
 * Created for US915 frequency plan (United States)
 * Uses OTAA activation for TTN
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
bool dataReceived = false;

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
 * @param jsonString The JSON string to process
 * @return true if processing was successful, false otherwise
 */
bool processJsonPayload(const String& jsonString) {
  Serial.println("Processing JSON payload: " + jsonString);
  
  // Create a JSON document
  StaticJsonDocument<MAX_JSON_SIZE> doc;
  
  // Parse the JSON string
  DeserializationError error = deserializeJson(doc, jsonString);
  
  // Check for parsing errors
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Check if the JSON has a "lights" array
  if (!doc.containsKey("lights") || !doc["lights"].is<JsonArray>()) {
    Serial.println("JSON format error: 'lights' array not found");
    return false;
  }
  
  // Get the lights array
  JsonArray lights = doc["lights"].as<JsonArray>();
  
  // Clear all DMX channels before setting new values
  dmx->clearAllChannels();
  
  // Process each light in the array
  for (JsonObject light : lights) {
    // Check if light has required fields
    if (!light.containsKey("address") || !light.containsKey("channels")) {
      Serial.println("JSON format error: light missing required fields");
      continue;  // Skip this light
    }
    
    // Get the DMX start address
    int address = light["address"].as<int>();
    
    // Get the channels array
    JsonArray channels = light["channels"].as<JsonArray>();
    
    Serial.print("Setting DMX channels for fixture at address ");
    Serial.print(address);
    Serial.print(": ");
    
    // Set the channel values
    int channelIndex = 0;
    for (JsonVariant value : channels) {
      uint8_t dmxValue = value.as<uint8_t>();
      // Set the DMX value for this channel
      dmx->getDmxData()[address + channelIndex] = dmxValue;
      
      Serial.print(dmxValue);
      Serial.print(" ");
      
      channelIndex++;
    }
    Serial.println();
  }
  
  // Send the updated DMX data
  dmx->sendData();
  
  return true;
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
  Serial.print("Downlink callback triggered on port ");
  Serial.print(port);
  Serial.print(", size: ");
  Serial.println(size);
  
  // Store the data for processing in the main loop
  if (size <= MAX_JSON_SIZE) {
    memcpy(receivedData, payload, size);
    receivedDataSize = size;
    receivedPort = port;
    dataReceived = true;
    
    // Blink LED once to indicate data reception
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
  } else {
    Serial.println("ERROR: Received payload exceeds buffer size");
  }
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
  // Initialize serial first
  Serial.begin(SERIAL_BAUD);
  delay(2000);
  
  // Set up LED pin
  pinMode(LED_PIN, OUTPUT);
  
  // Initial blink to indicate we're alive
  DmxController::blinkLED(LED_PIN, 2, 500);
  
  // Print startup message
  Serial.println("\n\n===== DMX LoRa Control System =====");
  Serial.println("Initializing...");
  
  // Print ESP-IDF version
  Serial.print("ESP-IDF Version: ");
  Serial.println(ESP.getSdkVersion());
  
  // Print memory info
  Serial.print("Free heap at startup: ");
  Serial.println(ESP.getFreeHeap());
  
  // Print LoRa pin configuration for debugging
  Serial.println("\nLoRa Pin Configuration:");
  Serial.printf("CS Pin: %d\n", LORA_CS_PIN);
  Serial.printf("DIO1 Pin: %d\n", LORA_DIO1_PIN);
  Serial.printf("Reset Pin: %d\n", LORA_RESET_PIN);
  Serial.printf("Busy Pin: %d\n", LORA_BUSY_PIN);
  Serial.printf("SPI SCK: %d, MISO: %d, MOSI: %d\n", LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI);
  Serial.println("Frequency: Default to US915 (United States) - but configurable to other bands");
  Serial.println("Default frequency band is US915 with subband 2");
  
  // Initialize SPI for LoRa
  SPI.begin(LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI);
  Serial.println("SPI initialized for LoRa");
  delay(100); // Short delay after SPI init
  
  // Initialize LoRaWAN
  Serial.println("\nInitializing LoRaWAN...");
  
  try {
    // Create delay before LoRa initialization
    delay(200);
    
    // Create a new LoRaManager with default US915 band and subband 2 (can be changed for other regions)
    lora = new LoRaManager(US915, 2);
    Serial.println("LoRaManager instance created, attempting to initialize radio...");
    
    // Add delay before begin
    delay(100);
    
    if (lora->begin(LORA_CS_PIN, LORA_DIO1_PIN, LORA_RESET_PIN, LORA_BUSY_PIN)) {
      Serial.println("LoRaWAN module initialized successfully!");
      
      // Set credentials
      lora->setCredentials(joinEUI, devEUI, appKey, nwkKey);
      Serial.println("LoRaWAN credentials set");
      
      // Register the downlink callback
      lora->setDownlinkCallback(handleDownlinkCallback);
      Serial.println("Downlink callback registered");
      
      // Join network
      Serial.println("Joining LoRaWAN network...");
      if (lora->joinNetwork()) {
        Serial.println("Successfully joined LoRaWAN network!");
        
        // Add delay to allow session establishment to complete
        Serial.println("Waiting for session establishment...");
        delay(7000);  // 7 second delay after join for more reliable session establishment
        
        loraInitialized = true;
        DmxController::blinkLED(LED_PIN, 3, 300);  // 3 blinks for successful join
      } else {
        Serial.print("Failed to join LoRaWAN network. Error code: ");
        Serial.println(lora->getLastErrorCode());
        DmxController::blinkLED(LED_PIN, 4, 200);  // 4 blinks for join failure
      }
    } else {
      Serial.print("Failed to initialize LoRaWAN. Error code: ");
      Serial.println(lora->getLastErrorCode());
      DmxController::blinkLED(LED_PIN, 5, 100);  // 5 blinks for init failure
    }
  } catch (...) {
    Serial.println("ERROR: Exception during LoRaWAN initialization!");
    lora = NULL;
  }
  
  // Initialize DMX with error handling
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
    if (lora->sendString(message)) {
      Serial.println("Status uplink sent successfully");
    } else {
      Serial.println("Failed to send status uplink");
    }
  }
}

void loop() {
  // Handle LoRaWAN events (includes receiving downlinks)
  if (loraInitialized && lora != NULL) {
    lora->handleEvents();
    
    // Check if we have received data
    if (dataReceived) {
      // Process the received data
      handleDownlink(receivedData, receivedDataSize, receivedPort);
      
      // Reset the flag
      dataReceived = false;
    }
    
    // Send periodic status update with better timing controls
    static unsigned long lastStatusUpdate = 0;
    const unsigned long statusUpdateInterval = 600000; // 10 minutes in milliseconds
    
    if (millis() - lastStatusUpdate > statusUpdateInterval) {
      // Format a simple status message
      String statusMsg = "{\"status\":\"online\",\"dmx\":" + String(dmxInitialized ? "true" : "false") + "}";
      
      if (lora->sendString(statusMsg)) {
        Serial.println("Status update sent");
        lastStatusUpdate = millis();
      } else {
        // If send fails, try again in 1 minute instead of 10
        lastStatusUpdate = millis() - statusUpdateInterval + 60000;
      }
    }
  }
  
  // Receive DMX data if needed (for bidirectional operation)
  // This would be implemented if we needed to receive DMX data
  
  // Blink LED occasionally to show we're alive
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 5000) {  // Every 5 seconds
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    lastBlink = millis();
  }
  
  // Short delay to prevent hogging the CPU
  delay(10);
}
