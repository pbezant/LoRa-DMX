#include "DmxController.h"

DmxController::DmxController() {
    dmx = nullptr;
    txPin = -1;
    rxPin = -1;
    directionPin = -1;
    initialized = false;
    dmxTaskHandle = nullptr;
    dmxMutex = nullptr;
    
    // Initialize DMX data buffer with zeros
    memset(dmxData, 0, sizeof(dmxData));
    dmxData[0] = 0; // DMX start code
}

DmxController::~DmxController() {
    stop();
}

bool DmxController::begin(int tx, int rx, int dir) {
    Serial.printf("DMX: Initializing with TX:%d, RX:%d, DIR:%d\n", tx, rx, dir);
    
    if (initialized) {
        Serial.println("DMX: Already initialized");
        return true;
    }
    
    txPin = tx;
    rxPin = rx;
    directionPin = dir;
    
    // Create mutex for thread safety
    dmxMutex = xSemaphoreCreateMutex();
    if (dmxMutex == nullptr) {
        Serial.println("DMX: Failed to create mutex");
        return false;
    }
    
    // Create DMX object - using UART port 2 as recommended by LXESP32DMX
    dmx = new LX32DMX(2);
    if (dmx == nullptr) {
        Serial.println("DMX: Failed to create DMX object");
        vSemaphoreDelete(dmxMutex);
        dmxMutex = nullptr;
        return false;
    }
    
    // Set direction pin for output
    dmx->setDirectionPin(directionPin);
    
    // Start DMX output with UART pins and task priority
    const int DMX_OUTPUT_TASK_PRIORITY = 2;
    const int DMX_CORE = 0; // Run on core 0
    
    dmx->startOutput(txPin, DMX_OUTPUT_TASK_PRIORITY, DMX_CORE);
    
    initialized = true;
    Serial.println("DMX: Initialization successful");
    
    return true;
}

void DmxController::setChannel(uint16_t channel, uint8_t value) {
    if (!initialized || channel < 1 || channel > 512) {
        return;
    }
    
    if (xSemaphoreTake(dmxMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        dmxData[channel] = value; // Channel 1 maps to index 1 (index 0 is start code)
        xSemaphoreGive(dmxMutex);
    }
}

uint8_t DmxController::getChannel(uint16_t channel) {
    if (!initialized || channel < 1 || channel > 512) {
        return 0;
    }
    
    uint8_t value = 0;
    if (xSemaphoreTake(dmxMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        value = dmxData[channel];
        xSemaphoreGive(dmxMutex);
    }
    
    return value;
}

bool DmxController::setFixtureChannels(uint16_t startChannel, const JsonObject& fixture) {
    if (!initialized || startChannel < 1 || startChannel > 512) {
        return false;
    }
    
    bool success = true;
    uint16_t currentChannel = startChannel;
    
    if (xSemaphoreTake(dmxMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        
        // Handle different fixture types
        if (fixture.containsKey("red") && fixture.containsKey("green") && fixture.containsKey("blue")) {
            // RGB fixture
            if (currentChannel + 2 <= 512) {
                dmxData[currentChannel] = fixture["red"].as<uint8_t>();
                dmxData[currentChannel + 1] = fixture["green"].as<uint8_t>();
                dmxData[currentChannel + 2] = fixture["blue"].as<uint8_t>();
                currentChannel += 3;
                
                // Check for white channel (RGBW)
                if (fixture.containsKey("white") && currentChannel <= 512) {
                    dmxData[currentChannel] = fixture["white"].as<uint8_t>();
                    currentChannel++;
                }
            } else {
                success = false;
            }
        } else if (fixture.containsKey("intensity") || fixture.containsKey("dimmer")) {
            // Single channel dimmer
            if (currentChannel <= 512) {
                uint8_t value = fixture.containsKey("intensity") ? 
                                fixture["intensity"].as<uint8_t>() : 
                                fixture["dimmer"].as<uint8_t>();
                dmxData[currentChannel] = value;
            } else {
                success = false;
            }
        } else if (fixture.containsKey("channels")) {
            // Array of channel values
            JsonArray channels = fixture["channels"];
            for (size_t i = 0; i < channels.size() && currentChannel <= 512; i++) {
                dmxData[currentChannel] = channels[i].as<uint8_t>();
                currentChannel++;
            }
        }
        
        xSemaphoreGive(dmxMutex);
    } else {
        success = false;
    }
    
    return success;
}

void DmxController::update() {
    if (!initialized || dmx == nullptr) {
        return;
    }
    
    if (xSemaphoreTake(dmxMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Copy DMX data to LXESP32DMX buffer
        for (int i = 0; i < 513; i++) {
            dmx->setSlot(i, dmxData[i]);
        }
        xSemaphoreGive(dmxMutex);
    }
}

bool DmxController::isRunning() {
    return initialized && dmx != nullptr;
}

void DmxController::stop() {
    if (dmx != nullptr) {
        delete dmx;
        dmx = nullptr;
    }
    
    if (dmxMutex != nullptr) {
        vSemaphoreDelete(dmxMutex);
        dmxMutex = nullptr;
    }
    
    initialized = false;
    Serial.println("DMX: Stopped");
}

void DmxController::dmxTask(void* parameter) {
    // This was for the old implementation - LXESP32DMX handles the task internally
    vTaskDelete(nullptr);
}

void DmxController::setFixtureColor(int fixtureId, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    // Simple RGBW fixture mapping - assume 4 channels per fixture starting at channel 1
    int startChannel = (fixtureId * 4) + 1;
    
    if (startChannel <= 509) { // Leave room for 4 channels
        setChannel(startChannel, r);     // Red
        setChannel(startChannel + 1, g); // Green
        setChannel(startChannel + 2, b); // Blue
        setChannel(startChannel + 3, w); // White
    }
}

void DmxController::blinkLED(int pin, int count, int delayMs) {
    for (int i = 0; i < count; i++) {
        digitalWrite(pin, HIGH);
        delay(delayMs);
        digitalWrite(pin, LOW);
        delay(delayMs);
    }
} 