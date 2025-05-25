#include "DMXHelper.h"
#include "../DmxController/DmxController.h"
#include <Arduino.h>

// --- Internal state ---
static DmxController dmx;

// Pattern enum (must match main.cpp)
enum PatternType { NONE, COLORFADE, RAINBOW, STROBE, CHASE, ALTERNATE };
static PatternType currentPattern = NONE;
static unsigned long patternStart = 0;
static unsigned long patternLastUpdate = 0;
static int patternCycles = 0;
static int patternMaxCycles = 0;
static int patternSpeed = 50;

void dmx_helper_init() {
    dmx.begin();
    dmx.clearAllChannels();
    dmx.sendData();
    Serial.println("[DMXHelper] DMX initialized");
    currentPattern = NONE;
}

void dmx_helper_set_fixture_channels(int address, const uint8_t* channels, size_t num_channels) {
    for (size_t i = 0; i < num_channels; i++) {
        dmx.getDmxData()[address - 1 + i] = channels[i];
    }
}

void dmx_helper_update() {
    dmx.sendData();
}

void dmx_helper_clear() {
    dmx.clearAllChannels();
    dmx.sendData();
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
    uint8_t* buf = dmx.getDmxData();
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
    dmx.sendData();
    if (patternMaxCycles > 0) {
        patternCycles++;
        if (patternCycles >= patternMaxCycles) dmx_helper_stop_pattern();
    }
} 