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
 * - LoRaWrapper: Custom LoRaWAN communication wrapper for Heltec
 * - ArduinoJson: JSON parsing
 * - DmxController: DMX output control
 */

#include <Arduino.h>
#include <vector>
#include "LoRaWan_APP.h"
#include "driver/board.h"
#include <ArduinoJson.h>
#include "DmxController.h"
#include <esp_task_wdt.h>  // Watchdog
#include "secrets.h"  // LoRaWAN credentials

// Debug output
#define SERIAL_BAUD 115200

// Diagnostic LED pin
#define LED_PIN 35  // Built-in LED on Heltec LoRa 32 V3

// LoRaWAN parameters
uint8_t devEui[] = { 0x90, 0xcf, 0xf8, 0x68, 0xef, 0x8b, 0xd4, 0xcc };
uint8_t appEui[] = { 0xED, 0x73, 0x32, 0x20, 0xD2, 0xA9, 0xF1, 0x33 };
uint8_t appKey[] = { 0xf7, 0xed, 0xcf, 0xe4, 0x61, 0x7e, 0x66, 0x70, 
                     0x16, 0x65, 0xa1, 0x3a, 0x2b, 0x76, 0xdd, 0x52 };

// Application port and data
uint8_t appPort = 2;
uint8_t appData[LORAWAN_APP_DATA_MAX_SIZE];
uint8_t appDataSize = 0;

// LoRaWAN settings
bool overTheAirActivation = true;
bool loraWanAdr = true;
bool isTxConfirmed = true;
uint32_t appTxDutyCycle = 15000;
DeviceClass_t loraWanClass = CLASS_C;
LoRaMacRegion_t loraWanRegion = LORAMAC_REGION_US915;
uint8_t confirmedNbTrials = 8;

// Channel mask for US915 sub-band 2 (channels 8-15 + 65)
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0002, 0x0000 };

// DMX configuration for Heltec LoRa 32 V3
#define DMX_PORT 1
#define DMX_TX_PIN 19  // TX pin for DMX
#define DMX_RX_PIN 20  // RX pin for DMX
#define DMX_DIR_PIN 5  // DIR pin for DMX (connect to both DE and RE on MAX485)

// DMX configuration
#define MAX_FIXTURES 32
#define MAX_CHANNELS_PER_FIXTURE 16
#define MAX_JSON_SIZE 1024

// Global variables for DMX application
bool dmxInitialized = false;
DmxController* dmx = NULL;
SemaphoreHandle_t dmxMutex = NULL;
TaskHandle_t dmxTaskHandle = NULL;
bool keepDmxDuringRx = true;

// Forward declarations for DMX application
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port);
bool processLightsJson(JsonArray lightsArray);
void processMessageQueue(); 

uint8_t receivedData[MAX_JSON_SIZE];
size_t receivedDataSize = 0;
uint8_t receivedPort = 0;

bool runningRainbowDemo = false;
unsigned long lastRainbowStep = 0;
uint32_t rainbowStepCounter = 0;
int rainbowStepDelay = 30;
bool rainbowStaggered = true;

unsigned long lastHeartbeat = 0;

#define WDT_TIMEOUT 30 // Reverted from 120 back to 30 seconds - sync word issue fixed

// Downlink data handler
void downLinkDataHandle(McpsIndication_t *mcpsIndication) {
    Serial.printf("Received data on port %d\n", mcpsIndication->Port);
    
    if (mcpsIndication->BufferSize > 0) {
        // Create a null-terminated string from the received data
        char jsonString[MAX_JSON_SIZE + 1];
        size_t copySize = min((size_t)mcpsIndication->BufferSize, (size_t)MAX_JSON_SIZE);
        memcpy(jsonString, mcpsIndication->Buffer, copySize);
        jsonString[copySize] = '\0';
        
        // Process the JSON payload
        processJsonPayload(jsonString);
    }
}

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
    if (!isConnected || messageQueue.empty() || !loraDevice) return;
    std::sort(messageQueue.begin(), messageQueue.end(), [](const PendingMessage& a, const PendingMessage& b) { return a.priority < b.priority; });
    const PendingMessage& msg = messageQueue.front();
    if (loraDevice->send((uint8_t*)msg.payload.c_str(), msg.payload.length(), msg.port, msg.confirmed)) messageQueue.erase(messageQueue.begin());
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
            } else { Serial.println("LoRaManager: Failed to process downlink"); DmxController::blinkLED(LED_PIN, 5, 100); }
        } else Serial.println("LoRaManager ERROR: DMX not initialized");
    }
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

// Helper function to convert hex string to byte array
void hexStringToByteArray(const char* hexString, uint8_t* byteArray, int byteLen) {
    for (int i = 0; i < byteLen; i++) {
        char byteChars[3] = {hexString[i*2], hexString[i*2+1], 0};
        byteArray[i] = strtol(byteChars, nullptr, 16);
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(3000); // Wait for serial
    Serial.println("\n\n===================================");
    Serial.println("LoRa-DMX Controller - LoRaWrapper Implementation");
    Serial.println("===================================");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Initialize DMX
    dmx = new DmxController();
    dmx->begin(DMX_TX_PIN, DMX_RX_PIN, DMX_DIR_PIN);
    dmxInitialized = true;

    dmxMutex = xSemaphoreCreateMutex();
    if (dmxMutex == NULL) {
        Serial.println("Failed to create DMX mutex!");
        return;
    }

    // Initialize LoRaWAN
    Serial.println("LoRaWrapper: Initializing LoRaWAN...");

    // Convert credentials from strings to byte arrays
    uint8_t actual_dev_eui[8];
    uint8_t actual_app_eui[8];
    uint8_t actual_app_key[16];
    uint8_t actual_nwk_key[16];

    hexStringToByteArray(DEVEUI, actual_dev_eui, 8);
    hexStringToByteArray(APPEUI, actual_app_eui, 8);
    hexStringToByteArray(APPKEY, actual_app_key, 16);
    hexStringToByteArray(NWKKEY, actual_nwk_key, 16);

    // Initialize Heltec board
    Mcu.begin(30, 0);  // 30 = WIFI_LORA_32_V3, 0 = no external 32K crystal

    // Create and configure LoRaWAN device
    loraDevice = new HeltecLoRaWan();
    loraDevice->setDevEui(actual_dev_eui);
    loraDevice->setAppEui(actual_app_eui);
    loraDevice->setAppKey(actual_app_key);
    loraDevice->setNwkKey(actual_nwk_key);
    loraDevice->setActivationType(true); // true for OTAA
    loraDevice->setAdr(true);

    // Initialize for Class C operation in US915 region
    if (loraDevice->init(CLASS_C, LORAMAC_REGION_US915, &loraCallbacks)) {
        Serial.println("LoRaWrapper: Device initialized successfully");
        loraWanInitialized = true;
        loraDevice->join();
    } else {
        Serial.println("LoRaWrapper: Device initialization failed!");
        while(1) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }

    // Create DMX task
    xTaskCreatePinnedToCore(dmxTask, "DMX Task", 4096, NULL, 1, &dmxTaskHandle, 0);
    
    // Initialize watchdog
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    Serial.println("Main setup complete.");
}

void loop() {
    esp_task_wdt_reset();

    // Process LoRaWAN stack
    if (loraWanInitialized && loraDevice != nullptr) {
        loraDevice->process();

        // Send heartbeat if joined
        if (loraDevice->isJoined() && millis() - lastHeartbeat >= 60000) {
            lastHeartbeat = millis();
            String heartbeat = "{\"heartbeat\":1,\"uptime\":" + String(millis()) + "}";
            Serial.println("LoRaWrapper: Sending heartbeat: " + heartbeat);
            loraDevice->send((uint8_t*)heartbeat.c_str(), heartbeat.length(), 1, false);
        }
    }

    // Process DMX patterns
    if (runningRainbowDemo && dmxInitialized && dmx != NULL) {
        if (millis() - lastRainbowStep >= rainbowStepDelay) {
            lastRainbowStep = millis();
            if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
                dmx->updateRainbowStep(rainbowStepCounter++, rainbowStaggered);
                xSemaphoreGive(dmxMutex);
            }
        }
    }
    
    if (patternHandler.isActive()) {
        patternHandler.update();
    }
    
    delay(10); // Cooperative delay
}

