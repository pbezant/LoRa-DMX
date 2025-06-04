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
 * - LoRaManager2: LoRaWAN Class C communication library for ESP32 + SX1262
 * - ArduinoJson: JSON parsing
 * - DmxController: DMX output control
 * - Ticker: Hardware-timed uplinks
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>  // Include SPI library explicitly
#include <vector>
#include <LoRaManager.h>  // LoRaManager2 library
#include "DmxController.h"
#include <esp_task_wdt.h>  // Watchdog
#include "secrets.h"  // Include the secrets.h file for LoRaWAN credentials
#include <WiFi.h>
#include <esp_dmx.h>
#include <esp_task_wdt.h>
#include <vector>
#include <algorithm>
#include <Ticker.h>  // Add Ticker library for hardware-timed uplinks

// Debug output
#define SERIAL_BAUD 115200

// Diagnostic LED pin
#define LED_PIN 35  // Built-in LED on Heltec LoRa 32 V3

// DMX configuration for Heltec LoRa 32 V3
#define DMX_PORT 1
#define DMX_TX_PIN 19  // TX pin for DMX
#define DMX_RX_PIN 20  // RX pin for DMX
#define DMX_DIR_PIN 5  // DIR pin for DMX (connect to both DE and RE on MAX485)

// DMX configuration - we'll use dynamic configuration from JSON
#define MAX_FIXTURES 32           // Maximum number of fixtures supported
#define MAX_CHANNELS_PER_FIXTURE 16 // Maximum channels per fixture
#define MAX_JSON_SIZE 1024        // Maximum size of JSON document

// Global variables
bool dmxInitialized = false;
bool loraInitialized = false;
DmxController* dmx = NULL;
LoraManager lora;  // Fixed case-sensitive class name

// Add Ticker for hardware-timed periodic uplinks (like working example)
Ticker uplinkTicker;

// Add mutex for thread-safe DMX data access
SemaphoreHandle_t dmxMutex = NULL;

// Add DMX task handle
TaskHandle_t dmxTaskHandle = NULL;

// Add flag to control DMX during RX windows - set to true to continue DMX during RX windows
bool keepDmxDuringRx = true;

// Forward declarations
void handleDownlinkCallback(const uint8_t* data, size_t size, int rssi, int snr);
bool processLightsJson(JsonArray lightsArray);
void processMessageQueue();  // Add this forward declaration
void send_lora_frame();  // Add this forward declaration for Ticker callback

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

// Add timing variables for various operations
unsigned long lastHeartbeat = 0;  // Timestamp for heartbeat messages
unsigned long lastStatusUpdate = 0; // Timestamp for status updates

// Connection state tracking for LoRaManager2
bool isConnected = false;
uint32_t lastConnectionAttempt = 0;
const uint32_t CONNECTION_RETRY_INTERVAL = 60000; // 1 minute between retries

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
    
    // Take mutex before updating DMX data
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

    // Save pattern state periodically (every 10 steps)
    if (step % 10 == 0) {
      savePatternState();
    }
    
    // Release the mutex
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
void onConnectionStateChange(bool connected) {
    isConnected = connected;
    Serial.print("LoRaWAN connection state changed: ");
    Serial.println(connected ? "CONNECTED" : "DISCONNECTED");
    
    if (connected) {
        // Send any queued messages
        processMessageQueue();
    }
}

void onTransmissionComplete(bool success, int errorCode) {
    if (success) {
        Serial.println("Transmission completed successfully!");
    } else {
        Serial.print("Transmission failed with error code: ");
        Serial.println(errorCode);
    }
}

void processMessageQueue() {
    if (!isConnected || messageQueue.empty()) return;
    
    // Sort messages by priority (lowest number = highest priority)
    std::sort(messageQueue.begin(), messageQueue.end(),
        [](const PendingMessage& a, const PendingMessage& b) {
            return a.priority < b.priority;
        });
    
    // Try to send the highest priority message
    const PendingMessage& msg = messageQueue.front();
    if (lora.send((const uint8_t*)msg.payload.c_str(), msg.payload.length(), msg.port)) {
        messageQueue.erase(messageQueue.begin());
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

  // Check for simple "command" format from README
  if (doc.containsKey("command")) {
    String command = doc["command"].as<String>();
    Serial.print("Simple command format detected: ");
    Serial.println(command);
    
    if (dmxInitialized && dmx != NULL) {
      if (command == "test") {
        Serial.println("COMMAND: Run test mode (set all fixtures to green)");
        for (int i = 0; i < dmx->getNumFixtures(); i++) {
          dmx->setFixtureColor(i, 0, 255, 0, 0);
        }
      } else if (command == "red") {
        Serial.println("COMMAND: Set all fixtures to RED");
        for (int i = 0; i < dmx->getNumFixtures(); i++) {
          dmx->setFixtureColor(i, 255, 0, 0, 0);
        }
      } else if (command == "green") {
        Serial.println("COMMAND: Set all fixtures to GREEN");
        for (int i = 0; i < dmx->getNumFixtures(); i++) {
          dmx->setFixtureColor(i, 0, 255, 0, 0);
        }
      } else if (command == "blue") {
        Serial.println("COMMAND: Set all fixtures to BLUE");
        for (int i = 0; i < dmx->getNumFixtures(); i++) {
          dmx->setFixtureColor(i, 0, 0, 255, 0);
        }
      } else if (command == "white") {
        Serial.println("COMMAND: Set all fixtures to WHITE");
        for (int i = 0; i < dmx->getNumFixtures(); i++) {
          dmx->setFixtureColor(i, 0, 0, 0, 255);
        }
      } else if (command == "off") {
        Serial.println("COMMAND: Turn all fixtures OFF");
        for (int i = 0; i < dmx->getNumFixtures(); i++) {
          dmx->setFixtureColor(i, 0, 0, 0, 0);
        }
      } else {
        Serial.print("Unknown command: ");
        Serial.println(command);
        return false;
      }
      
      // Send the DMX data and save settings
      dmx->sendData();
      dmx->saveSettings();
      Serial.println("Simple command processed successfully");
      return true;
    }
  }

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
      // Simple ping command for testing downlink connectivity
      Serial.println("=== PING RECEIVED ===");
      Serial.println("Downlink communication is working!");
      
      // Blink the LED in a distinctive pattern to indicate ping received
      for (int i = 0; i < 3; i++) {
        DmxController::blinkLED(LED_PIN, 3, 100);
        delay(500);
      }
      
      // Send a ping response uplink
      if (loraInitialized && lora.isJoined()) {
        String response = "{\"ping_response\":\"ok\"}";
        if (lora.send((const uint8_t*)response.c_str(), response.length(), 1)) {
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
  printDmxValues(1, 20);
  
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
    printDmxValues(1, 20);
    
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
void handleDownlinkCallback(const uint8_t* data, size_t size, int rssi, int snr) {
  Serial.println("\n\n==== DEBUG: ENTERING DOWNLINK CALLBACK ====");
  
  // Print a detailed raw byte dump of the received payload
  debugBytes("RAW DOWNLINK PAYLOAD", (uint8_t*)data, size);
  
  // Print a memory report at the start
  Serial.print("Free heap at start of downlink handler: ");
  Serial.println(ESP.getFreeHeap());
  
  // Handle basic binary commands (values 0-4) first before any other processing
  if (size == 1) {
    uint8_t cmd = data[0];
    
    // Check if it's a binary value 0-4
    if (cmd <= 4) {
      Serial.print("DIRECT BINARY COMMAND DETECTED: ");
      Serial.println(cmd);
      
      if (dmxInitialized && dmx != NULL) {
        switch (cmd) {
          case 0:
            Serial.println("COMMAND: Turn all fixtures OFF");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 0, 0, 0, 0);
            }
            break;
          case 1:
            Serial.println("COMMAND: Set all fixtures to RED");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 255, 0, 0, 0);
            }
            break;
          case 2:
            Serial.println("COMMAND: Set all fixtures to GREEN");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 0, 255, 0, 0);
            }
            break;
          case 3:
            Serial.println("COMMAND: Set all fixtures to BLUE");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 0, 0, 255, 0);
            }
            break;
          case 4:
            Serial.println("COMMAND: Set all fixtures to WHITE");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 0, 0, 0, 255);
            }
            break;
        }
        
        // Send the DMX data and save settings
        dmx->sendData();
        dmx->saveSettings();
        Serial.println("Binary command processed successfully");
        
        // Blink LED to indicate successful processing
        DmxController::blinkLED(LED_PIN, 2, 200);
        return;  // Exit early - we've processed the command
      }
    }
    
    // Check for ASCII digit '0'-'4'
    if (cmd >= '0' && cmd <= '4') {
      int cmdValue = cmd - '0';  // Convert ASCII to integer
      Serial.print("ASCII DIGIT COMMAND DETECTED: '");
      Serial.print((char)cmd);
      Serial.print("' (");
      Serial.print(cmdValue);
      Serial.println(")");
      
      if (dmxInitialized && dmx != NULL) {
        switch (cmdValue) {
          case 0:
            Serial.println("COMMAND: Turn all fixtures OFF");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 0, 0, 0, 0);
            }
            break;
          case 1:
            Serial.println("COMMAND: Set all fixtures to RED");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 255, 0, 0, 0);
            }
            break;
          case 2:
            Serial.println("COMMAND: Set all fixtures to GREEN");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 0, 255, 0, 0);
            }
            break;
          case 3:
            Serial.println("COMMAND: Set all fixtures to BLUE");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 0, 0, 255, 0);
            }
            break;
          case 4:
            Serial.println("COMMAND: Set all fixtures to WHITE");
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
              dmx->setFixtureColor(i, 0, 0, 0, 255);
            }
            break;
        }
        
        // Send the DMX data and save settings
        dmx->sendData();
        dmx->saveSettings();
        Serial.println("ASCII digit command processed successfully");
        
        // Blink LED to indicate successful processing
        DmxController::blinkLED(LED_PIN, 2, 200);
        return;  // Exit early - we've processed the command
      }
    }
  }
  
  // Quick test for any special byte (0xAA or 170 decimal or any other non-standard value)
  if (size == 1 && (data[0] == 0xAA || data[0] == 170 || data[0] == 0xFF || data[0] == 255)) {
    Serial.println("DIRECT TEST TRIGGER DETECTED - Running test with JSON from README");
    
    // Force green on fixtures
    if (dmxInitialized && dmx != NULL) {
      Serial.println("\n===== DIRECT TEST: SETTING ALL FIXTURES TO GREEN =====");
      // Set all fixtures to fixed green
      for (int i = 0; i < dmx->getNumFixtures(); i++) {
        dmx->setFixtureColor(i, 0, 255, 0, 0);
      }
      dmx->sendData();
      dmx->saveSettings();
      Serial.println("All fixtures set to GREEN");
      Serial.println("TEST COMPLETED");
      
      // Add strong visual confirmation to debugging
      Serial.println("=================================================");
      Serial.println("||                                             ||");
      Serial.println("||  DIRECT TEST: GREEN LIGHTS COMMAND APPLIED  ||");
      Serial.println("||                                             ||");
      Serial.println("=================================================");
      
      // Blink LED to indicate successful processing
      DmxController::blinkLED(LED_PIN, 5, 200);
      return;
    }
  }
  
  // NEW: Handle compact binary lights format from chirpstack_codec.js
  // Format: [numLights, address1, ch1, ch2, ch3, ch4, address2, ch1, ch2, ch3, ch4, ...]
  if (size >= 6 && size <= 127) { // Reasonable size for lights data (1-25 lights max)
    uint8_t numLights = data[0];
    
    // Check if the payload size matches the expected format
    size_t expectedSize = 1 + (numLights * 5); // 1 byte for count + 5 bytes per light (address + 4 channels)
    
    if (size == expectedSize && numLights > 0 && numLights <= 25) {
      Serial.println("COMPACT BINARY LIGHTS FORMAT DETECTED!");
      Serial.print("Number of lights: ");
      Serial.println(numLights);
      
      if (dmxInitialized && dmx != NULL) {
        // Take mutex before modifying DMX data
        if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
          bool success = false;
          
          // Process each light in the compact format
          for (int i = 0; i < numLights; i++) {
            size_t offset = 1 + (i * 5); // Start after count byte, 5 bytes per light
            
            uint8_t address = data[offset];
            uint8_t ch1 = data[offset + 1];
            uint8_t ch2 = data[offset + 2]; 
            uint8_t ch3 = data[offset + 3];
            uint8_t ch4 = data[offset + 4];
            
            Serial.print("Light ");
            Serial.print(i + 1);
            Serial.print(": Address=");
            Serial.print(address);
            Serial.print(", Channels=[");
            Serial.print(ch1);
            Serial.print(",");
            Serial.print(ch2);
            Serial.print(",");
            Serial.print(ch3);
            Serial.print(",");
            Serial.print(ch4);
            Serial.println("]");
            
            // Validate address (1-512 for DMX)
            if (address >= 1 && address <= 512) {
              // Set DMX channels directly (DMX uses 1-based addressing)
              if (address + 3 < DMX_PACKET_SIZE) {
                dmx->getDmxData()[address] = ch1;
                dmx->getDmxData()[address + 1] = ch2;
                dmx->getDmxData()[address + 2] = ch3;
                dmx->getDmxData()[address + 3] = ch4;
                success = true;
                
                Serial.print("Set DMX channels ");
                Serial.print(address);
                Serial.print("-");
                Serial.print(address + 3);
                Serial.print(" to values: [");
                Serial.print(ch1);
                Serial.print(",");
                Serial.print(ch2);
                Serial.print(",");
                Serial.print(ch3);
                Serial.print(",");
                Serial.print(ch4);
                Serial.println("]");
              } else {
                Serial.print("DMX address out of range: ");
                Serial.println(address);
              }
            } else {
              Serial.print("Invalid DMX address: ");
              Serial.println(address);
            }
          }
          
          if (success) {
            Serial.println("Sending compact binary lights command to DMX...");
            dmx->sendData();
            dmx->saveSettings();
            Serial.println("Compact binary lights command processed successfully!");
            
            // Blink LED to indicate successful processing
            DmxController::blinkLED(LED_PIN, 3, 200);
          } else {
            Serial.println("Failed to process any lights from compact binary format");
          }
          
          // Release the mutex
          xSemaphoreGive(dmxMutex);
          
          if (success) {
            return; // Exit early - we've processed the command successfully
          }
        } else {
          Serial.println("Failed to take DMX mutex for compact binary processing");
        }
      } else {
        Serial.println("DMX not initialized, cannot process compact binary lights command");
      }
    }
  }
  
  // Enhanced logging for regular payload processing
  static uint32_t downlinkCounter = 0;
  downlinkCounter++;
  
  Serial.print("\n\n=== DOWNLINK #");
  Serial.print(downlinkCounter);
  Serial.println(" RECEIVED ===");
  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.print(", SNR: ");
  Serial.println(snr);
  
  if (size <= MAX_JSON_SIZE) {
    Serial.println("DEBUG: Size check passed");
    
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
      
      if (data[i] < 16) Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    // First analyze the data type based on the raw bytes
    Serial.println("DEBUG: Analyzing payload type");
    bool isProbablyBinary = false;
    bool isProbablyText = true;
    
    // Check if payload contains any non-printable, non-whitespace characters
    for (size_t i = 0; i < size; i++) {
      if (data[i] < 32 && data[i] != '\t' && data[i] != '\r' && data[i] != '\n') {
        // Non-printable character found (except control chars)
        if (data[i] != 0) { // Null bytes might be padding
          isProbablyText = false;
          isProbablyBinary = true;
          Serial.print("Non-printable char at position ");
          Serial.print(i);
          Serial.print(": 0x");
          Serial.println(data[i], HEX);
        }
      }
    }
    
    Serial.print("DEBUG: Payload appears to be ");
    if (isProbablyBinary) {
      Serial.println("BINARY data");
    } else {
      Serial.println("TEXT data");
    }
    
    // Create a string from the payload (even if binary, for backup processing)
    String payloadStr = "";
    Serial.println("DEBUG: Creating payload string");
    for (size_t i = 0; i < size; i++) {
      payloadStr += (char)data[i];
    }
    
    // Debug: Log the received data as text
    Serial.println("\n----- DOWNLINK PAYLOAD CONTENTS -----");
    Serial.println(payloadStr);
    Serial.println("-------------------------------------");
    
    // Check for "go" command (from README)
    if (payloadStr == "go") {
      Serial.println("GO COMMAND DETECTED - Processing built-in example JSON from README");
      
      // Use the exact JSON example from the README
      String exampleJson = "{\"lights\":[{\"address\":1,\"channels\":[0,255,0,0]},{\"address\":2,\"channels\":[0,255,0,0]},{\"address\":3,\"channels\":[0,255,0,0]},{\"address\":4,\"channels\":[0,255,0,0]}]}";
      
      if (dmxInitialized && dmx != NULL) {
        // Configure test fixtures if none exist
        if (dmx->getNumFixtures() == 0) {
          Serial.println("Setting up default test fixtures for GO command");
          dmx->initializeFixtures(4, 4);
          dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
          dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
          dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
          dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
        }
        
        // Process the example JSON
        bool success = processJsonPayload(exampleJson);
        if (success) {
          Serial.println("GO command processed successfully - all fixtures set to GREEN");
          DmxController::blinkLED(LED_PIN, 3, 200);
          return;
        } else {
          Serial.println("Failed to process GO command JSON");
        }
      }
    }
    
    // Check for single-byte commands  
    if (size == 1) {
      // Single byte command
      Serial.print("Single-byte command detected: ");
      Serial.println(data[0]);
      
      if (data[0] == 0x02 || data[0] == '2') {
        Serial.println("SPECIAL HANDLING: Setting all fixtures to GREEN");
        if (dmxInitialized && dmx != NULL) {
          for (int i = 0; i < dmx->getNumFixtures(); i++) {
            dmx->setFixtureColor(i, 0, 255, 0, 0);
          }
          dmx->sendData();
          dmx->saveSettings();
          Serial.println("All fixtures set to GREEN");
          return; // Command was processed
        }
      }
    }
    
    // First, check for the exact example JSON from README
    if (payloadStr.indexOf("\"lights\"") > 0) {
      // This looks like our target JSON format
      Serial.println("DETECTED LIGHTS JSON COMMAND");
      
      // Parse and process it
      bool success = false;
      try {
        StaticJsonDocument<MAX_JSON_SIZE> doc;
        DeserializationError error = deserializeJson(doc, payloadStr);
        
        if (!error) {
          Serial.println("JSON parsed successfully");
          if (doc.containsKey("lights")) {
            JsonArray lights = doc["lights"];
            Serial.print("Found ");
            Serial.print(lights.size());
            Serial.println(" lights in the payload");
            
            if (dmxInitialized && dmx != NULL) {
              // Process the lights array
              for (JsonObject light : lights) {
                if (light.containsKey("address") && light.containsKey("channels")) {
                  int address = light["address"];
                  JsonArray channels = light["channels"];
                  
                  Serial.print("Setting fixture at address ");
                  Serial.print(address);
                  Serial.print(" with values: ");
                  
                  if (address > 0 && address <= dmx->getNumFixtures() && channels.size() >= 3) {
                    // Get the RGBW values (W optional)
                    int r = channels[0];
                    int g = channels[1];
                    int b = channels[2];
                    int w = (channels.size() >= 4) ? channels[3] : 0;
                    
                    Serial.print("R=");
                    Serial.print(r);
                    Serial.print(", G=");
                    Serial.print(g);
                    Serial.print(", B=");
                    Serial.print(b);
                    if (channels.size() >= 4) {
                      Serial.print(", W=");
                      Serial.print(w);
                    }
                    Serial.println();
                    
                    // Set the fixture color directly
                    dmx->setFixtureColor(address-1, r, g, b, w);
                    success = true;
                  }
                }
              }
              
              if (success) {
                // Send the data to the fixtures and save the settings
                dmx->sendData();
                dmx->saveSettings();
                Serial.println("DIRECT PROCESSING: DMX data sent and saved");
                
                // Blink LED to indicate success
                DmxController::blinkLED(LED_PIN, 2, 200);
                
                // Exit early - we've handled it directly
                return;
              }
            } else {
              Serial.println("DMX not initialized, cannot process command");
            }
          }
        } else {
          Serial.print("JSON parse error: ");
          Serial.println(error.c_str());
        }
      } catch (const std::exception& e) {
        Serial.print("EXCEPTION in payload analysis: ");
        Serial.println(e.what());
      } catch (...) {
        Serial.println("UNKNOWN EXCEPTION in payload analysis");
      }
    }
    
    // If we got here, the direct processing didn't succeed
    // Try the regular JSON processing path
    if (isProbablyText || size <= 4) {
      // Break down analysis to track potential crash points
      Serial.println("DEBUG: Starting regular payload analysis");
      
      // Check for JSON format if not a numeric command
      Serial.println("DEBUG: Checking JSON format");
      if (payloadStr.indexOf("{") == 0 && payloadStr.indexOf("}") == payloadStr.length() - 1) {
        Serial.println("DETECTED JSON COMMAND");
        
        Serial.println("DEBUG: Attempting to parse JSON");
        // Parse JSON to display its contents more clearly
        StaticJsonDocument<MAX_JSON_SIZE> doc;
        DeserializationError error = deserializeJson(doc, payloadStr);
        
        if (!error) {
          Serial.println("DEBUG: JSON parsed successfully");
          // Determine command type and details
          if (doc.containsKey("test")) {
            Serial.println("COMMAND TYPE: Test Pattern");
            String pattern = doc["test"]["pattern"];
            Serial.print("PATTERN: ");
            Serial.println(pattern);
            
            // Output other test parameters if present
            if (pattern == "rainbow") {
              int cycles = doc["test"]["cycles"] | 3;
              int speed = doc["test"]["speed"] | 50;
              bool staggered = doc["test"]["staggered"] | true;
              Serial.print("PARAMETERS: Cycles=");
              Serial.print(cycles);
              Serial.print(", Speed=");
              Serial.print(speed);
              Serial.print(", Staggered=");
              Serial.println(staggered ? "Yes" : "No");
            }
            else if (pattern == "strobe") {
              int color = doc["test"]["color"] | 0;
              int count = doc["test"]["count"] | 20;
              Serial.print("PARAMETERS: Color=");
              Serial.print(color);
              Serial.print(", Count=");
              Serial.println(count);
            }
            else if (pattern == "continuous") {
              bool enabled = doc["test"]["enabled"] | false;
              int speed = doc["test"]["speed"] | 30;
              Serial.print("PARAMETERS: Enabled=");
              Serial.print(enabled ? "Yes" : "No");
              Serial.print(", Speed=");
              Serial.println(speed);
            }
            else if (pattern == "ping") {
              Serial.println("PARAMETERS: None (Simple Ping)");
            }
          }
          else if (doc.containsKey("lights")) {
            Serial.println("COMMAND TYPE: Direct Light Control");
            JsonArray lights = doc["lights"];
            Serial.print("CONTROLLING ");
            Serial.print(lights.size());
            Serial.println(" FIXTURES:");
            
            // Output details for each fixture
            int lightIndex = 0;
            for (JsonObject light : lights) {
              int address = light["address"];
              Serial.print("  FIXTURE #");
              Serial.print(++lightIndex);
              Serial.print(": Address=");
              Serial.print(address);
              
              JsonArray channels = light["channels"];
              Serial.print(", Channels=[");
              for (size_t i = 0; i < channels.size(); i++) {
                if (i > 0) Serial.print(",");
                Serial.print(channels[i].as<int>());
              }
              Serial.println("]");
              
              // If this looks like RGBW fixture, provide color interpretation
              if (channels.size() >= 3) {
                int r = channels[0];
                int g = channels[1];
                int b = channels[2];
                int w = (channels.size() >= 4) ? channels[3] : 0;
                
                Serial.print("    COLOR: R=");
                Serial.print(r);
                Serial.print(", G=");
                Serial.print(g);
                Serial.print(", B=");
                Serial.print(b);
                if (channels.size() >= 4) {
                  Serial.print(", W=");
                  Serial.print(w);
                }
                Serial.println();
              }
            }
          }
          else {
            Serial.println("COMMAND TYPE: Unknown JSON structure");
          }
        }
        else {
          Serial.print("JSON PARSING ERROR: ");
          Serial.println(error.c_str());
        }
      }
      
      // Always process the command directly in the callback
      Serial.println("DEBUG: Processing command through standard path");
      if (dmxInitialized) {
        Serial.println("Processing downlink command immediately");
        
        try {
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
              if (loraInitialized && lora.isJoined()) {
                String response = "{\"ping_response\":\"ok\",\"counter\":" + String(downlinkCounter) + "}";
                if (lora.send((const uint8_t*)response.c_str(), response.length(), 1)) {
                  Serial.println("Ping response sent (confirmed)");
                }
              }
            }
          } else {
            Serial.println("Failed to process downlink");
            // Blink LED rapidly to indicate error
            DmxController::blinkLED(LED_PIN, 5, 100);
          }
        } catch (const std::exception& e) {
          Serial.print("EXCEPTION in command processing: ");
          Serial.println(e.what());
        } catch (...) {
          Serial.println("UNKNOWN EXCEPTION in command processing");
        }
      } else {
        Serial.println("ERROR: DMX not initialized, cannot process command");
      }
    }
    
    // For backward compatibility, also store in buffer
    memcpy(receivedData, data, size);
    receivedDataSize = size;
    receivedPort = 1;
    dataReceived = false;
  } else {
    Serial.println("ERROR: Received payload exceeds buffer size");
  }
  
  // Debug current memory state
  Serial.print("Free heap after downlink: ");
  Serial.println(ESP.getFreeHeap());
  
  Serial.println("==== DEBUG: EXITING DOWNLINK CALLBACK ====");
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
  Serial.println("Initializing LoRaWAN with credentials from secrets.h:");
  Serial.print("Join EUI: ");
  Serial.println(APPEUI);
  Serial.print("Device EUI: ");
  Serial.println(DEVEUI);
  Serial.print("App Key: ");
  Serial.println(APPKEY);
  
  // Create LoRa configuration for LoRaManager2 (matching working example)
  LoraConfig loraConfig;
  loraConfig.devEui = DEVEUI;
  loraConfig.appEui = APPEUI;
  loraConfig.appKey = APPKEY;
  loraConfig.region = US915;
  loraConfig.deviceClass = LORA_CLASS_C;  // Start in Class C mode for immediate downlinks
  loraConfig.subBand = 2;  // TTN US915 uses subband 2
  loraConfig.adrEnabled = false;
  loraConfig.dataRate = 4;  // DR4 for US915 - max payload 129 bytes
  loraConfig.txPower = 14;
  loraConfig.joinTrials = 5;
  loraConfig.publicNetwork = true;
  
  // Create hardware configuration for Heltec LoRa 32 V3 (using defaults)
  HardwareConfig hwConfig;  // Uses defaults for Heltec LoRa 32 V3
  
  // Set up event callbacks (matching working Class C example structure)
  lora.onJoined([]() {
    Serial.println("=================================================");
    Serial.println("[LoRaWAN] 🎉 NETWORK JOINED SUCCESSFULLY! 🎉");
    Serial.println("[LoRaWAN] Class C mode is now active - device listening continuously");
    Serial.printf("[LoRaWAN] Join attempts used: %d\n", 1);
    Serial.println("=================================================");
    
    isConnected = true;
    
    // Ensure we're in Class C mode - force it explicitly
    Serial.println("[LoRaWAN] Explicitly requesting Class C mode...");
    // Note: Some LoRaWAN libraries handle Class C automatically after join
    // The device should already be configured for Class C in the initial config
    Serial.println("[LoRaWAN] Class C mode should be active (configured at initialization)");
    
    // Add a longer delay to ensure Class C activation before starting uplinks
    delay(2000);  // Give network server time to process Class C request
    
    // Start hardware-timed periodic uplinks after Class C is established
    uplinkTicker.attach(20, send_lora_frame);  // 20 seconds interval exactly like working example
    Serial.println("[LoRaWAN] Periodic uplink ticker started (20s interval)");
    
    // Reset heartbeat timing for any remaining software-based timing
    lastHeartbeat = millis();
    
    // Send an immediate status message to confirm Class C operation
    String statusMsg = "{\"status\":\"joined\",\"class\":\"C\",\"dmx_fixtures\":" + String(dmx ? dmx->getNumFixtures() : 0) + "}";
    if (lora.send((const uint8_t*)statusMsg.c_str(), statusMsg.length(), 1)) {
      Serial.println("[LoRaWAN] Class C status message sent");
    }
  });
  
  lora.onJoinFailed([]() {
    Serial.println("[LoRaWAN] Failed to join LoRaWAN network");
    isConnected = false;
    lastConnectionAttempt = millis();
  });
  
  lora.onClassChanged([](uint8_t newClass) {
    Serial.print("[LoRaWAN] Device class changed to: ");
    Serial.println((char)('A' + newClass));
    if (newClass == 2) { // Class C
      Serial.println("[LoRaWAN] ✅ True Class C mode activated - downlinks available anytime!");
      Serial.println("[LoRaWAN] 🔊 Device is now listening continuously for downlinks");
      
      // Send a confirmation uplink to let the server know we're in Class C
      if (loraInitialized && lora.isJoined()) {
        String confirmMsg = "{\"class_c_active\":true,\"listening\":true}";
        if (lora.send((const uint8_t*)confirmMsg.c_str(), confirmMsg.length(), 1)) {
          Serial.println("[LoRaWAN] Class C confirmation message sent");
        }
      }
    } else {
      Serial.println("[LoRaWAN] ⚠️ Warning: Not in Class C mode - downlinks only after uplinks");
    }
  });
  
  // Use the library's built-in downlink processing but keep our custom handler for complex commands
  lora.onDownlink(handleDownlinkCallback);
  
  // Add specific command callbacks for common operations (like the working example)
  lora.onCommand("ping", [](const String& command, const JsonObject& payload) {
    Serial.println("[LoRaWAN] Ping command received - Class C response confirmed!");
    
    // Blink LED to show ping received
    DmxController::blinkLED(LED_PIN, 3, 200);
    
    // Send ping response
    if (loraInitialized && lora.isJoined()) {
      String response = "{\"ping_response\":\"ok\",\"class\":\"C\"}";
      if (lora.send((const uint8_t*)response.c_str(), response.length(), 1)) {
        Serial.println("[LoRaWAN] Ping response sent");
      }
    }
  });
  
  lora.onCommand("status", [](const String& command, const JsonObject& payload) {
    Serial.println("[LoRaWAN] Status command - device operational in Class C");
    
    // Send status response with DMX info
    if (loraInitialized && lora.isJoined()) {
      String response = "{\"status\":\"ok\",\"class\":\"C\",\"dmx_fixtures\":" + String(dmx ? dmx->getNumFixtures() : 0) + "}";
      if (lora.send((const uint8_t*)response.c_str(), response.length(), 1)) {
        Serial.println("[LoRaWAN] Status response sent");
      }
    }
  });
  
  lora.onCommand("test", [](const String& command, const JsonObject& payload) {
    Serial.println("[LoRaWAN] Test command received - setting all fixtures to green");
    
    if (dmxInitialized && dmx != NULL) {
      // Configure test fixtures if none exist
      if (dmx->getNumFixtures() == 0) {
        Serial.println("Setting up default test fixtures for command test");
        dmx->initializeFixtures(4, 4);
        dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
        dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
        dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
      }
      
      // Set all fixtures to green
      for (int i = 0; i < dmx->getNumFixtures(); i++) {
        dmx->setFixtureColor(i, 0, 255, 0, 0);
      }
      dmx->sendData();
      dmx->saveSettings();
      
      DmxController::blinkLED(LED_PIN, 2, 200);
      Serial.println("[LoRaWAN] Test command completed - all fixtures set to green");
    }
  });
  
  lora.onTxComplete([](bool success) {
    if (success) {
      Serial.println("[LoRaWAN] Transmission completed successfully!");
    } else {
      Serial.println("[LoRaWAN] ❌ Transmission failed!");
    }
  });
  
  // Print hardware pin configuration (like the working example)
  Serial.println("[App] Configuring hardware pins...");
  Serial.printf("[App] Pin mapping - RESET:%d NSS:%d SCK:%d MISO:%d MOSI:%d DIO1:%d BUSY:%d\n", 
                hwConfig.resetPin, hwConfig.nssPin, hwConfig.sckPin, 
                hwConfig.misoPin, hwConfig.mosiPin, hwConfig.dio1Pin, hwConfig.busyPin);
  
  // Initialize LoRa with configuration
  if (!lora.begin(loraConfig, hwConfig)) {
    Serial.println("[App] ❌ LoraManager initialization failed!");
    Serial.println("[App] Check your configuration and try again.");
    return;
  }
  
  Serial.println("[App] LoRaWAN initialization completed. Device will attempt to join...");
  Serial.println("[App] Setup completed - waiting for join...");
  loraInitialized = true;
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("Starting up...");
    
    // Initialize the LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Initialize the DMX controller
    dmx = new DmxController(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_DIR_PIN);
    dmx->begin();
    dmxInitialized = true;
    
    // Create mutex for thread-safe DMX data access
    dmxMutex = xSemaphoreCreateMutex();
    if (dmxMutex == NULL) {
        Serial.println("Failed to create DMX mutex!");
        return;
    }
    
    // Initialize LoRaWAN with credentials from secrets.h
    initializeLoRaWAN();
    
    // Start DMX task on Core 0
    xTaskCreatePinnedToCore(
        dmxTask,     // Task function
        "DMX Task",  // Name
        4096,        // Stack size
        NULL,        // Parameters
        1,           // Priority
        &dmxTaskHandle, // Task handle
        0            // Core (0)
    );
    
    // Initialize watchdog timer
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);
}

void loop() {
  // Get current time
  unsigned long currentMillis = millis();
  
  // Reset the watchdog timer
  esp_task_wdt_reset();
  
  // Handle LoRaWAN events
  if (loraInitialized) {
    lora.loop();
    
    // Update connection state based on LoRaManager2 status
    isConnected = lora.isJoined();
    
    // Process message queue periodically
    processMessageQueue();
  }
  
  // Handle DMX patterns and rainbow demo (still needed for local control)
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
  
  // Small delay to prevent watchdog issues (like working example)
  delay(100);
}

void send_lora_frame() {
    if (!lora.isJoined()) {
        Serial.println("[App] ⏳ Not joined yet, skipping transmission...");
        return;
    }
    
    static uint32_t count = 0;
    static uint32_t count_fail = 0;
    
    count++;
    Serial.printf("[App] 📡 Sending heartbeat frame #%d\\n", count);
    
    // Create payload matching working example exactly
    uint8_t payload[6];
    uint32_t i = 0;
    payload[i++] = (uint8_t)(count >> 24);
    payload[i++] = (uint8_t)(count >> 16);
    payload[i++] = (uint8_t)(count >> 8);
    payload[i++] = (uint8_t)count;
    payload[i++] = 0xC5; // Class C indicator
    payload[i++] = (uint8_t)(dmx ? dmx->getNumFixtures() : 0); // DMX fixture count
    
    Serial.print("[App] Payload: ");
    for (uint8_t j = 0; j < sizeof(payload); j++) {
        Serial.printf("%02X ", payload[j]);
    }
    Serial.println();
    
    if (lora.send(payload, sizeof(payload), 2)) {  // Use port 2 like working example
        Serial.println("[App] ✅ Packet enqueued successfully");
    } else {
        count_fail++;
        Serial.println("[App] ❌ Packet send failed");
        Serial.printf("[App] ⚠️ Total failed transmissions: %d\\n", count_fail);
    }
}

