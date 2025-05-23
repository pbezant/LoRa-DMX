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
#include "LoRaManager.h" // This is for the original LoRa implementation
#include "DmxController.h"
#include <esp_task_wdt.h>  // Watchdog
#include "secrets.h"  // Include the secrets.h file for LoRaWAN credentials

// Includes for LoRaWrapper
#include "heltec_board_config.h"  // MUST be included before Heltec headers
#include "ILoRaWanDevice.h"
#include "HeltecLoRaWan.h"

// Debug output
#define SERIAL_BAUD 115200

// Diagnostic LED pin
#define LED_PIN 35  // Built-in LED on Heltec LoRa 32 V3

// Global LoRaWAN device instance - declared early for visibility in all functions
ILoRaWanDevice* loraDevice = nullptr;

// DMX configuration for Heltec LoRa 32 V3
#define DMX_PORT 1
#define DMX_TX_PIN 19  // TX pin for DMX
#define DMX_RX_PIN 20  // RX pin for DMX
#define DMX_DIR_PIN 5  // DIR pin for DMX (connect to both DE and RE on MAX485)

// LoRaWAN TTN Connection Parameters (Potentially for LoRaManager, may not be used by LoRaWrapper)
#define LORA_CS_PIN   8     // Corrected CS pin for Heltec LoRa 32 V3
#define LORA_DIO1_PIN 14    // DIO1 pin
#define LORA_RESET_PIN 12   // Reset pin  
#define LORA_BUSY_PIN 13    // Busy pin

// SPI pins for Heltec LoRa 32 V3 (Potentially for LoRaManager)
#define LORA_SPI_SCK  9   // SPI clock
#define LORA_SPI_MISO 11  // SPI MISO
#define LORA_SPI_MOSI 10  // SPI MOSI

// LoRaWAN Credentials from secrets.h (used by LoRaManager)
uint64_t joinEUI_orig = 0; // Renamed to avoid conflict if LoRaWrapper uses a similar name
uint64_t devEUI_orig = 0;  // Renamed to avoid conflict

// Function to convert hex string to uint64_t for the EUIs (used by LoRaManager)
uint64_t hexStringToUint64(const char* hexStr) {
  uint64_t result = 0;
  for (int i = 0; i < 16 && hexStr[i] != '\0'; i++) {
    char c = hexStr[i];
    uint8_t nibble = 0;
    if (c >= '0' && c <= '9') {
      nibble = c - '0';
    } else if (c >= 'A' && c <= 'F') {
      nibble = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
      nibble = c - 'a' + 10;
    }
    result = (result << 4) | nibble;
  }
  return result;
}

// DMX configuration
#define MAX_FIXTURES 32
#define MAX_CHANNELS_PER_FIXTURE 16
#define MAX_JSON_SIZE 1024

// Global variables for DMX application
bool dmxInitialized = false;
// bool loraInitialized = false; // This might be managed by LoRaWrapper now
DmxController* dmx = NULL;
LoRaManager* lora = NULL; // This is for the original LoRa implementation. Consider if it should be replaced by loraDevice

SemaphoreHandle_t dmxMutex = NULL;
TaskHandle_t dmxTaskHandle = NULL;
bool keepDmxDuringRx = true;

// Forward declarations for DMX application
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port); // This is likely for LoRaManager
bool processLightsJson(JsonArray lightsArray);
void processMessageQueue(); 

uint8_t receivedData[MAX_JSON_SIZE];
size_t receivedDataSize = 0;
uint8_t receivedPort = 0;
// bool dataReceived = false; // Replaced by direct callback processing

bool runningRainbowDemo = false;
unsigned long lastRainbowStep = 0;
uint32_t rainbowStepCounter = 0;
int rainbowStepDelay = 30;
bool rainbowStaggered = true;

unsigned long lastHeartbeat = 0;
// unsigned long lastStatusUpdate = 0; // Not used

// bool processInCallback = true; // Not used in this structure

#define WDT_TIMEOUT 30

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

struct PatternState {
  bool isActive;
  uint8_t patternType;
  int speed;
  int maxCycles;
  bool staggered;
  uint32_t step;
};

class DmxPattern {
public:
  enum PatternType { NONE, COLOR_FADE, RAINBOW, STROBE, CHASE, ALTERNATE };
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
    savePatternState();
  }

  void stop() {
    active = false;
    patternType = NONE;
    Serial.println("Pattern stopped");
    clearSavedPatternState();
  }
  
  bool isActive() { return active; }

  void savePatternState() {
    if (!dmxInitialized || dmx == NULL) return;
    PatternState state;
    state.isActive = active;
    state.patternType = (uint8_t)patternType;
    state.speed = speed;
    state.maxCycles = maxCycles;
    state.staggered = staggered;
    state.step = step;
    dmx->saveCustomData("pattern_state", (uint8_t*)&state, sizeof(PatternState));
    Serial.println("Pattern state saved to persistent storage");
  }

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
        cycleCount = 0;
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

  void clearSavedPatternState() {
    if (!dmxInitialized || dmx == NULL) return;
    PatternState state;
    memset(&state, 0, sizeof(PatternState));
    dmx->saveCustomData("pattern_state", (uint8_t*)&state, sizeof(PatternState));
    Serial.println("Pattern state cleared from persistent storage");
  }
  
  void update() {
    if (!active || !dmxInitialized || dmx == NULL) return;
    unsigned long now = millis();
    if (now - lastUpdate < speed) return;
    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) != pdTRUE) {
      Serial.println("Failed to take DMX mutex for pattern update");
      return;
    }
    lastUpdate = now;
    switch (patternType) {
      case COLOR_FADE: updateColorFade(); break;
      case RAINBOW: updateRainbow(); break;
      case STROBE: updateStrobe(); break;
      case CHASE: updateChase(); break;
      case ALTERNATE: updateAlternate(); break;
      default: break;
    }
    dmx->sendData();
    if (step % 10 == 0) savePatternState();
    xSemaphoreGive(dmxMutex);
  }

private:
  bool active;
  PatternType patternType;
  int speed;
  uint32_t step;
  unsigned long lastUpdate;
  int cycleCount;
  int maxCycles;
  bool staggered;
  
  void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    float c = v * s;
    float x = c * (1 - abs(fmod(h / 60.0, 2) - 1));
    float m = v - c;
    float r1, g1, b1;
    if (h < 60) { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }
    r = (r1 + m) * 255; g = (g1 + m) * 255; b = (b1 + m) * 255;
  }
  
  void updateColorFade() {
    float hue = (step % 360); step = (step + 2) % 360;
    uint8_t r, g, b; hsvToRgb(hue, 1.0, 1.0, r, g, b);
    int numFixtures = dmx->getNumFixtures();
    for (int i = 0; i < numFixtures; i++) dmx->setFixtureColor(i, r, g, b, 0);
    if (step == 0) { cycleCount++; if (cycleCount >= maxCycles && maxCycles > 0) stop(); }
  }
  
  void updateRainbow() {
    int numFixtures = dmx->getNumFixtures(); if (numFixtures == 0) return;
    int baseHue = step % 360; step = (step + 5) % 360;
    for (int i = 0; i < numFixtures; i++) {
      float hue = fmod(baseHue + (360.0 * i / numFixtures), 360);
      uint8_t r, g, b; hsvToRgb(hue, 1.0, 1.0, r, g, b);
      dmx->setFixtureColor(i, r, g, b, 0);
    }
    if (step == 0) { cycleCount++; if (cycleCount >= maxCycles && maxCycles > 0) stop(); }
  }
  
  void updateStrobe() {
    bool isOn = (step % 2) == 0; step++;
    int numFixtures = dmx->getNumFixtures();
    for (int i = 0; i < numFixtures; i++) {
      if (isOn) dmx->setFixtureColor(i, 255, 255, 255, 255);
      else dmx->setFixtureColor(i, 0, 0, 0, 0);
    }
    if (step % 2 == 0) { cycleCount++; if (cycleCount >= maxCycles && maxCycles > 0) stop(); }
  }
  
  void updateChase() {
    int numFixtures = dmx->getNumFixtures(); if (numFixtures == 0) return;
    int activeFixture = step % numFixtures; step = (step + 1) % numFixtures;
    for (int i = 0; i < numFixtures; i++) {
      if (i == activeFixture) {
        uint8_t r, g, b; float hue = (cycleCount * 30) % 360;
        hsvToRgb(hue, 1.0, 1.0, r, g, b);
        dmx->setFixtureColor(i, r, g, b, 0);
      } else dmx->setFixtureColor(i, 0, 0, 0, 0);
    }
    if (step == 0) { cycleCount++; if (cycleCount >= maxCycles && maxCycles > 0) stop(); }
  }
  
  void updateAlternate() {
    int numFixtures = dmx->getNumFixtures(); bool flipState = (step % 2) == 0; step++;
    for (int i = 0; i < numFixtures; i++) {
      bool isOn = (i % 2 == 0) ? flipState : !flipState;
      if (isOn) {
        uint8_t r, g, b; float hue = (cycleCount * 40) % 360;
        hsvToRgb(hue, 1.0, 1.0, r, g, b);
        dmx->setFixtureColor(i, r, g, b, 0);
      } else dmx->setFixtureColor(i, 0, 0, 0, 0);
    }
    if (step % 2 == 0) { cycleCount++; if (cycleCount >= maxCycles && maxCycles > 0) stop(); }
  }
};

DmxPattern patternHandler;
bool isConnected = false; // For LoRaManager
uint32_t lastConnectionAttempt = 0; // For LoRaManager
const uint32_t CONNECTION_RETRY_INTERVAL = 60000; // For LoRaManager

struct PendingMessage {
    String payload; uint8_t port; bool confirmed; uint8_t priority; uint32_t timestamp;
};
std::vector<PendingMessage> messageQueue; // For LoRaManager
const size_t MAX_QUEUE_SIZE = 10; // For LoRaManager

// Event callbacks for LoRaManager
void onConnectionStateChange(bool connected) {
    isConnected = connected;
    Serial.print("LoRaManager: Connection state changed: "); Serial.println(connected ? "CONNECTED" : "DISCONNECTED");
    if (connected) processMessageQueue();
}

void onTransmissionComplete(bool success, int errorCode) {
    if (success) Serial.println("LoRaManager: Transmission completed successfully!");
    else { Serial.print("LoRaManager: Transmission failed with error code: "); Serial.println(errorCode); }
}

void processMessageQueue() { // For LoRaManager
    if (!isConnected || messageQueue.empty() || !lora) return;
    std::sort(messageQueue.begin(), messageQueue.end(), [](const PendingMessage& a, const PendingMessage& b) { return a.priority < b.priority; });
    const PendingMessage& msg = messageQueue.front();
    if (lora->sendString(msg.payload, msg.port, msg.confirmed)) messageQueue.erase(messageQueue.begin());
}

void queueMessage(const String& payload, uint8_t port, bool confirmed, uint8_t priority) { // For LoRaManager
    if (messageQueue.size() >= MAX_QUEUE_SIZE) {
        auto lowestPriority = std::max_element(messageQueue.begin(), messageQueue.end(), [](const PendingMessage& a, const PendingMessage& b) { return a.priority < b.priority; });
        if (lowestPriority != messageQueue.end() && lowestPriority->priority > priority) messageQueue.erase(lowestPriority);
        else { Serial.println("LoRaManager: Message queue full, priority too low"); return; }
    }
    PendingMessage msg = {payload, port, confirmed, priority, millis()};
    messageQueue.push_back(msg);
    if (isConnected) processMessageQueue();
}

bool processJsonPayload(const String& jsonString) {
  StaticJsonDocument<MAX_JSON_SIZE> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) { Serial.print("JSON parsing error: "); Serial.println(error.c_str()); return false; }
  Serial.print("Processing JSON payload: "); Serial.println(jsonString);

  if (doc.containsKey("pattern")) {
    if (doc["pattern"].is<JsonObject>()) {
      JsonObject pattern = doc["pattern"];
      if (pattern.containsKey("type")) {
        String type = pattern["type"]; int speed = pattern["speed"] | 50; int cycles = pattern["cycles"] | 5;
        DmxPattern::PatternType patternType = DmxPattern::NONE;
        if (type == "colorFade") patternType = DmxPattern::COLOR_FADE;
        else if (type == "rainbow") patternType = DmxPattern::RAINBOW;
        else if (type == "strobe") { patternType = DmxPattern::STROBE; speed = pattern["speed"] | 100; }
        else if (type == "chase") patternType = DmxPattern::CHASE;
        else if (type == "alternate") patternType = DmxPattern::ALTERNATE;
        else if (type == "stop") { patternHandler.stop(); return true; }
        if (patternType != DmxPattern::NONE) { patternHandler.start(patternType, speed, cycles); return true; }
      }
    } else if (doc["pattern"].is<String>()) {
      String patternTypeStr = doc["pattern"].as<String>(); Serial.print("Simple pattern format: "); Serial.println(patternTypeStr);
      int speed = 50; int cycles = 5; DmxPattern::PatternType type = DmxPattern::NONE;
      if (patternTypeStr == "colorFade") type = DmxPattern::COLOR_FADE;
      else if (patternTypeStr == "rainbow") { type = DmxPattern::RAINBOW; cycles = 3; }
      else if (patternTypeStr == "strobe") { type = DmxPattern::STROBE; speed = 100; cycles = 10; }
      else if (patternTypeStr == "chase") { type = DmxPattern::CHASE; speed = 200; cycles = 3; }
      else if (patternTypeStr == "alternate") { type = DmxPattern::ALTERNATE; speed = 300; cycles = 5; }
      else if (patternTypeStr == "stop") { patternHandler.stop(); return true; }
      if (type != DmxPattern::NONE) { Serial.println("Starting pattern..."); patternHandler.start(type, speed, cycles); return true; }
    }
  }

  if (doc.containsKey("test")) {
    JsonObject testObj = doc["test"];
    if (!testObj.containsKey("pattern")) { Serial.println("JSON error: 'pattern' missing in test object"); return false; }
    String pattern = testObj["pattern"].as<String>(); pattern.toLowerCase();
    Serial.print("Processing test pattern: "); Serial.println(pattern);
    if (pattern == "rainbow") {
      int cycles = max(1, min(testObj["cycles"] | 3, 10)); int speed = max(10, min(testObj["speed"] | 50, 500));
      bool staggered_local = testObj["staggered"] | true;
      Serial.printf("Rainbow chase: Cycles=%d, Speed=%dms, Staggered=%s\n", cycles, speed, staggered_local ? "Yes" : "No");
      if (dmx->getNumFixtures() == 0) {
        Serial.println("Default test fixtures for rainbow"); dmx->initializeFixtures(4, 4);
        dmx->setFixtureConfig(0, "F1", 1, 1, 2, 3, 4); dmx->setFixtureConfig(1, "F2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "F3", 9, 9, 10, 11, 12); dmx->setFixtureConfig(3, "F4", 13, 13, 14, 15, 16);
      }
      dmx->runRainbowChase(cycles, speed, staggered_local); dmx->saveSettings(); return true;
    } else if (pattern == "strobe") {
      int color = max(0, min(testObj["color"] | 0, 3)); int count = max(1, min(testObj["count"] | 20, 100));
      int onTime = max(10, min(testObj["onTime"] | 50, 1000)); int offTime = max(10, min(testObj["offTime"] | 50, 1000));
      bool alternate = testObj["alternate"] | false;
      Serial.printf("Strobe: Color=%d, Count=%d, On=%dms, Off=%dms, Alternate=%s\n", color, count, onTime, offTime, alternate ? "Yes" : "No");
       if (dmx->getNumFixtures() == 0) {
        Serial.println("Default test fixtures for strobe"); dmx->initializeFixtures(4, 4);
        dmx->setFixtureConfig(0, "F1", 1, 1, 2, 3, 4); dmx->setFixtureConfig(1, "F2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "F3", 9, 9, 10, 11, 12); dmx->setFixtureConfig(3, "F4", 13, 13, 14, 15, 16);
      }
      dmx->runStrobeTest(color, count, onTime, offTime, alternate); dmx->saveSettings(); return true;
    } else if (pattern == "continuous") {
      bool enabled = testObj["enabled"] | false; int speed = max(5, min(testObj["speed"] | 30, 500));
      bool new_staggered = testObj["staggered"] | true;
      runningRainbowDemo = enabled; rainbowStepDelay = speed; rainbowStaggered = new_staggered;
      Serial.printf("Continuous rainbow: %s, Speed=%dms, Staggered=%s\n", enabled ? "ENABLED" : "DISABLED", speed, new_staggered ? "Yes" : "No");
      if (dmx->getNumFixtures() == 0) {
        Serial.println("Default test fixtures for continuous rainbow"); dmx->initializeFixtures(4, 4);
        dmx->setFixtureConfig(0, "F1", 1, 1, 2, 3, 4); dmx->setFixtureConfig(1, "F2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "F3", 9, 9, 10, 11, 12); dmx->setFixtureConfig(3, "F4", 13, 13, 14, 15, 16);
      }
      if (!enabled) { runningRainbowDemo = false; Serial.println("Continuous rainbow disabled"); dmx->saveSettings(); }
      return true;
    } else if (pattern == "ping") {
      Serial.println("=== PING RECEIVED ==="); Serial.println("Downlink communication is working!");
      for (int i = 0; i < 3; i++) { DmxController::blinkLED(LED_PIN, 3, 100); delay(500); }
      // Ping response via LoRaWrapper - temporarily disabled due to scope issues
      // if (::loraDevice && ::loraDevice->isJoined()) { 
      //   String response = "{\"ping_response\":\"ok\"}";
      //   Serial.printf("LoRaWrapper: Would send ping response here: %s\n", response.c_str());
      // }
      return true;
    } else { Serial.print("Unknown test pattern: "); Serial.println(pattern); return false; }
  }
  
  if (doc.containsKey("lights")) {
    JsonArray lightsArray = doc["lights"];
    bool success = processLightsJson(lightsArray);
    if (success) return true;
    else { Serial.println("Failed to process lights array"); return false; }
  }
  Serial.println("JSON error: missing 'lights', 'pattern', or 'test' object"); return false;
}

bool processLightsJson(JsonArray lightsArray) {
  if (!dmxInitialized || dmx == NULL) { Serial.println("DMX not initialized"); return false; }
  Serial.println("\n===== PROCESSING DOWNLINK LIGHTS COMMAND ====="); printDmxValues(1, 20);
  bool atLeastOneValid = false;
  if (xSemaphoreTake(dmxMutex, portMAX_DELAY) != pdTRUE) { Serial.println("Failed to take DMX mutex"); return false; }
  for (JsonObject light : lightsArray) {
    if (!light.containsKey("address")) { Serial.println("Light missing 'address'"); continue; }
    int address = light["address"].as<int>();
    if (address < 1 || address > 512) { Serial.print("Invalid DMX address: "); Serial.println(address); continue; }
    if (!light.containsKey("channels")) { Serial.println("Light missing 'channels'"); continue; }
    JsonArray channelsArray = light["channels"];
    if (channelsArray.size() == 0) { Serial.println("Empty channels array"); continue; }
    Serial.printf("Setting light at address %d with %d channels:\n", address, channelsArray.size());
    int channelIndex = 0;
    for (JsonVariant channelValue : channelsArray) {
      int value = max(0, min(channelValue.as<int>(), 255));
      int dmxChannel = address + channelIndex;
      Serial.printf("  Channel %d = %d\n", dmxChannel, value);
      if (dmxChannel < DMX_PACKET_SIZE) { dmx->getDmxData()[dmxChannel] = value; channelIndex++; }
      else { Serial.print("DMX channel out of range: "); Serial.println(dmxChannel); break; }
    }
    atLeastOneValid = true;
    Serial.print("Set DMX address "); Serial.print(address); Serial.print(" to values: [");
    for (int i = 0; i < channelsArray.size(); i++) { if (i > 0) Serial.print(", "); Serial.print(dmx->getDmxData()[address + i]); }
    Serial.println("]");
  }
  if (atLeastOneValid) {
    Serial.println("Sending updated DMX values..."); printDmxValues(1, 20);
    dmx->sendData(); dmx->saveSettings(); Serial.println("DMX settings saved");
  }
  xSemaphoreGive(dmxMutex);
  return atLeastOneValid;
}

void debugBytes(const char* label, uint8_t* data, size_t size) {
  Serial.print(label); Serial.printf(" (%d bytes):\n", size);
  Serial.print("HEX: ");
  for (size_t i = 0; i < size; i++) { if (data[i] < 16) Serial.print("0"); Serial.print(data[i], HEX); Serial.print(" "); }
  Serial.println();
  Serial.print("ASCII: \"");
  for (size_t i = 0; i < size; i++) { Serial.print((data[i] >= 32 && data[i] <= 126) ? (char)data[i] : '.'); }
  Serial.println("\"");
}

// This is the downlink callback for LoRaManager
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port) {
  Serial.println("\n\n==== LoRaManager: DEBUG: ENTERING DOWNLINK CALLBACK ====");
  debugBytes("LoRaManager: RAW DOWNLINK PAYLOAD", payload, size);
  Serial.print("LoRaManager: Free heap at start of downlink: "); Serial.println(ESP.getFreeHeap());

  if (size == 1) {
    uint8_t cmd = payload[0];
    if (cmd <= 4) { // Binary 0-4
      Serial.print("LoRaManager: DIRECT BINARY COMMAND: "); Serial.println(cmd);
      if (dmxInitialized && dmx != NULL) {
        // Simplified color setting
        for (int i = 0; i < dmx->getNumFixtures(); i++) {
            if (cmd==0) dmx->setFixtureColor(i,0,0,0,0); else if (cmd==1) dmx->setFixtureColor(i,255,0,0,0);
            else if (cmd==2) dmx->setFixtureColor(i,0,255,0,0); else if (cmd==3) dmx->setFixtureColor(i,0,0,255,0);
            else if (cmd==4) dmx->setFixtureColor(i,0,0,0,255);
        }
        dmx->sendData(); dmx->saveSettings(); Serial.println("LoRaManager: Binary command processed");
        DmxController::blinkLED(LED_PIN, 2, 200); return;
      }
    }
    if (cmd >= '0' && cmd <= '4') { // ASCII '0'-'4'
        int cmdValue = cmd - '0';
        Serial.printf("LoRaManager: ASCII DIGIT COMMAND: '%c' (%d)\n", (char)cmd, cmdValue);
         if (dmxInitialized && dmx != NULL) {
            for (int i = 0; i < dmx->getNumFixtures(); i++) {
                if (cmdValue==0) dmx->setFixtureColor(i,0,0,0,0); else if (cmdValue==1) dmx->setFixtureColor(i,255,0,0,0);
                else if (cmdValue==2) dmx->setFixtureColor(i,0,255,0,0); else if (cmdValue==3) dmx->setFixtureColor(i,0,0,255,0);
                else if (cmdValue==4) dmx->setFixtureColor(i,0,0,0,255);
            }
            dmx->sendData(); dmx->saveSettings(); Serial.println("LoRaManager: ASCII digit command processed");
            DmxController::blinkLED(LED_PIN, 2, 200); return;
        }
    }
  }

  if (size == 1 && (payload[0] == 0xAA || payload[0] == 170 || payload[0] == 0xFF || payload[0] == 255)) {
    Serial.println("LoRaManager: DIRECT TEST TRIGGER - GREEN LIGHTS");
    if (dmxInitialized && dmx != NULL) {
      for (int i = 0; i < dmx->getNumFixtures(); i++) dmx->setFixtureColor(i, 0, 255, 0, 0);
      dmx->sendData(); dmx->saveSettings(); Serial.println("LoRaManager: All fixtures GREEN");
      DmxController::blinkLED(LED_PIN, 5, 200); return;
    }
  }

  static uint32_t downlinkCounter = 0; downlinkCounter++;
  Serial.printf("\n\n=== LoRaManager: DOWNLINK #%u RECEIVED ===\nPort: %u, Size: %u\n", downlinkCounter, port, size);

  if (size <= MAX_JSON_SIZE) {
    digitalWrite(LED_PIN, HIGH); delay(50); digitalWrite(LED_PIN, LOW);
    String payloadStr = ""; for (size_t i = 0; i < size; i++) payloadStr += (char)payload[i];
    Serial.println("\n----- LoRaManager: DOWNLINK PAYLOAD CONTENTS -----"); Serial.println(payloadStr); Serial.println("-------------------------------------");
    
    bool isProbablyText = true;
    for (size_t i = 0; i < size; i++) if (payload[i] < 32 && payload[i] != '\t' && payload[i] != '\r' && payload[i] != '\n' && payload[i] != 0) isProbablyText = false;

    if (payloadStr.indexOf("\"lights\"") > 0) { // Specific "lights" JSON check
        Serial.println("LoRaManager: DETECTED LIGHTS JSON COMMAND");
        // Simplified direct processing for "lights"
        StaticJsonDocument<MAX_JSON_SIZE> doc; DeserializationError error = deserializeJson(doc, payloadStr);
        if (!error && doc.containsKey("lights") && dmxInitialized && dmx) {
            JsonArray lights = doc["lights"]; bool success = false;
            for (JsonObject light : lights) {
                if (light.containsKey("address") && light.containsKey("channels")) {
                    int address = light["address"]; JsonArray channels = light["channels"];
                    if (address > 0 && address <= dmx->getNumFixtures() && channels.size() >= 3) {
                        dmx->setFixtureColor(address-1, channels[0], channels[1], channels[2], (channels.size() >= 4) ? channels[3] : 0);
                        success = true;
                    }
                }
            }
            if (success) { dmx->sendData(); dmx->saveSettings(); Serial.println("LoRaManager: DIRECT LIGHTS JSON processed"); DmxController::blinkLED(LED_PIN,2,200); return; }
        }
    }
    
    if (isProbablyText || size <= 4) { // General JSON processing if text or short
        Serial.println("LoRaManager: Processing command through standard path");
        if (dmxInitialized) {
            if (processJsonPayload(payloadStr)) {
                Serial.println("LoRaManager: Successfully processed downlink"); DmxController::blinkLED(LED_PIN, 2, 200);
                // Ping response via LoRaWrapper - temporarily disabled due to scope issues
                // if (::loraDevice && ::loraDevice->isJoined()) { 
                //     String response = "{\"ping_response\":\"ok\",\"counter\":" + String(downlinkCounter) + "}";
                //     Serial.printf("LoRaWrapper: Would send ping response from callback here: %s\n", response.c_str());
                // }
            } else { Serial.println("LoRaManager: Failed to process downlink"); DmxController::blinkLED(LED_PIN, 5, 100); }
        } else Serial.println("LoRaManager ERROR: DMX not initialized");
    }
    // Backward compatibility buffer - consider removing if LoRaWrapper is primary
    // memcpy(receivedData, payload, size); receivedDataSize = size; receivedPort = port; // dataReceived = false; 
  } else Serial.println("LoRaManager ERROR: Received payload exceeds buffer size");
  Serial.print("LoRaManager: Free heap after downlink: "); Serial.println(ESP.getFreeHeap());
  Serial.println("==== LoRaManager: DEBUG: EXITING DOWNLINK CALLBACK ====");
}


void dmxTask(void * parameter) {
  vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
  Serial.println("DMX task started on Core 0");
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(20); // 20ms -> 50Hz
  while(true) {
    if (dmxInitialized && dmx != NULL) {
      if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        dmx->sendData();
        xSemaphoreGive(dmxMutex);
      }
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// This is the LoRaWAN initialization for LoRaManager
bool loraInitialized = false; // Moved here, was global. For LoRaManager.
void initializeLoRaWAN_Manager() { 
  joinEUI_orig = hexStringToUint64(APPEUI); // Use renamed globals
  devEUI_orig = hexStringToUint64(DEVEUI);  // Use renamed globals

  Serial.println("LoRaManager: Initializing with credentials from secrets.h:");
  Serial.print("LoRaManager: Join EUI: "); Serial.println(APPEUI);
  Serial.print("LoRaManager: Device EUI: "); Serial.println(DEVEUI);
  Serial.print("LoRaManager: App Key: "); Serial.println(APPKEY);
  
  if (!lora->begin(LORA_CS_PIN, LORA_DIO1_PIN, LORA_RESET_PIN, LORA_BUSY_PIN)) { // LoRaManager specific pins
    Serial.println("LoRaManager: LoRa initialization failed!"); return;
  }
  
  // For LoRaManager, NWKKEY might be part of AppKey or a separate define
  const char* nwkKeyToUse = NWKKEY; // Assuming NWKKEY is defined in secrets.h, might be same as APPKEY for LoRaWAN 1.0.x
  if (!lora->setCredentialsHex(joinEUI_orig, devEUI_orig, APPKEY, nwkKeyToUse)) {
    Serial.println("LoRaManager: Failed to set credentials!"); return;
  }
  
  lora->setDownlinkCallback(handleDownlinkCallback); // For LoRaManager
  
  Serial.println("LoRaManager: Attempting to join LoRaWAN network...");
  if (lora->joinNetwork()) {
    Serial.println("LoRaManager: Successfully joined the network!"); isConnected = true; // For LoRaManager
    Serial.println("LoRaManager: Switching to Class C mode...");
    if (lora->setDeviceClass(DEVICE_CLASS_C)) Serial.println("LoRaManager: Successfully switched to Class C!");
    else Serial.println("LoRaManager: Failed to switch to Class C, staying Class A");
  } else {
    Serial.println("LoRaManager: Failed to join network, will retry later"); lastConnectionAttempt = millis(); // For LoRaManager
  }
  loraInitialized = true; // For LoRaManager
}


// Helper function to convert LoRaWAN credential strings from secrets.h to byte arrays FOR LORAWWRAPPER
void hexStringToByteArray_wrapper(const char* hexString, uint8_t* byteArray, int byteLen) { //Renamed
    for (int i = 0; i < byteLen; i++) {
        char byteChars[3] = {hexString[i * 2], hexString[i * 2 + 1], 0};
        byteArray[i] = strtol(byteChars, nullptr, 16);
    }
}

// Byte arrays for LoRaWAN credentials FOR LORAWWRAPPER
uint8_t actual_dev_eui[8];
uint8_t actual_app_eui[8];
uint8_t actual_app_key[16];
// uint8_t actual_nwk_key[16]; // If using LoRaWAN 1.1 and have a separate NWKKEY for LoRaWrapper

// Callback handler for LoRaWrapper
class MyAppLoRaCallbacks : public ILoRaWanCallbacks {
public:
    void onJoined() override {
        Serial.println("LoRaWrapper: MyApp: LoRaWAN Joined successfully!");
    }

    void onJoinFailed() override {
        Serial.println("LoRaWrapper: MyApp: LoRaWAN Join Failed. Check credentials, coverage, or antenna.");
    }

    void onDataReceived(const uint8_t* data, uint8_t len, uint8_t port, int16_t rssi, int8_t snr) override {
        Serial.printf("LoRaWrapper: MyApp: Data Received! Port: %d, RSSI: %d, SNR: %d, Length: %d\n", port, rssi, snr, len);
        Serial.print("LoRaWrapper: MyApp: Payload: ");
        for (int i = 0; i < len; i++) Serial.printf("%02X ", data[i]);
        Serial.println();
        
        // Here, you could call processJsonPayload or a similar function
        // tailored for data received via LoRaWrapper
        String payloadStr = "";
        for(int i=0; i<len; i++) payloadStr += (char)data[i];
        Serial.println("LoRaWrapper: Processing received payload string:");
        Serial.println(payloadStr);
        processJsonPayload(payloadStr); // Example: reuse existing JSON processor

    }

    void onSendConfirmed(bool success) override {
        if (success) Serial.println("LoRaWrapper: MyApp: LoRaWAN Send sequence considered completed by the MAC.");
        else Serial.println("LoRaWrapper: MyApp: LoRaWAN Send sequence failed at MAC layer (e.g., no channel available).");
    }

    void onMacCommand(uint8_t cmd, uint8_t* payload, uint8_t len) override {
        Serial.printf("LoRaWrapper: MyApp: MAC Command Received: CMD=0x%02X, Length=%d\n", cmd, len);
    }
};
MyAppLoRaCallbacks myCallbacks; // Instance for LoRaWrapper


void setup() { // This is the MAIN setup function
    Serial.begin(SERIAL_BAUD);
    delay(3000); // Wait for serial
    Serial.println("\n\n===================================");
    Serial.println("LoRa-DMX Controller - Main App Starting Up");
    Serial.println("===================================");

    pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);

    dmx = new DmxController(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_DIR_PIN);
    dmx->begin();
    dmxInitialized = true;

    dmxMutex = xSemaphoreCreateMutex();
    if (dmxMutex == NULL) { Serial.println("Failed to create DMX mutex!"); return; }

    // Option 1: Initialize original LoRaManager (if still needed)
    // lora = new LoRaManager();
    // initializeLoRaWAN_Manager(); 

    // Option 2: Initialize LoRaWrapper (Preferred for new development)
    Serial.println("LoRaWrapper: Initializing MCU and LoRaWAN hardware...");
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE); // CRITICAL for Heltec boards

    hexStringToByteArray_wrapper(DEVEUI, actual_dev_eui, 8);
    hexStringToByteArray_wrapper(APPEUI, actual_app_eui, 8);
    hexStringToByteArray_wrapper(APPKEY, actual_app_key, 16);
    // if NWKKEY is separate for LoRaWAN 1.1.x: hexStringToByteArray_wrapper(NWKKEY, actual_nwk_key, 16);
    
    Serial.println("LoRaWrapper: Creating LoRaWAN device instance...");
    loraDevice = new HeltecLoRaWan();

    Serial.println("LoRaWrapper: Configuring parameters...");
    loraDevice->setDevEui(actual_dev_eui);
    loraDevice->setAppEui(actual_app_eui);
    loraDevice->setAppKey(actual_app_key);
    // if NwkKey: loraDevice->setNwkKey(actual_nwk_key);
    
    loraDevice->setActivationType(true); // OTAA
    loraDevice->setAdr(true);            // ADR
    
    LoRaMacRegion_t region = LORAMAC_REGION_US915; // Customize
    DeviceClass_t devClass = CLASS_C;            // Customize

    Serial.printf("LoRaWrapper: Initializing for Region: %d, Class: %s\n", region, (devClass == CLASS_A ? "A" : "C"));
    if (loraDevice->init(devClass, region, &myCallbacks)) {
        Serial.println("LoRaWAN device interface initialized.");
        Serial.println("Attempting to join the LoRaWAN network (OTAA)...");
        loraDevice->join();
    } else {
        Serial.println("FATAL: LoRaWAN device interface initialization failed. Halting.");
        while (1) { delay(1000); }
    }


    xTaskCreatePinnedToCore(dmxTask, "DMX Task", 4096, NULL, 1, &dmxTaskHandle, 0);
    
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    Serial.println("Main setup complete.");
}

unsigned long lastSendMillis_wrapper = 0; // For LoRaWrapper example send
const unsigned long sendIntervalMillis_wrapper = 60000; // For LoRaWrapper
uint8_t uplinkCounter_wrapper = 0; // For LoRaWrapper

void loop() { // This is the MAIN loop function
  esp_task_wdt_reset();

  // Process LoRaWrapper
  if (loraDevice) {
      loraDevice->process(); 
  }

  // Process original LoRaManager (if used)
  // if (loraInitialized && lora != NULL) {
  //   lora->handleEvents();
  //   if (!isConnected && millis() - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
  //       lastConnectionAttempt = millis(); Serial.println("LoRaManager: Reconnecting...");
  //       if (lora->joinNetwork()) { isConnected = true; Serial.println("LoRaManager: Reconnected!");
  //           if (lora->setDeviceClass(DEVICE_CLASS_C)) Serial.println("LoRaManager: Class C re-set.");
  //           else Serial.println("LoRaManager: Failed to re-set Class C.");
  //       }
  //   }
  //   if (isConnected && lora->getDeviceClass() != DEVICE_CLASS_C) { /* try to set class C */ }
  //   processMessageQueue(); // For LoRaManager
  // }

  // Heartbeat via LoRaManager (if used)
  // if (millis() - lastHeartbeat >= 60000) {
  //   lastHeartbeat = millis();
  //   if (loraInitialized && lora != NULL) {
  //       char currentDevClass = lora->getDeviceClass();
  //       String message = "{\"hb\":1,\"class\":\"" + String(currentDevClass) + "\"}\"";
  //       queueMessage(message, 1, true, 200); // For LoRaManager
  //   }
  // }

  // Example: Periodically send an uplink message via LoRaWrapper if joined
    if (loraDevice && loraDevice->isJoined()) {
        if (millis() - lastSendMillis_wrapper >= sendIntervalMillis_wrapper) {
            uplinkCounter_wrapper++;
            char payload_wrapper[32];
            snprintf(payload_wrapper, sizeof(payload_wrapper), "Hello DMX (Wrapper) #%d", uplinkCounter_wrapper);
            
            Serial.printf("LoRaWrapper: Sending uplink: %s\n", payload_wrapper);
            bool sendInitiated = loraDevice->send((uint8_t*)payload_wrapper, strlen(payload_wrapper), 1, false);
            
            if (sendInitiated) Serial.println("LoRaWrapper: Uplink send request initiated.");
            else Serial.println("Failed to initiate uplink send (Example) (e.g., busy, not joined).");
            lastSendMillis_wrapper = millis();
        }
    }


  if (runningRainbowDemo && dmxInitialized && dmx != NULL) {
    if (millis() - lastRainbowStep >= rainbowStepDelay) {
      lastRainbowStep = millis();
      if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        dmx->updateRainbowStep(rainbowStepCounter++, rainbowStaggered);
        xSemaphoreGive(dmxMutex);
      }
    }
  }
  
  if (patternHandler.isActive()) patternHandler.update();
  
  delay(10); // Cooperative delay
}

