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
#include <Heltec_LoRaWAN_Wrapper.h> // ADDED - New Heltec LoRaWAN wrapper
#include "config.h"           // Pin configuration

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
DmxController* dmx = NULL;
unsigned long downlinkCounter = 0; // Counter for downlink messages

// Create global instance of the Heltec LoRaWAN Wrapper
HeltecLoRaWANWrapper myLoRaWAN; // As suggested by the wrapper's comments

// Add mutex for thread-safe DMX data access
SemaphoreHandle_t dmxMutex = NULL;

// Add DMX task handle
TaskHandle_t dmxTaskHandle = NULL;

// Add flag to control DMX during RX windows - set to true to continue DMX during RX windows
bool keepDmxDuringRx = true;

// Forward declarations
void handleDownlink(uint8_t* payload, uint16_t size, uint8_t port);
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
    dmx->sendDmxData();

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
    Serial.print("LoRaWAN connection state changed (via callback): ");
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
    if (messageQueue.empty() || !isConnected) { // isConnected needs to be updated by LoRaWAN events
        return;
    }
    
    // Sort by priority (lower number = higher priority), then by timestamp (older first)
    std::sort(messageQueue.begin(), messageQueue.end(),
        [](const PendingMessage& a, const PendingMessage& b) {
            if (a.priority != b.priority) {
            return a.priority < b.priority;
            }
            return a.timestamp < b.timestamp;
        });
    
    // Try to send the highest priority message
    const PendingMessage& msg = messageQueue.front();
    
    // Convert String payload to uint8_t* and size for Heltec wrapper
    // Max payload size for sendUplink should be respected (LORAWAN_APP_DATA_MAX_SIZE in wrapper)
    uint8_t payloadBuffer[LORAWAN_APP_DATA_MAX_SIZE]; // Use define from wrapper
    size_t payloadSize = msg.payload.length();
    if (payloadSize > LORAWAN_APP_DATA_MAX_SIZE) { // Check against LORAWAN_APP_DATA_MAX_SIZE
        Serial.println("Error: Queued payload too large for buffer!");
        payloadSize = LORAWAN_APP_DATA_MAX_SIZE; // Truncate
    }
    msg.payload.getBytes(payloadBuffer, payloadSize + 1); // getBytes includes null terminator if space allows
    // If payloadSize is from .length(), it doesn't include null. sendUplink needs actual data length.

    // Note: msg.port and msg.confirmed are ignored by current myLoRaWAN.sendUplink
    // It uses the global port and confirmed status set during wrapper init.
    if (myLoRaWAN.sendUplink(payloadBuffer, payloadSize)) {
        Serial.print("Uplink sent from queue (port & confirmed status are global): ");
        Serial.println(msg.payload);
        messageQueue.erase(messageQueue.begin());
    } else {
        Serial.println("Failed to send uplink from queue via Heltec wrapper.");
        // Consider if the message should remain in queue or if there's a retry mechanism.
        // For now, if Heltec lib fails to queue, it might be due to duty cycle or internal state.
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
      if (dmxInitialized) {
        String pingResponseStr = "{\"ping_response\":\"ok\",\"counter\":" + String(downlinkCounter) + "}";
        
        uint8_t pingResponseBuffer[LORAWAN_APP_DATA_MAX_SIZE]; // Use define from wrapper
        size_t pingResponseSize = pingResponseStr.length();
        if (pingResponseSize > LORAWAN_APP_DATA_MAX_SIZE) { // Check against LORAWAN_APP_DATA_MAX_SIZE
            pingResponseSize = LORAWAN_APP_DATA_MAX_SIZE; // Truncate
        }
        pingResponseStr.getBytes(pingResponseBuffer, pingResponseSize + 1);

        // Note: Port 1 and confirmed status true are ignored by current wrapper sendUplink
        if (myLoRaWAN.sendUplink(pingResponseBuffer, pingResponseSize)) {
          Serial.println("Ping response sent (port & confirmed status are global).");
        } else {
          Serial.println("Failed to send ping response via Heltec wrapper.");
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
    
    dmx->sendDmxData();
    
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

// New downlink handler function to be registered with the Heltec wrapper
void handleDownlink(uint8_t* payload, uint16_t size, uint8_t port) {
    Serial.print("[Main] Downlink received via Heltec Wrapper. Port: ");
  Serial.print(port);
  Serial.print(", Size: ");
  Serial.println(size);
  
    Serial.print("Payload (HEX): ");
    for (int i = 0; i < size; i++) {
        if (payload[i] < 0x10) Serial.print("0");
      Serial.print(payload[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    char jsonBuffer[size + 1];
    memcpy(jsonBuffer, payload, size);
    jsonBuffer[size] = '\0';
    String jsonString = String(jsonBuffer);

    Serial.println("Processing JSON payload:");
    Serial.println(jsonString);

    // Store the result of payload processing
    bool processingSuccess = processJsonPayload(jsonString);

    if (processingSuccess) {
        downlinkCounter++;
        Serial.print("Downlink processed successfully. Total downlinks: ");
        Serial.println(downlinkCounter);

        // If the processed command was a ping, send a response
        // We need to parse the jsonString again or pass the JsonDocument from processJsonPayload
        StaticJsonDocument<MAX_JSON_SIZE> doc; // Re-parse for ping check - could be optimized
        deserializeJson(doc, jsonString);
        if (doc.containsKey("test") && doc["test"].is<JsonObject>()) {
            JsonObject testObj = doc["test"];
            if (testObj.containsKey("pattern") && testObj["pattern"].as<String>() == "ping") {
                Serial.println("Ping detected, sending response...");
                String pingResponseStr = "{\"ping_response\":\"ok\",\"counter\":" + String(downlinkCounter) + "}";
                
                uint8_t pingResponseBuffer[LORAWAN_APP_DATA_MAX_SIZE]; // Use define from wrapper
                size_t pingResponseSize = pingResponseStr.length();
                if (pingResponseSize > LORAWAN_APP_DATA_MAX_SIZE) { // Check against LORAWAN_APP_DATA_MAX_SIZE
                    pingResponseSize = LORAWAN_APP_DATA_MAX_SIZE; // Truncate
                }
                pingResponseStr.getBytes(pingResponseBuffer, pingResponseSize + 1);

                // Note: Port 1 and confirmed status true are ignored by current wrapper sendUplink
                if (myLoRaWAN.sendUplink(pingResponseBuffer, pingResponseSize)) {
                    Serial.println("Ping response sent (port & confirmed status are global).");
                } else {
                    Serial.println("Failed to send ping response via Heltec wrapper.");
                }
            }
              }
            } else {
        Serial.println("Failed to process JSON payload from downlink.");
    }
}

void setup() {
    // Serial.begin(SERIAL_BAUD); // Wrapper calls Serial.begin()
    // pinMode(LED_PIN, OUTPUT); // Wrapper might handle LED or not, check if needed
    
    // Heltec wrapper's begin() handles Heltec.begin, Serial, WiFi/BT off, Display off.
    myLoRaWAN.begin(); 

    // Register the downlink handler
    myLoRaWAN.onDownlink(handleDownlink);
    // Register the connection status handler
    myLoRaWAN.onConnectionStatusChange(onConnectionStateChange);

    Serial.println("Initializing DMX...");
    dmx = new DmxController(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_DIR_PIN);
    if (dmx->begin()) {
        dmxInitialized = true;
        Serial.println("DMX Initialized.");
        dmx->clearAllChannels(); // Clear DMX channels at startup
        dmx->sendDmxData(); // Send the cleared data to fixtures
          } else {
        Serial.println("DMX Initialization Failed!");
    }

    // Initialize Watchdog
    esp_task_wdt_init(WDT_TIMEOUT, true); // Enable panic on timeout
    esp_task_wdt_add(NULL); // Add current task to WDT

    // Initialize DMX Mutex
    dmxMutex = xSemaphoreCreateMutex();
    if (dmxMutex == NULL) {
        Serial.println("Failed to create DMX mutex!");
    }

    // Create DMX update task (if you have one, or if DMX updates are in main loop)
    // xTaskCreatePinnedToCore(dmxTask, "DMXTask", 4096, NULL, 5, &dmxTaskHandle, 1);

    // Restore pattern state if needed
    DmxPattern::getInstance().restorePatternState();

    Serial.println("Setup complete. Device ready.");
    digitalWrite(LED_PIN, LOW); // Turn LED on (LOW for Heltec V3 built-in LED)
}

void loop() {
    myLoRaWAN.loop(); // Process LoRaWAN stack

    // Process any pending DMX commands or patterns
    if (dmxInitialized && dmx != NULL) {
        if (xSemaphoreTake(dmxMutex, (TickType_t)10) == pdTRUE) {
            DmxPattern::getInstance().update(); // Update DMX patterns
            if (runningRainbowDemo && (millis() - lastRainbowStep > rainbowStepDelay)) {
                // ... (continuous rainbow logic - keep as is) ...
            }
            // dmx->update(); // DmxPattern usually calls dmx->update() or sets channels
        xSemaphoreGive(dmxMutex);
      }
    }
    
    processMessageQueue(); // Process any queued uplink messages

    // Heartbeat LED and Watchdog Reset
    if (millis() - lastHeartbeat > 1000) {
        lastHeartbeat = millis();
        // digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Blink LED - Handled by wrapper/LoRa events?
        esp_task_wdt_reset(); // Reset WDT
    }

    // Handle other periodic tasks if any
    // ...
}

// Ensure existing functions like processMessageQueue, queueMessage, processJsonPayload, etc.
// are adapted to use `myLoRaWAN.sendUplink` and the new downlink mechanism.
// The LoRaWAN_Event_Callback is provided by the wrapper. If `main.cpp` had its own,
// the logic might need to be merged or use the wrapper's version.
// The `OnRxData` in the wrapper calls the registered `_downlinkCallback`.

// Example modification for queueMessage (you'll need to find the actual function)
// This is a conceptual change, adapt to your actual queueMessage function.
/*
void queueMessage(const String& payloadStr, uint8_t port, bool confirmed, uint8_t priority) {
    // ... (your existing message queuing logic) ...
    
    // When ready to send:
    // Convert String payloadStr to uint8_t* buffer and size
    uint8_t buffer[payloadStr.length() + 1]; // Or LORAWAN_APP_DATA_SIZE
    payloadStr.getBytes(buffer, payloadStr.length() + 1);
    uint16_t size = payloadStr.length();

    if(myLoRaWAN.sendUplink(buffer, size)) {
        Serial.println("Uplink added to Heltec queue.");
  } else {
        Serial.println("Failed to add uplink to Heltec queue.");
    }
}
*/

// Make sure the LoRaWAN_Event_Callback function in main.cpp is removed if it duplicates
// the one in Heltec_LoRaWAN_Wrapper.h, or ensure it's the one being called.
// The wrapper provides one that prints to Serial. If you need custom event handling,
// you might modify the wrapper's callback or ensure this one is correctly linked.
// For now, rely on the wrapper's provided LoRaWAN_Event_Callback.

// The global OnRxData in the wrapper will call the 'handleDownlink' function registered via onDownlink.
// So, the old handleDownlinkCallback might be redundant if handleDownlink now serves that purpose.

