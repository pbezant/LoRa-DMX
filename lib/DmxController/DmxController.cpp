#include "DmxController.h"

DmxController::DmxController() {
    dmxSerial = nullptr;
    serialPort = 2;
    txPin = -1;
    rxPin = -1;
    directionPin = -1;
    initialized = false;
    dmxMutex = nullptr;
    dmxTaskHandle = nullptr;
    shouldSendDMX = false;
    
    // Initialize DMX data buffer with zeros
    memset(dmxData, 0, sizeof(dmxData));
    dmxData[0] = 0; // DMX start code
}

DmxController::~DmxController() {
    stop();
}

bool DmxController::begin(int tx, int rx, int dir, int serialPortNum) {
    Serial.printf("DMX: Initializing with TX:%d, RX:%d, DIR:%d, Serial:%d\n", tx, rx, dir, serialPortNum);
    
    if (initialized) {
        Serial.println("DMX: Already initialized");
        return true;
    }
    
    txPin = tx;
    rxPin = rx;
    directionPin = dir;
    serialPort = serialPortNum;
    
    // Create mutex for thread safety
    dmxMutex = xSemaphoreCreateMutex();
    if (dmxMutex == nullptr) {
        Serial.println("DMX: Failed to create mutex");
        return false;
    }
    
    // Initialize direction pin for RS485 control
    pinMode(directionPin, OUTPUT);
    digitalWrite(directionPin, HIGH); // Set to transmit mode
    
    // Get the appropriate HardwareSerial instance
    switch (serialPort) {
        case 0:
            dmxSerial = &Serial;
            break;
        case 1:
            dmxSerial = &Serial1;
            break;
        case 2:
            dmxSerial = &Serial2;
            break;
        default:
            Serial.println("DMX: Invalid serial port number");
            vSemaphoreDelete(dmxMutex);
            dmxMutex = nullptr;
            return false;
    }
    
    // Initialize the serial port with DMX parameters
    // DMX uses 250,000 baud, 8 data bits, no parity, 2 stop bits
    dmxSerial->begin(250000, SERIAL_8N2, rxPin, txPin);
    
    if (!dmxSerial) {
        Serial.println("DMX: Failed to initialize serial port");
        vSemaphoreDelete(dmxMutex);
        dmxMutex = nullptr;
        return false;
    }
    
    // Create DMX transmission task
    BaseType_t taskResult = xTaskCreatePinnedToCore(
        dmxTask,
        "DMX_Task",
        4096,  // Stack size
        this,  // Task parameter
        2,     // Priority
        &dmxTaskHandle,
        0      // Core 0
    );
    
    if (taskResult != pdPASS) {
        Serial.println("DMX: Failed to create DMX task");
        dmxSerial->end();
        vSemaphoreDelete(dmxMutex);
        dmxMutex = nullptr;
        return false;
    }
    
    initialized = true;
    Serial.println("DMX: Initialization successful");
    
    return true;
}

void DmxController::sendBreak() {
    // Send DMX break by switching to a lower baud rate and sending null
    dmxSerial->end();
    dmxSerial->begin(90000, SERIAL_8N1, rxPin, txPin);  // Lower baud rate for break timing
    dmxSerial->write(0x00);
    dmxSerial->flush();
    delayMicroseconds(DMX_BREAK_TIME_US);
    
    // Switch back to normal DMX baud rate
    dmxSerial->end();
    dmxSerial->begin(250000, SERIAL_8N2, rxPin, txPin);
}

void DmxController::sendMarkAfterBreak() {
    delayMicroseconds(DMX_MAB_TIME_US);
}

void DmxController::dmxTask(void* parameter) {
    DmxController* dmx = static_cast<DmxController*>(parameter);
    
    while (dmx->initialized) {
        // Wait for signal to send DMX data
        if (dmx->shouldSendDMX) {
            dmx->shouldSendDMX = false;
            
            // Set direction pin to transmit
            digitalWrite(dmx->directionPin, HIGH);
            
            // Send DMX break
            dmx->sendBreak();
            
            // Send mark after break
            dmx->sendMarkAfterBreak();
            
            // Send DMX data packet
            if (xSemaphoreTake(dmx->dmxMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                dmx->dmxSerial->write(dmx->dmxData, DMX_PACKET_SIZE);
                dmx->dmxSerial->flush();
                xSemaphoreGive(dmx->dmxMutex);
            }
        }
        
        // Small delay to prevent task from hogging CPU
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    vTaskDelete(nullptr);
}

void DmxController::setChannel(uint16_t channel, uint8_t value) {
    if (!initialized || channel < 1 || channel > MAX_DMX_CHANNELS) {
        return;
    }
    
    if (xSemaphoreTake(dmxMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        dmxData[channel] = value; // Channel 1 maps to index 1 (index 0 is start code)
        xSemaphoreGive(dmxMutex);
    }
}

uint8_t DmxController::getChannel(uint16_t channel) {
    if (!initialized || channel < 1 || channel > MAX_DMX_CHANNELS) {
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
    if (!initialized || startChannel < 1 || startChannel > MAX_DMX_CHANNELS) {
        return false;
    }
    
    bool success = true;
    uint16_t currentChannel = startChannel;
    
    if (xSemaphoreTake(dmxMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        
        // Handle different fixture types
        if (fixture.containsKey("red") && fixture.containsKey("green") && fixture.containsKey("blue")) {
            // RGB fixture
            if (currentChannel + 2 <= MAX_DMX_CHANNELS) {
                dmxData[currentChannel] = fixture["red"].as<uint8_t>();
                dmxData[currentChannel + 1] = fixture["green"].as<uint8_t>();
                dmxData[currentChannel + 2] = fixture["blue"].as<uint8_t>();
                currentChannel += 3;
                
                // Check for white channel (RGBW)
                if (fixture.containsKey("white") && currentChannel <= MAX_DMX_CHANNELS) {
                    dmxData[currentChannel] = fixture["white"].as<uint8_t>();
                    currentChannel++;
                }
            } else {
                success = false;
            }
        } else if (fixture.containsKey("intensity") || fixture.containsKey("dimmer")) {
            // Single channel dimmer
            if (currentChannel <= MAX_DMX_CHANNELS) {
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
            for (size_t i = 0; i < channels.size() && currentChannel <= MAX_DMX_CHANNELS; i++) {
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
    if (!initialized) {
        return;
    }
    
    // Signal the DMX task to send data
    shouldSendDMX = true;
}

bool DmxController::isRunning() {
    return initialized;
}

void DmxController::stop() {
    if (initialized) {
        initialized = false;
        
        // Wait for task to finish
        if (dmxTaskHandle != nullptr) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Give task time to clean up
            dmxTaskHandle = nullptr;
        }
        
        if (dmxSerial != nullptr) {
            dmxSerial->end();
            dmxSerial = nullptr;
        }
    }
    
    if (dmxMutex != nullptr) {
        vSemaphoreDelete(dmxMutex);
        dmxMutex = nullptr;
    }
    
    Serial.println("DMX: Stopped");
}

void DmxController::setFixtureColor(int fixtureId, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    // Simple RGBW fixture mapping - assume 4 channels per fixture starting at channel 1
    int startChannel = (fixtureId * 4) + 1;
    
    if (startChannel <= MAX_DMX_CHANNELS - 3) { // Leave room for 4 channels
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