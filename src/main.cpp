/**
 * DMX LoRa Control System for Heltec LoRa 32 V3
 * 
 * Receives JSON commands over The Things Network (TTN) and translates them
 * into DMX signals to control multiple DMX lighting fixtures.
 * 
 * Created for US915 frequency plan (United States)
 * Uses OTAA activation for TTN
 * 
 * IMPORTANT: This firmware operates in LoRaWAN Class C mode, which keeps
 * the receive window open continuously. This provides immediate response to
 * downlink commands from the server but significantly increases power consumption.
 * FOR MAINS-POWERED APPLICATIONS ONLY - not suitable for battery operation.
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
#include <vector>
#include "McciLmicWrapper.h" // Added MCCI LMIC Wrapper
#include "DmxController.h"
#include <esp_task_wdt.h>  // Watchdog
#include "secrets.h"  // Include the secrets.h file for LoRaWAN credentials

// Required by MCCI LMIC library for SX126x
// Pin mapping for Heltec LoRa 32 V3 (SX1262)
// This must be globally defined.
const lmic_pinmap lmic_pins = {
    .nss = 8,      // LoRa Chip Select
    .rst = 12,     // LoRa Reset
    .dio = { /* DIO0 */ 4, /* DIO1 */ 14, /* DIO2 */ LMIC_UNUSED_PIN }, // DIO0 set to GPIO4
    // The HalPinmap_t for MCCI LMIC v5.0.1 does NOT include a '.busy' member directly.
    // Busy line is handled by other mechanisms (e.g., build flags, internal DIO usage).
    // Other fields like .rxtx, .rxtx_rx_active, .rssi_cal, .spi_freq will use defaults
};

// TEMPORARY PLACEHOLDER DEFINITIONS - REMOVE AND USE secrets.h PROPERLY
// const char* TTN_APP_EUI = "0000000000000000"; 
// const char* TTN_DEV_EUI = "0000000000000000";
// const char* TTN_APP_KEY = "00000000000000000000000000000000";

// Debug output
#define SERIAL_BAUD 115200

// Diagnostic LED pin
#define LED_PIN 35  // Built-in LED on Heltec LoRa 32 V3

// DMX configuration for Heltec LoRa 32 V3
#define DMX_PORT 1
#define DMX_TX_PIN 19  // TX pin for DMX
#define DMX_RX_PIN 20  // RX pin for DMX
#define DMX_DIR_PIN 5  // DIR pin for DMX (connect to both DE and RE on MAX485)

// LoRaWAN TTN Connection Parameters - These are now handled by McciLmicWrapper's pinmap
// #define LORA_CS_PIN   8
// #define LORA_DIO1_PIN 14
// #define LORA_RESET_PIN 12
// #define LORA_BUSY_PIN 13

// SPI pins for Heltec LoRa 32 V3 - LMIC HAL typically uses default SPI pins
// #define LORA_SPI_SCK  9
// #define LORA_SPI_MISO 11
// #define LORA_SPI_MOSI 10

// DMX configuration
#define MAX_FIXTURES 32
#define MAX_CHANNELS_PER_FIXTURE 16
#define MAX_JSON_SIZE 1024

// --- Original Global Variables Block (Keep This One) --- 
bool dmxInitialized = false;
DmxController* dmx = NULL;
McciLmicWrapper* loraWan = NULL; 
bool loraInitialized = false; 

SemaphoreHandle_t dmxMutex = NULL;
TaskHandle_t dmxTaskHandle = NULL;

// Global variables for continuous rainbow effect (Keep this block)
bool runningRainbowDemo = false;
unsigned long lastRainbowStep = 0;
uint32_t rainbowStepCounter = 0;
int rainbowStepDelay = 30;
bool rainbowStaggered = true;

// Add timing variables for various operations (Keep this block)
unsigned long lastHeartbeat = 0;
unsigned long lastStatusUpdate = 0;

#define WDT_TIMEOUT 30
// --- End Original Global Variables Block ---

// Forward declarations
void processReceivedLorawanData(uint8_t port, const uint8_t* data, int len);
static void lmicRxCallbackAdaptor(McciLmicWrapper* pUserData, uint8_t port, const uint8_t *data, int len);
bool processLightsJson(JsonArray lightsArray);
void processMessageQueue();
void printDmxValues(int startAddr, int numChannels);

// Placeholder for the received data - Direct processing in callback is preferred
// uint8_t receivedData[MAX_JSON_SIZE];
// size_t receivedDataSize = 0;
// uint8_t receivedPort = 0;
// bool dataReceived = false;

// Add pattern state persistence structure before DmxPattern class
struct PatternState {
  bool isActive;
  uint8_t patternType;
  int speed;
  int maxCycles;
  bool staggered;
  uint32_t step;
};

// Add pattern handler class before the main setup() function
class DmxPattern {
public:
  enum PatternType {
    NONE,
    COLOR_FADE,
    RAINBOW,
    STROBE,
    CHASE,
    ALTERNATE
  };

  DmxPattern() : active(false), patternType(NONE), speed(50), step(0), lastUpdate(0), cycleCount(0), maxCycles(5), staggered(true) {}

  void start(PatternType type, int patternSpeed, int cycles = 5) {
    active = true;
    patternType = type;
    speed = patternSpeed;
    step = 0;
    cycleCount = 0;
    maxCycles = cycles;
    lastUpdate = millis();
    staggered = true;
    
    Serial.print("Pattern started: ");
    switch (patternType) {
      case COLOR_FADE: Serial.println("COLOR_FADE"); break;
      case RAINBOW: Serial.println("RAINBOW"); break;
      case STROBE: Serial.println("STROBE"); break;
      case CHASE: Serial.println("CHASE"); break;
      case ALTERNATE: Serial.println("ALTERNATE"); break;
      default: Serial.println("UNKNOWN");
    }

    // Save pattern state when started
    savePatternState();
  }

  void stop() {
    active = false;
    patternType = NONE;
    Serial.println("Pattern stopped");
    
    // Clear saved pattern state when stopped
    clearSavedPatternState();
  }
  
  bool isActive() {
    return active;
  }

  // Add save pattern state function
  void savePatternState() {
    if (!dmxInitialized || dmx == NULL) return;
    
    PatternState state;
    state.isActive = active;
    state.patternType = (uint8_t)patternType;
    state.speed = speed;
    state.maxCycles = maxCycles;
    state.staggered = staggered;
    state.step = step;

    // Use DMX controller's persistent storage to save pattern state
    dmx->saveCustomData("pattern_state", (uint8_t*)&state, sizeof(PatternState));
    Serial.println("Pattern state saved to persistent storage");
  }

  // Add restore pattern state function
  void restorePatternState() {
    if (!dmxInitialized || dmx == NULL) return;
    
    PatternState state;
    if (dmx->loadCustomData("pattern_state", (uint8_t*)&state, sizeof(PatternState))) {
      Serial.println("Restoring saved pattern state");
      
      if (state.isActive) {
        active = true;
        patternType = (PatternType)state.patternType;
        speed = state.speed;
        maxCycles = state.maxCycles;
        staggered = state.staggered;
        step = state.step;
        lastUpdate = millis();
        cycleCount = 0;  // Reset cycle count on restore
        
        Serial.print("Restored pattern: ");
        switch (patternType) {
          case COLOR_FADE: Serial.println("COLOR_FADE"); break;
          case RAINBOW: Serial.println("RAINBOW"); break;
          case STROBE: Serial.println("STROBE"); break;
          case CHASE: Serial.println("CHASE"); break;
          case ALTERNATE: Serial.println("ALTERNATE"); break;
          default: Serial.println("UNKNOWN");
        }
        Serial.print("Speed: "); Serial.println(speed);
        Serial.print("Staggered: "); Serial.println(staggered ? "Yes" : "No");
      }
    } else {
      Serial.println("No saved pattern state found");
      active = false;
      patternType = NONE;
    }
  }

  // Add function to clear saved pattern state
  void clearSavedPatternState() {
    if (!dmxInitialized || dmx == NULL) return;
    
    PatternState state;
    memset(&state, 0, sizeof(PatternState));  // Clear all data
    dmx->saveCustomData("pattern_state", (uint8_t*)&state, sizeof(PatternState));
    Serial.println("Pattern state cleared from persistent storage");
  }
  
  void update() {
    if (!active || !dmxInitialized || dmx == NULL) {
      return;
    }
    
    unsigned long now = millis();
    if (now - lastUpdate < speed) {
      return;
    }
    
    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) != pdTRUE) {
      Serial.println("Failed to take DMX mutex for pattern update");
      return;
    }
    
    lastUpdate = now;
    
    switch (patternType) {
      case COLOR_FADE:
        updateColorFade();
        break;
      case RAINBOW:
        updateRainbow();
        break;
      case STROBE:
        updateStrobe();
        break;
      case CHASE:
        updateChase();
        break;
      case ALTERNATE:
        updateAlternate();
        break;
      default:
        break;
    }
    
    // Send the DMX data
    dmx->sendData();

    if (step % 10 == 0) {
      savePatternState();
    }
    
    xSemaphoreGive(dmxMutex);
  }

private:
  bool active;
  PatternType patternType;
  int speed;  // Time in ms between updates
  uint32_t step;   // Current step in the pattern
  unsigned long lastUpdate;
  int cycleCount;
  int maxCycles;
  bool staggered;
  
  // HSV to RGB conversion for color effects
  void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    float c = v * s;
    float x = c * (1 - abs(fmod(h / 60.0, 2) - 1));
    float m = v - c;
    
    float r1, g1, b1;
    if (h < 60) {
      r1 = c; g1 = x; b1 = 0;
    } else if (h < 120) {
      r1 = x; g1 = c; b1 = 0;
    } else if (h < 180) {
      r1 = 0; g1 = c; b1 = x;
    } else if (h < 240) {
      r1 = 0; g1 = x; b1 = c;
    } else if (h < 300) {
      r1 = x; g1 = 0; b1 = c;
    } else {
      r1 = c; g1 = 0; b1 = x;
    }
    
    r = (r1 + m) * 255;
    g = (g1 + m) * 255;
    b = (b1 + m) * 255;
  }
  
  // Color fade pattern (gradually cycles through colors)
  void updateColorFade() {
    float hue = (step % 360);
    step = (step + 2) % 360;
    
    uint8_t r, g, b;
    hsvToRgb(hue, 1.0, 1.0, r, g, b);
    
    // Set all fixtures to the same color
    int numFixtures = dmx->getNumFixtures();
    for (int i = 0; i < numFixtures; i++) {
      dmx->setFixtureColor(i, r, g, b, 0);
    }
    
    // Check if we've completed a cycle
    if (step == 0) {
      cycleCount++;
      if (cycleCount >= maxCycles && maxCycles > 0) {
        stop();
      }
    }
  }
  
  // Rainbow pattern (different color on each fixture)
  void updateRainbow() {
    int numFixtures = dmx->getNumFixtures();
    if (numFixtures == 0) return;
    
    // Calculate hue offset for this step
    int baseHue = step % 360;
    step = (step + 5) % 360;
    
    // Distribute colors across fixtures
    for (int i = 0; i < numFixtures; i++) {
      float hue = fmod(baseHue + (360.0 * i / numFixtures), 360);
      
      uint8_t r, g, b;
      hsvToRgb(hue, 1.0, 1.0, r, g, b);
      
      dmx->setFixtureColor(i, r, g, b, 0);
    }
    
    // Check if we've completed a cycle
    if (step == 0) {
      cycleCount++;
      if (cycleCount >= maxCycles && maxCycles > 0) {
        stop();
      }
    }
  }
  
  // Strobe pattern (flash on and off)
  void updateStrobe() {
    bool isOn = (step % 2) == 0;
    step++;
    
    int numFixtures = dmx->getNumFixtures();
    for (int i = 0; i < numFixtures; i++) {
      if (isOn) {
        dmx->setFixtureColor(i, 255, 255, 255, 255);  // White when on
      } else {
        dmx->setFixtureColor(i, 0, 0, 0, 0);  // Off
      }
    }
    
    // Count each on-off cycle as one complete cycle
    if (step % 2 == 0) {
      cycleCount++;
      if (cycleCount >= maxCycles && maxCycles > 0) {
        stop();
      }
    }
  }
  
  // Chase pattern (one light at a time)
  void updateChase() {
    int numFixtures = dmx->getNumFixtures();
    if (numFixtures == 0) return;
    
    int activeFixture = step % numFixtures;
    step = (step + 1) % numFixtures;
    
    // Turn all fixtures off first
    for (int i = 0; i < numFixtures; i++) {
      if (i == activeFixture) {
        // This fixture gets the color - use a rotating hue
        uint8_t r, g, b;
        float hue = (cycleCount * 30) % 360;  // Change color every full chase cycle
        hsvToRgb(hue, 1.0, 1.0, r, g, b);
        
        dmx->setFixtureColor(i, r, g, b, 0);
      } else {
        dmx->setFixtureColor(i, 0, 0, 0, 0);
      }
    }
    
    // Count a full chase sequence as one complete cycle
    if (step == 0) {
      cycleCount++;
      if (cycleCount >= maxCycles && maxCycles > 0) {
        stop();
      }
    }
  }
  
  // Alternating pattern (every other fixture)
  void updateAlternate() {
    int numFixtures = dmx->getNumFixtures();
    bool flipState = (step % 2) == 0;
    step++;
    
    for (int i = 0; i < numFixtures; i++) {
      bool isOn = (i % 2 == 0) ? flipState : !flipState;
      
      if (isOn) {
        uint8_t r, g, b;
        float hue = (cycleCount * 40) % 360;  // Change color every flip
        hsvToRgb(hue, 1.0, 1.0, r, g, b);
        
        dmx->setFixtureColor(i, r, g, b, 0);
      } else {
        dmx->setFixtureColor(i, 0, 0, 0, 0);
      }
    }
    
    // Count each on-off alternation as one complete cycle
    if (step % 2 == 0) {
      cycleCount++;
      if (cycleCount >= maxCycles && maxCycles > 0) {
        stop();
      }
    }
  }
};

// Create a global pattern handler
DmxPattern patternHandler;

// Add connection state tracking
bool isConnected = false;
uint32_t lastConnectionAttempt = 0;
const uint32_t CONNECTION_RETRY_INTERVAL = 60000; // 1 minute between retries

// Add message queue for priority handling
struct PendingMessage {
    String payload;
    uint8_t port;
    bool confirmed;
    uint8_t priority;  // 0 = highest, 255 = lowest
    uint32_t timestamp;
};

std::vector<PendingMessage> messageQueue;
const size_t MAX_QUEUE_SIZE = 10;

// Event callback functions
// void onConnectionStateChange(bool connected) { ... } // This was likely for old LoRaManager
// void onTransmissionComplete(bool success, int errorCode) { ... } // This was likely for old LoRaManager

void processMessageQueue() {
  if (messageQueue.empty() || (loraWan && !loraWan->isTxReady())) {
    return; // Nothing to send or LoRaWAN busy
  }

  // Sort by priority and then by timestamp (FIFO for same priority)
  std::sort(messageQueue.begin(), messageQueue.end(), [](const PendingMessage& a, const PendingMessage& b) {
    if (a.priority != b.priority) {
      return a.priority < b.priority;
    }
    return a.timestamp < b.timestamp;
  });

  // Try to send the highest priority message
  const PendingMessage& msg = messageQueue.front();
  
  // Convert String payload to uint8_t buffer for sendData
  // Ensure msg.payload.length() doesn't exceed buffer or LoRaWAN max payload size
  size_t payloadLen = msg.payload.length();
  uint8_t payloadBuffer[payloadLen + 1]; // +1 for null terminator if needed, though sendData takes len
  msg.payload.getBytes(payloadBuffer, payloadLen + 1);

  if (loraWan && loraWan->sendData(msg.port, payloadBuffer, payloadLen, msg.confirmed)) {
    Serial.print(F("Message sent from queue: ")); Serial.println(msg.payload);
    messageQueue.erase(messageQueue.begin());
  } else {
    Serial.println(F("LoRaWAN busy or send failed, message stays in queue."));
    // Optionally, add a retry limit or age-out for messages in queue
  }
}

void queueMessage(const String& payload, uint8_t port, bool confirmed, uint8_t priority) {
    // If queue is full, remove lowest priority message
    if (messageQueue.size() >= MAX_QUEUE_SIZE) {
        auto lowestPriority = std::max_element(messageQueue.begin(), messageQueue.end(),
            [](const PendingMessage& a, const PendingMessage& b) {
                return a.priority < b.priority;
            });
        
        if (lowestPriority != messageQueue.end() && lowestPriority->priority > priority) {
            messageQueue.erase(lowestPriority);
        } else {
            Serial.println("Message queue full and new message priority too low");
            return;
        }
    }
    
    PendingMessage msg = {
        .payload = payload,
        .port = port,
        .confirmed = confirmed,
        .priority = priority,
        .timestamp = millis()
    };
    
    messageQueue.push_back(msg);
    
    // Try to process the queue immediately if we're connected
    if (isConnected) {
        processMessageQueue();
    }
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
  StaticJsonDocument<MAX_JSON_SIZE> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print("JSON parsing error: ");
    Serial.println(error.c_str());
    return false;
  }

  Serial.print("Processing JSON payload: ");
  Serial.println(jsonString);

  // First, check for pattern commands
  if (doc.containsKey("pattern")) {
    // Handle both object and string pattern formats
    if (doc["pattern"].is<JsonObject>()) {
      // Object format - {"pattern": {"type": "rainbow", "speed": 50}}
      JsonObject pattern = doc["pattern"];
      if (pattern.containsKey("type")) {
        String type = pattern["type"];
        int speed = pattern["speed"] | 50;  // Default 50ms
        int cycles = pattern["cycles"] | 5;  // Default 5 cycles
        
        DmxPattern::PatternType patternType = DmxPattern::NONE;
        
        if (type == "colorFade") {
          patternType = DmxPattern::COLOR_FADE;
        } else if (type == "rainbow") {
          patternType = DmxPattern::RAINBOW;
        } else if (type == "strobe") {
          patternType = DmxPattern::STROBE;
          speed = pattern["speed"] | 100;  // Slower default for strobe
        } else if (type == "chase") {
          patternType = DmxPattern::CHASE;
        } else if (type == "alternate") {
          patternType = DmxPattern::ALTERNATE;
        } else if (type == "stop") {
          patternHandler.stop();
          return true;
        }
        
        if (patternType != DmxPattern::NONE) {
          patternHandler.start(patternType, speed, cycles);
          return true;
        }
      }
    } else if (doc["pattern"].is<String>()) {
      // String format - {"pattern": "rainbow"}
      String patternType = doc["pattern"].as<String>();
      Serial.print("Simple pattern format detected: ");
      Serial.println(patternType);
      
      // Default values for each pattern
      int speed = 50;
      int cycles = 5;
      DmxPattern::PatternType type = DmxPattern::NONE;
      
      if (patternType == "colorFade") {
        type = DmxPattern::COLOR_FADE;
      } else if (patternType == "rainbow") {
        type = DmxPattern::RAINBOW;
        cycles = 3;
      } else if (patternType == "strobe") {
        type = DmxPattern::STROBE;
        speed = 100;
        cycles = 10;
      } else if (patternType == "chase") {
        type = DmxPattern::CHASE;
        speed = 200;
        cycles = 3;
      } else if (patternType == "alternate") {
        type = DmxPattern::ALTERNATE;
        speed = 300;
        cycles = 5;
      } else if (patternType == "stop") {
        patternHandler.stop();
        return true;
      }
      
      if (type != DmxPattern::NONE) {
        Serial.println("Starting pattern...");
        patternHandler.start(type, speed, cycles);
        return true;
      }
    }
  }

  // Then check for test commands
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
      Serial.print(", Alternate: ");
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
      Serial.println(F("=== PING RECEIVED (JSON) ==="));
      // ... blink LED ...
      if (loraInitialized && loraWan && loraWan->isJoined()) {
        String response = "{\"ping_response\":\"ok_json\"}";
        loraWan->sendData(1, (const uint8_t*)response.c_str(), response.length(), true);
        Serial.println(F("JSON Ping response sent (confirmed via loraWan->sendData)"));
      }
      return true;
    }
    else {
      Serial.print("Unknown test pattern: ");
      Serial.println(pattern);
      return false;
    }
  }
  // Finally check for direct light control
  if (doc.containsKey("lights")) {
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

  // If we got here, no valid command objects were found
  Serial.println("JSON format error: missing 'lights', 'pattern', or 'test' object");
  return false;
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
  // printDmxValues(1, 20); // Commented out to resolve linker error
  
  bool atLeastOneValid = false;
  
  // Take mutex before modifying DMX data
  if (xSemaphoreTake(dmxMutex, portMAX_DELAY) != pdTRUE) {
    Serial.println("Failed to take DMX mutex, aborting light update");
    return false;
  }
  
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
    // printDmxValues(1, 20); // Commented out to resolve linker error
    
    dmx->sendData();
    
    // Save settings to persistent storage
    dmx->saveSettings();
    Serial.println("DMX settings saved to persistent storage");
  }
  
  // Release the mutex
  xSemaphoreGive(dmxMutex);
  
  return atLeastOneValid;
}

// Add this helper function at the end of the file, before the loop() function
void debugBytes(const char* label, uint8_t* data, size_t size) {
  Serial.print(label);
  Serial.print(" (");
  Serial.print(size);
  Serial.println(" bytes):");
  
  // Print as hex
  Serial.print("HEX: ");
  for (size_t i = 0; i < size; i++) {
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Print as ASCII
  Serial.print("ASCII: \"");
  for (size_t i = 0; i < size; i++) {
    if (data[i] >= 32 && data[i] <= 126) {
      Serial.print((char)data[i]);
    } else {
      Serial.print("·"); // Placeholder for non-printable chars
    }
  }
  Serial.println("\"");
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
  // This function is largely superseded by processReceivedLorawanData
  // but I will check for any direct send calls that might have been missed.

  // ... (original logic for parsing payloadStr) ...
  String payloadStr;
  for (size_t i = 0; i < size; i++) {
    payloadStr += (char)payload[i];
  }
  payloadStr.trim();

  // Example of a direct send that might have been missed from old logic:
  if (payloadStr.equalsIgnoreCase("test_send_old_style")) {
    if (loraInitialized && loraWan && loraWan->isJoined()) { 
      String response = "{\"test_ack\":\"ok\"}";
      // Corrected call:
      loraWan->sendData(port, (const uint8_t*)response.c_str(), response.length(), false);
      Serial.println(F("Test ACK sent via loraWan->sendData"));
    }
    return; // Assuming this was a command handled directly
  }

  // The main processing is now in processReceivedLorawanData via the adaptor
  // This function (handleDownlinkCallback) would ideally be removed or fully integrated.
  // For now, ensure any SEND operations are corrected if they were here.
  if (payloadStr.equalsIgnoreCase("ping_old")) { // Example if you had an old ping here
    Serial.println("=== PING RECEIVED (old handler) ===");
    if (loraInitialized && loraWan && loraWan->isJoined()) {
      String response = "{\"ping_response\":\"ok_old_handler\"}";
      loraWan->sendData(1, (const uint8_t*)response.c_str(), response.length(), true);
      Serial.println(F("Old Ping response sent via loraWan->sendData"));
    }
    return;
  }

  // If this function is still called, delegate to the new one for JSON processing
  // processReceivedLorawanData(port, payload, size); 
  // NO - this creates a loop. processReceivedLorawanData is called by the adaptor.
  // This handleDownlinkCallback should be phased out.
  Serial.println(F("handleDownlinkCallback was called - this should be phased out."));
}

/**
 * DMX Task - Runs on Core 0 for continuous DMX output
 * This dedicated task ensures DMX signals are sent continuously without
 * being interrupted by LoRa operations which run on Core 1
 */
void dmxTask(void * parameter) {
  // Set task priority to high for consistent timing
  vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
  
  Serial.println("DMX task started on Core 0");
  
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 20; // 20ms refresh (50Hz)
  
  // Initialize the xLastWakeTime variable with the current time
  xLastWakeTime = xTaskGetTickCount();
  
  while(true) {
    // Check if DMX is initialized
    if (dmxInitialized && dmx != NULL) {
      // Take mutex to ensure thread-safe access to DMX data
      if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        // Send DMX data - this function now runs uninterrupted by LoRa even during RX windows
        dmx->sendData();
        
        // Give mutex back
        xSemaphoreGive(dmxMutex);
      }
    }
    
    // Yield to other tasks at exactly the right refresh frequency
    // This is more precise than delay() and ensures a stable DMX refresh rate
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void initializeLoRaWAN() {
  Serial.println(F("DEBUG: initializeLoRaWAN() - Partially restoring..."));
  delay(100);

  Serial.println(F("DEBUG: initializeLoRaWAN - loraWan check point 1"));
  if (loraWan != NULL) {
    Serial.println(F("DEBUG: initializeLoRaWAN - loraWan was NOT NULL. Deleting..."));
    delete loraWan;
    loraWan = NULL; // Good practice
  } else {
    Serial.println(F("DEBUG: initializeLoRaWAN - loraWan was NULL."));
  }
  Serial.println(F("DEBUG: initializeLoRaWAN - loraWan check point 2. Creating new instance..."));
  loraWan = new McciLmicWrapper();
  Serial.println(F("DEBUG: initializeLoRaWAN - McciLmicWrapper instance CREATED."));
  delay(100);

  Serial.println(F("DEBUG: initializeLoRaWAN - About to call loraWan->begin()..."));
  if (!loraWan->begin()) {
    Serial.println(F("Failed to initialize McciLmicWrapper (loraWan->begin() failed)"));
    loraInitialized = false;
    Serial.println(F("DEBUG: initializeLoRaWAN - Returning after loraWan->begin() FAILED."));
    return;
  }
  Serial.println(F("DEBUG: initializeLoRaWAN - loraWan->begin() SUCCEEDED."));
  
  loraInitialized = true; // Set true as begin() succeeded.
  
  // Register the RX callback
  loraWan->onReceive(lmicRxCallbackAdaptor);
  Serial.println(F("RX Callback registered."));

  // Attempt OTAA join using credentials from secrets.h
  Serial.print(F("Attempting OTAA join with AppEUI: ")); Serial.println(APPEUI); // Using APPEUI from secrets.h
  
  if (loraWan->joinOTAA(APPEUI, DEVEUI, APPKEY)) { // Using macros from secrets.h
    Serial.println(F("OTAA join process started..."));
    // loraInitialized = true; // Already set true if begin() succeeded, join is async
  } else {
    Serial.println(F("Failed to start OTAA join process."));
    loraInitialized = false; // If joinOTAA itself fails to start (e.g., bad params, though unlikely here)
  }

  Serial.println(F("DEBUG: initializeLoRaWAN - Returning after loraWan->begin() and join started."));
  return;

/*
  // Original first line: Serial.println(F("Initializing LoRaWAN with MCCI LMIC Wrapper..."));
  // delay(100); // ADDED DELAY TO HELP FLUSH SERIAL

  // ... debug prints for lmic_pins and HAL call were here ...

  // ... loraWan pointer handling and new McciLmicWrapper() IS NOW ABOVE ...

  // Serial.println(F("DEBUG main.cpp: About to call loraWan.begin()...")); // MOVED UP
  // Serial.println(F("DEBUG main.cpp: Before loraWan.begin():"));
  // Serial.print(F("  Address of global lmic_pins: 0x")); Serial.println((uintptr_t)&lmic_pins, HEX);
  // Serial.print(F("  lmic_pins.nss: ")); Serial.println(lmic_pins.nss);
  // Serial.print(F("  lmic_pins.rst: ")); Serial.println(lmic_pins.rst);
  // Serial.print(F("  lmic_pins.dio[0]: ")); Serial.println(lmic_pins.dio[0]);
  // Serial.print(F("  lmic_pins.dio[1]: ")); Serial.println(lmic_pins.dio[1]);
  // Serial.print(F("  lmic_pins.dio[2]: ")); Serial.println(lmic_pins.dio[2]);

  // if (!loraWan->begin()) { // THIS BLOCK IS NOW UNCOMMENTED ABOVE
  //   Serial.println(F("Failed to initialize McciLmicWrapper"));
  //   loraInitialized = false;
  //   return;
  // }
  
  // Serial.println(F("MCCI LMIC Wrapper initialized.")); // MOVED DEBUG LINE
  // loraWan->onReceive(lmicRxCallbackAdaptor); // MOVED UP
  // Serial.println(F("RX Callback registered.")); // MOVED UP

  // Serial.print(F("Attempting OTAA join with AppEUI: ")); Serial.println(TTN_APP_EUI); // MOVED UP & CHANGED
  
  // if (loraWan->joinOTAA(TTN_APP_EUI, TTN_DEV_EUI, TTN_APP_KEY)) { // MOVED UP & CHANGED
  //   Serial.println(F("OTAA join process started..."));
  //   loraInitialized = true; 
  // } else {
  //   Serial.println(F("Failed to start OTAA join process."));
  //   loraInitialized = false;
  // }
*/
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 2000); 
  Serial.println(F("\n\n>>> SETUP FUNCTION STARTED <<<")); // NEW DEBUG LINE
  delay(100); // Allow serial to flush

  Serial.println(F("\n\nStarting DMX LoRa Controller - Heltec LoRa 32 V3 - MCCI LMIC Version"));
  delay(100); // Allow serial to flush

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.println(F(">>> LED PIN INITIALIZED <<<"));
  delay(100);

  Serial.println(F("Initializing Watchdog..."));
  esp_task_wdt_init(WDT_TIMEOUT, true); 
  esp_task_wdt_add(NULL); 
  Serial.println(F("Watchdog Initialized."));
  Serial.println(F(">>> WATCHDOG INITIALIZED <<<"));
  delay(100);

  dmxMutex = xSemaphoreCreateMutex();
  if (dmxMutex == NULL) {
    Serial.println(F("Failed to create DMX mutex!"));
    while(1);
  }
  Serial.println(F(">>> DMX MUTEX CREATED <<<"));
  delay(100);

  Serial.println(F("Initializing DMX..."));
  dmx = new DmxController(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_DIR_PIN);
  dmx->begin(); 
  dmxInitialized = true; 
  Serial.println(F("DMX Initialized."));
  dmx->clearAllChannels(); 
  dmx->sendData(); 
  Serial.println(F(">>> DMX FULLY INITIALIZED AND DATA SENT <<<"));
  delay(100);
  
  Serial.println(F(">>> CALLING initializeLoRaWAN() NOW... <<<")); // NEW DEBUG LINE
  delay(100); // Allow serial to flush
  initializeLoRaWAN(); // Call to our problematic function
  
  Serial.println(F(">>> initializeLoRaWAN() HAS RETURNED (or program is about to crash if it didn't) <<<")); // NEW DEBUG
  delay(100);

  Serial.println(F("Setup Complete. Device ready."));
  if(loraInitialized && loraWan && loraWan->isJoined()){
    Serial.println(F("LoRaWAN Joined! Enabling Class C."));
    loraWan->enableClassC();
  } else if (loraInitialized) {
    Serial.println(F("LoRaWAN Initialized, waiting for join to complete to enable Class C..."));
  } else {
    Serial.println(F("LoRaWAN failed to initialize. Class C not enabled."));
  }
}

// Static adaptor function to match McciLmicWrapper's callback signature
static void lmicRxCallbackAdaptor(McciLmicWrapper* pUserData, uint8_t port, const uint8_t *data, int len) {
    // pUserData could be used to pass 'this' if loraWan was a member of another class
    // For now, directly call the processing function.
    if (len > 0) {
        Serial.print(F("LMIC Adaptor: Received data. Port: ")); Serial.print(port);
        Serial.print(F(", Length: ")); Serial.println(len);
        digitalWrite(LED_PIN, HIGH); // Turn on LED on RX
        
        // Call your main data processing function
        // This function will need to be created/adapted from your old handleDownlinkCallback
        processReceivedLorawanData(port, data, len);
        
        digitalWrite(LED_PIN, LOW); // Turn off LED after processing
    } else {
        Serial.println(F("LMIC Adaptor: Received empty message or MAC command."));
    }
}

// New function to process data, adapted from old handleDownlinkCallback
void processReceivedLorawanData(uint8_t port, const uint8_t* data, int len) {
    Serial.println(F("Processing received LoRaWAN data..."));
    char jsonBuffer[MAX_JSON_SIZE];
    int bufferLen = (len < MAX_JSON_SIZE -1) ? len : MAX_JSON_SIZE -1;
    memcpy(jsonBuffer, data, bufferLen);
    jsonBuffer[bufferLen] = '\0';

    Serial.print(F("Payload (port ")); Serial.print(port); Serial.print(F("): "));
    Serial.println(jsonBuffer);

    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        bool success = processJsonPayload(String(jsonBuffer)); 
        if (success) {
            Serial.println(F("JSON payload processed successfully."));
            if (dmxInitialized && dmx) {
                dmx->sendData(); 
            }
        } else {
            Serial.println(F("Failed to process JSON payload."));
        }
        xSemaphoreGive(dmxMutex);
    } else {
        Serial.println(F("Could not obtain DMX mutex in RX callback!"));
    }

    if (dmxInitialized && dmx) {
    }
}

void loop() {
  // Get current time
  unsigned long currentMillis = millis();
  
  // Reset the watchdog timer
  esp_task_wdt_reset();
  
  // Handle LoRaWAN events
  if (loraWan) { // Check if loraWan object exists
    loraWan->loop(); // Call LMIC's run loop

    // If joined, enable Class C if it hasn't been enabled yet.
    // This handles the case where join completes after setup() finishes checking.
    static bool classCAttemptedPostJoin = false;
    if (loraWan->isJoined() && !classCAttemptedPostJoin) {
        Serial.println(F("LoRaWAN Joined (detected in loop)! Enabling Class C."));
        loraWan->enableClassC();
        classCAttemptedPostJoin = true;
    }
  }
  
  // Process message queue periodically
  static unsigned long lastQueueProcess = 0;
  if (currentMillis - lastQueueProcess >= 1000) { // Process queue every second
    lastQueueProcess = currentMillis;
    processMessageQueue(); 
  }
  
  // Heartbeat message every 60 seconds
  if (currentMillis - lastHeartbeat >= 60000) {
      lastHeartbeat = currentMillis;
      
     if (loraInitialized && loraWan && loraWan->isJoined()) {
          // Show device class in console
          // Serial.print("Current device class: "); // McciLmicWrapper does not have getDeviceClass() yet
          // char deviceClass = loraWan->getDeviceClass(); 
          // Serial.println(deviceClass);
      }
  }
  
  // Only update DMX data when changed (no periodic refresh here since it's handled by the dedicated task)
  if (runningRainbowDemo && dmxInitialized && dmx != NULL) {
    if (currentMillis - lastRainbowStep >= rainbowStepDelay) {
      lastRainbowStep = currentMillis;
      
      // Take mutex to safely update DMX data
      if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        // Generate rainbow colors
        dmx->updateRainbowStep(rainbowStepCounter++, rainbowStaggered);
        
        // Give mutex back after updating DMX data
        xSemaphoreGive(dmxMutex);
      }
    }
  }
  
  // Update pattern (if active)
  if (patternHandler.isActive()) {
    patternHandler.update();
  }
  
  // Yield to allow other tasks to run
  delay(1);
}

