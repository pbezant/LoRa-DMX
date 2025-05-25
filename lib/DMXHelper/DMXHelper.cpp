#include "DMXHelper.h"
#include <Arduino.h>
#include <esp_dmx.h>
#include <ArduinoJson.h>

// --- DMX Configuration ---
const int DMX_TX_PIN = 17;     // Adjust these pins based on your hardware
const int DMX_RX_PIN = 16;     // May need to change based on your connections
const int DMX_ENABLE_PIN = 21; // May need to change based on your connections
const dmx_port_t DMX_PORT = 1; // Using DMX port 1

// --- DMX Buffer ---
static uint8_t dmx_buffer[DMX_PACKET_SIZE]; // DMX data buffer

// --- Pattern enum (must match main.cpp) ---
enum PatternType { NONE, COLORFADE, RAINBOW, STROBE, CHASE, ALTERNATE };
static PatternType currentPattern = NONE;
static unsigned long patternStart = 0;
static unsigned long patternLastUpdate = 0;
static int patternCycles = 0;
static int patternMaxCycles = 0;
static int patternSpeed = 50;

void dmx_helper_init() {
    // Install DMX driver with default configuration
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmx_personality_t personalities[] = {};
    int personality_count = 0;
    dmx_driver_install(DMX_PORT, &config, personalities, personality_count);
    
    // Set the DMX hardware pins
    dmx_set_pin(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_ENABLE_PIN);
    
    // Clear all channels
    memset(dmx_buffer, 0, DMX_PACKET_SIZE);
    dmx_buffer[0] = 0; // Start code for DMX is 0
    
    // Send initial empty packet
    dmx_write(DMX_PORT, dmx_buffer, DMX_PACKET_SIZE);
    dmx_send_num(DMX_PORT, DMX_PACKET_SIZE);
    dmx_wait_sent(DMX_PORT, DMX_TIMEOUT_TICK);
    
    Serial.println("[DMXHelper] DMX initialized");
    currentPattern = NONE;
}

void dmx_helper_set_fixture_channels(int address, const uint8_t* channels, size_t num_channels) {
    // Make sure we don't write beyond the buffer
    if (address <= 0 || address + num_channels > DMX_PACKET_SIZE) {
        Serial.printf("[DMXHelper] Invalid address or channels: %d, %d\n", address, num_channels);
        return;
    }
    
    // Copy channel values to DMX buffer (offset by 1 for DMX address and start code)
    for (size_t i = 0; i < num_channels; i++) {
        dmx_buffer[address + i] = channels[i];
    }
}

void dmx_helper_update() {
    // Write and send the DMX data
    dmx_write(DMX_PORT, dmx_buffer, DMX_PACKET_SIZE);
    dmx_send_num(DMX_PORT, DMX_PACKET_SIZE);
    dmx_wait_sent(DMX_PORT, DMX_TIMEOUT_TICK);
}

void dmx_helper_clear() {
    // Clear all channels except start code
    memset(dmx_buffer + 1, 0, DMX_PACKET_SIZE - 1);
    dmx_helper_update();
}

void dmx_helper_start_pattern(int pattern_type, int speed, int cycles) {
    currentPattern = (PatternType)pattern_type;
    patternStart = millis();
    patternLastUpdate = 0;
    patternCycles = 0;
    patternMaxCycles = cycles;
    patternSpeed = speed;
    Serial.printf("[DMXHelper] Start pattern %d speed=%d cycles=%d\n", pattern_type, speed, cycles);
}

void dmx_helper_stop_pattern() {
    currentPattern = NONE;
    patternCycles = 0;
    patternMaxCycles = 0;
    patternSpeed = 50;
    Serial.println("[DMXHelper] Stop pattern");
}

void dmx_helper_run_pattern() {
    if (currentPattern == NONE) return;
    
    unsigned long now = millis();
    if (now - patternLastUpdate < (unsigned long)patternSpeed) return;
    patternLastUpdate = now;
    
    uint8_t* buf = dmx_buffer + 1; // Skip start code
    
    switch (currentPattern) {
        case COLORFADE: {
            static uint8_t val = 0;
            static int dir = 1;
            for (int i = 0; i < 512; i += 4) {
                buf[i] = val;
                buf[i+1] = 255-val;
                buf[i+2] = 0;
                buf[i+3] = 0;
            }
            val += dir;
            if (val == 0 || val == 255) dir = -dir;
            break;
        }
        case RAINBOW: {
            static uint8_t hue = 0;
            for (int i = 0; i < 512; i += 4) {
                uint8_t r = abs((hue + 0) % 256 - 128);
                uint8_t g = abs((hue + 85) % 256 - 128);
                uint8_t b = abs((hue + 170) % 256 - 128);
                buf[i] = r;
                buf[i+1] = g;
                buf[i+2] = b;
                buf[i+3] = 0;
            }
            hue++;
            break;
        }
        case STROBE: {
            static bool on = false;
            memset(buf, on ? 255 : 0, 512);
            on = !on;
            break;
        }
        case CHASE: {
            static int pos = 0;
            memset(buf, 0, 512);
            for (int i = 0; i < 512; i += 4) {
                if ((i/4) == pos) {
                    buf[i] = 255;
                    buf[i+1] = 255;
                    buf[i+2] = 255;
                    buf[i+3] = 0;
                }
            }
            pos = (pos + 1) % (512/4);
            break;
        }
        case ALTERNATE: {
            static bool even = false;
            for (int i = 0; i < 512; i += 4) {
                bool isEven = ((i/4) % 2 == 0);
                uint8_t val = (isEven == even) ? 255 : 0;
                buf[i] = val;
                buf[i+1] = val;
                buf[i+2] = val;
                buf[i+3] = 0;
            }
            even = !even;
            break;
        }
        default: break;
    }
    
    dmx_helper_update();
    
    if (patternMaxCycles > 0) {
        patternCycles++;
        if (patternCycles >= patternMaxCycles) dmx_helper_stop_pattern();
    }
}

// Process a JSON command for DMX control
void dmx_helper_process_json_command(const char* json_payload, size_t length) {
    // Add null termination if not present
    char* buffer = NULL;
    if (json_payload[length-1] != 0) {
        buffer = (char*)malloc(length + 1);
        if (!buffer) {
            Serial.println("[DMXHelper] Memory allocation failed for JSON processing");
            return;
        }
        memcpy(buffer, json_payload, length);
        buffer[length] = 0; // Add null terminator
        json_payload = buffer;
    }
    
    Serial.print("[DMXHelper] Processing JSON command: ");
    Serial.println(json_payload);
    
    // Parse JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json_payload);
    
    if (error) {
        Serial.print("[DMXHelper] JSON parse error: ");
        Serial.println(error.c_str());
        if (buffer) free(buffer);
        return;
    }
    
    // Process commands
    if (doc.containsKey("cmd")) {
        const char* cmd = doc["cmd"];
        
        if (strcmp(cmd, "set") == 0 && doc.containsKey("addr") && doc.containsKey("values")) {
            int addr = doc["addr"];
            JsonArray values = doc["values"].as<JsonArray>();
            
            if (values.size() > 0) {
                uint8_t channelValues[32]; // Max 32 channels per command
                size_t numChannels = min((size_t)values.size(), (size_t)32);
                
                for (size_t i = 0; i < numChannels; i++) {
                    channelValues[i] = values[i];
                }
                
                dmx_helper_set_fixture_channels(addr, channelValues, numChannels);
                dmx_helper_update();
                Serial.printf("[DMXHelper] Set address %d with %d values\n", addr, numChannels);
            }
        }
        else if (strcmp(cmd, "clear") == 0) {
            dmx_helper_clear();
            Serial.println("[DMXHelper] Cleared all channels");
        }
        else if (strcmp(cmd, "colorfade") == 0) {
            int speed = doc.containsKey("speed") ? doc["speed"] : 50;
            int cycles = doc.containsKey("cycles") ? doc["cycles"] : 0;
            dmx_helper_start_pattern(COLORFADE, speed, cycles);
        }
        else if (strcmp(cmd, "rainbow") == 0) {
            int speed = doc.containsKey("speed") ? doc["speed"] : 50;
            int cycles = doc.containsKey("cycles") ? doc["cycles"] : 0;
            dmx_helper_start_pattern(RAINBOW, speed, cycles);
        }
        else if (strcmp(cmd, "strobe") == 0) {
            int speed = doc.containsKey("speed") ? doc["speed"] : 50;
            int cycles = doc.containsKey("cycles") ? doc["cycles"] : 0;
            dmx_helper_start_pattern(STROBE, speed, cycles);
        }
        else if (strcmp(cmd, "chase") == 0) {
            int speed = doc.containsKey("speed") ? doc["speed"] : 50;
            int cycles = doc.containsKey("cycles") ? doc["cycles"] : 0;
            dmx_helper_start_pattern(CHASE, speed, cycles);
        }
        else if (strcmp(cmd, "alternate") == 0) {
            int speed = doc.containsKey("speed") ? doc["speed"] : 50;
            int cycles = doc.containsKey("cycles") ? doc["cycles"] : 0;
            dmx_helper_start_pattern(ALTERNATE, speed, cycles);
        }
        else if (strcmp(cmd, "stop") == 0) {
            dmx_helper_stop_pattern();
        }
        else {
            Serial.printf("[DMXHelper] Unknown command: %s\n", cmd);
        }
    }
    
    if (buffer) free(buffer);
}

void dmx_helper_loop() {
    dmx_helper_run_pattern();
} 