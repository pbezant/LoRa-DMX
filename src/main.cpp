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
#include "DmxController.h"
#include <esp_task_wdt.h>  // Watchdog
#include "secrets.h"  // Include the secrets.h file for LoRaWAN credentials
#include "config.h"           // Pin configuration
#include "LoRaWANManager.h"  // Modular LoRaWAN logic

// Debug output
#define SERIAL_BAUD 115200

// Diagnostic LED pin
#define LED_PIN 35  // Built-in LED on Heltec LoRa 32 V3

// LoRaWAN Class Configuration
#define DEFAULT_LORAWAN_CLASS LORAWAN_CLASS_C // Use Class C by default
#define DEFAULT_LORAWAN_REGION "US915"        // US915 region
#define DEFAULT_LORAWAN_SUBBAND 2             // Sub-band 2 (channels 8-15)

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

// LoRaWAN credentials from secrets.h (already defined)
// Use hex strings for AppKey and NwkKey

// DMX configuration - we'll use dynamic configuration from JSON
#define MAX_FIXTURES 32           // Maximum number of fixtures supported
#define MAX_CHANNELS_PER_FIXTURE 16 // Maximum channels per fixture
#define MAX_JSON_SIZE 1024        // Maximum size of JSON document

// Global variables
bool dmxInitialized = false;
bool loraInitialized = false;
DmxController* dmx = NULL;

// Add mutex for thread-safe DMX data access
SemaphoreHandle_t dmxMutex = NULL;

// Add DMX task handle
TaskHandle_t dmxTaskHandle = NULL;

// Add flag to control DMX during RX windows - set to true to continue DMX during RX windows
bool keepDmxDuringRx = true;

// Forward declarations
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port);
bool processLightsJson(JsonArray lightsArray);
void processMessageQueue();  // Add this forward declaration

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
  
  // Singleton implementation
  static DmxPattern& getInstance() {
    static DmxPattern instance;
    return instance;
  }

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
  // Make constructor private (for singleton pattern)
  DmxPattern() : active(false), patternType(NONE), speed(50), step(0), lastUpdate(0), cycleCount(0), maxCycles(5), staggered(true) {}
  
  // Delete copy constructor and assignment operator
  DmxPattern(const DmxPattern&) = delete;
  DmxPattern& operator=(const DmxPattern&) = delete;

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
// Remove this line: DmxPattern patternHandler;

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
    if (LoRaWANManager::getInstance().sendString(msg.payload, msg.port, msg.confirmed)) {
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
          DmxPattern::getInstance().stop();
          return true;
        }
        
        if (patternType != DmxPattern::NONE) {
          DmxPattern::getInstance().start(patternType, speed, cycles);
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
        DmxPattern::getInstance().stop();
        return true;
      }
      
      if (type != DmxPattern::NONE) {
        Serial.println("Starting pattern...");
        DmxPattern::getInstance().start(type, speed, cycles);
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
      if (loraInitialized) {
        String response = "{\"ping_response\":\"ok\",\"counter\":" + String(downlinkCounter) + "}";
        if (LoRaWANManager::getInstance().sendString(response, 1, true)) {
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
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port) {
  Serial.println("\n\n==== DEBUG: ENTERING DOWNLINK CALLBACK ====");
  
  // Print a detailed raw byte dump of the received payload
  debugBytes("RAW DOWNLINK PAYLOAD", payload, size);
  
  // Print a memory report at the start
  Serial.print("Free heap at start of downlink handler: ");
  Serial.println(ESP.getFreeHeap());
  
  // Handle basic binary commands (values 0-4) first before any other processing
  if (size == 1) {
    uint8_t cmd = payload[0];
    
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
  if (size == 1 && (payload[0] == 0xAA || payload[0] == 170 || payload[0] == 0xFF || payload[0] == 255)) {
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
  
  // Enhanced logging for regular payload processing
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
      
      if (payload[i] < 16) Serial.print("0");
      Serial.print(payload[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    // First analyze the data type based on the raw bytes
    Serial.println("DEBUG: Analyzing payload type");
    bool isProbablyBinary = false;
    bool isProbablyText = true;
    
    // Check if payload contains any non-printable, non-whitespace characters
    for (size_t i = 0; i < size; i++) {
      if (payload[i] < 32 && payload[i] != '\t' && payload[i] != '\r' && payload[i] != '\n') {
        // Non-printable character found (except control chars)
        if (payload[i] != 0) { // Null bytes might be padding
          isProbablyText = false;
          isProbablyBinary = true;
          Serial.print("Non-printable char at position ");
          Serial.print(i);
          Serial.print(": 0x");
          Serial.println(payload[i], HEX);
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
      payloadStr += (char)payload[i];
    }
    
    // Debug: Log the received data as text
    Serial.println("\n----- DOWNLINK PAYLOAD CONTENTS -----");
    Serial.println(payloadStr);
    Serial.println("-------------------------------------");
    
    // First, check for the exact example JSON from README
    if (size == 1) {
      // Single byte command
      Serial.print("Single-byte command detected: ");
      Serial.println(payload[0]);
      
      if (payload[0] == 0x02 || payload[0] == '2') {
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
    } else if (payloadStr.indexOf("\"lights\"") > 0) {
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
              if (LoRaWANManager::getInstance().sendString(response, 1, true)) {
                Serial.println("Ping response sent (confirmed)");
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
    memcpy(receivedData, payload, size);
    receivedDataSize = size;
    receivedPort = port;
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
 * Handle incoming downlink data from LoRaWAN
 * 
 * @param payload The payload data
 * @param size The size of the payload
 * @param port The port on which the data was received
 */
void handleDownlink(uint8_t* payload, size_t size, uint8_t port) {
    Serial.printf("Received downlink on port %d, size: %d\n", port, size);
    
    try {
        // Convert payload to string
        String payloadStr = "";
        for (size_t i = 0; i < size; i++) {
            payloadStr += (char)payload[i];
        }
        
        // Process the payload
        if (dmxInitialized) {
            bool success = processJsonPayload(payloadStr);
            if (success) {
                // Queue confirmation message with high priority (50)
                String response = "{\"ack\":true,\"port\":" + String(port) + "}";
                queueMessage(response, 1, true, 50);
                
                DmxController::blinkLED(LED_PIN, 2, 200);
            } else {
                // Queue error message with high priority (50)
                String response = "{\"ack\":false,\"error\":\"processing_failed\"}";
                queueMessage(response, 1, true, 50);
                
                DmxController::blinkLED(LED_PIN, 5, 100);
            }
        }
    } catch (const std::exception& e) {
        Serial.print("Error processing downlink: ");
        Serial.println(e.what());
        
        // Queue error message with high priority (50)
        String response = "{\"ack\":false,\"error\":\"exception\"}";
        queueMessage(response, 1, true, 50);
    }
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
  Serial.println("Initializing LoRaWAN...");

  // Configure SPI for LoRa radio
  SPI.begin(LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI, LORA_CS_PIN);
  delay(100);
  
  // Print pin configuration for debugging
  Serial.println("Begin LoRaWAN with pins:");
  Serial.print("CS: "); Serial.print(LORA_CS_PIN);
  Serial.print(", DIO1: "); Serial.print(LORA_DIO1_PIN);
  Serial.print(", RST: "); Serial.print(LORA_RESET_PIN);
  Serial.print(", BUSY: "); Serial.println(LORA_BUSY_PIN);
  
  // Get singleton instance
  auto& loraManager = LoRaWANManager::getInstance();
  
  // Initialize with pin config and keys
  if (!loraManager.begin(LORA_CS_PIN, LORA_DIO1_PIN, LORA_RESET_PIN, LORA_BUSY_PIN,
                        DEVEUI, APPEUI, APPKEY,
                        DEFAULT_LORAWAN_CLASS, DEFAULT_LORAWAN_REGION, DEFAULT_LORAWAN_SUBBAND)) {
    Serial.println("Failed to initialize LoRaWAN manager");
    return;
  }
  
  // Set downlink callback
  loraManager.setDownlinkCallback(handleDownlinkCallback);
  
  // Set event callback to handle LoRaWAN events
  loraManager.setEventCallback([](uint8_t event) {
    switch(event) {
      case LoRaWANManager::EV_INIT_SUCCESS:
        Serial.println("LoRaWAN initialization successful");
        break;
      case LoRaWANManager::EV_INIT_FAILED:
        Serial.println("LoRaWAN initialization failed");
        break;
      case LoRaWANManager::EV_JOIN_STARTED:
        Serial.println("LoRaWAN join started");
        break;
      case LoRaWANManager::EV_JOIN_SUCCESS:
        Serial.println("LoRaWAN join successful");
        // Blink LED to indicate join success
        for (int i = 0; i < 3; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(100);
          digitalWrite(LED_PIN, LOW);
          delay(100);
        }
        break;
      case LoRaWANManager::EV_JOIN_FAILED:
        Serial.println("LoRaWAN join failed");
        // Blink LED to indicate join failure
        for (int i = 0; i < 5; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(50);
          digitalWrite(LED_PIN, LOW);
          delay(50);
        }
        break;
      case LoRaWANManager::EV_TX_COMPLETE:
        Serial.println("LoRaWAN transmission complete");
        break;
      case LoRaWANManager::EV_RX_RECEIVED:
        Serial.println("LoRaWAN downlink received");
        break;
    }
  });
  
  // Join network
  Serial.println("Joining LoRaWAN network...");
  if (loraManager.joinNetwork()) {
    Serial.println("Join process started. Check for join events.");
    loraInitialized = true;
  } else {
    Serial.println("Failed to start join process");
    
    // Blink LED to indicate failure
    for (int i = 0; i < 4; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
  }
}

void setup() {
  // Initialize serial port
  Serial.begin(SERIAL_BAUD);
  delay(1000);  // Give time for serial to connect
  
  Serial.println("\nLoRa DMX Controller Starting...");
  Serial.println("==================================");
  
  // Set up diagnostic LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Set up watchdog with 30 second timeout
  esp_task_wdt_init(WDT_TIMEOUT, true);  // Enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);  // Add current thread to WDT watch
  
  // Initialize DMX
  dmxMutex = xSemaphoreCreateMutex();
  dmx = new DmxController();
  if (dmx->begin()) {
    dmxInitialized = true;
    Serial.println("DMX initialized successfully");
  } else {
    Serial.println("DMX initialization failed");
  }
  
  // Create DMX task
  xTaskCreatePinnedToCore(
    dmxTask,           // Function to implement the task
    "DMXTask",         // Name of the task
    4096,              // Stack size in words
    NULL,              // Task input parameter
    1,                 // Priority of the task (higher = higher priority)
    &dmxTaskHandle,    // Task handle
    1                  // Core where the task should run (core 1)
  );
  
  // Initialize LoRaWAN
  initializeLoRaWAN();
  
  // Successful initialization blink
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  esp_task_wdt_reset();  // Reset watchdog timer
  Serial.println("Setup complete");
}

void loop() {
  // Reset watchdog timer
  esp_task_wdt_reset();
  
  // Process LoRaWAN events if initialized
  if (loraInitialized) {
    // Get the singleton instance
    auto& loraManager = LoRaWANManager::getInstance();
    
    // Handle LoRaWAN events
    loraManager.handleEvents();
  }
  
  // Handle message queue
  processMessageQueue();
  
  // Handle patterns
  if (dmxInitialized) {
    // Make sure we use the correct static method
    DmxPattern::getInstance().update();
  }
  
  // Send a heartbeat ping every 60 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - lastHeartbeat > 60000) {
    lastHeartbeat = currentMillis;
    
    if (loraInitialized) {
      auto& loraManager = LoRaWANManager::getInstance();
      
      // Only send if we're joined to the network
      if (loraManager.isJoined()) {
        // Lower priority heartbeat message (255 = lowest)
        String heartbeat = "{\"status\":\"heartbeat\",\"uptime\":" + String(currentMillis / 1000) + "}";
        queueMessage(heartbeat, 2, false, 255);
      }
    }
  }
  
  // Short delay to prevent WDT issues and allow other tasks to run
  delay(10);
}

