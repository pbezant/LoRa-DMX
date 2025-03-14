/**
 * DmxController.cpp - Implementation of the DmxController library
 * 
 * This is an updated implementation to work with esp_dmx 4.1.0
 */

#include "DmxController.h"

// Constructor
DmxController::DmxController(uint8_t dmxPort, uint8_t txPin, uint8_t rxPin, uint8_t dirPin) {
    _dmxPort = dmxPort;
    _txPin = txPin;
    _rxPin = rxPin;
    _dirPin = dirPin;
    
    // Initialize the DMX data buffer with all zeros
    memset(_dmxData, 0, DMX_PACKET_SIZE);
    
    // DMX start code must be 0
    _dmxData[0] = 0;
}

// Initialize the DMX controller
void DmxController::begin() {
    // Configure DMX with default config
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    
    // Define DMX personality for simple RGB control
    dmx_personality_t personality;
    personality.footprint = 3;
    strcpy(personality.description, "RGB");
    
    // Install the DMX driver with the correct parameters for esp_dmx 4.1.0
    if (!dmx_driver_install((dmx_port_t)_dmxPort, &config, &personality, 1)) {
        Serial.println("Failed to install DMX driver");
        return;
    }
    
    // Set DMX pins
    dmx_set_pin((dmx_port_t)_dmxPort, _txPin, _rxPin, _dirPin);
    
    // Manually set direction pin mode (some fixtures need this)
    pinMode(_dirPin, OUTPUT);
    digitalWrite(_dirPin, HIGH);  // HIGH = transmit mode
    
    Serial.println("DMX driver installed successfully");
    Serial.print("DMX using pins - TX: ");
    Serial.print(_txPin);
    Serial.print(", RX: ");
    Serial.print(_rxPin);
    Serial.print(", DIR: ");
    Serial.println(_dirPin);
}

// Set a fixture's RGB color
void DmxController::setFixtureColor(int address, Color color) {
    // Assuming RGB fixtures with 3 consecutive channels:
    // Channel 1 (address): Red
    // Channel 2 (address+1): Green
    // Channel 3 (address+2): Blue
    
    // Check if address is within valid DMX range
    if (address > 0 && address+2 < DMX_PACKET_SIZE) {
        // Direct buffer access for better debugging
        _dmxData[address] = color.r;      // Red
        _dmxData[address+1] = color.g;    // Green
        _dmxData[address+2] = color.b;    // Blue
        
        // Print the actual values being set for debugging
        Serial.print("Setting DMX channels - [");
        Serial.print(address);
        Serial.print("]=");
        Serial.print(color.r);
        Serial.print(", [");
        Serial.print(address+1);
        Serial.print("]=");
        Serial.print(color.g);
        Serial.print(", [");
        Serial.print(address+2);
        Serial.print("]=");
        Serial.println(color.b);
    } else {
        Serial.print("Invalid DMX address range: ");
        Serial.print(address);
        Serial.print(" to ");
        Serial.println(address+2);
    }
}

// Send the current DMX data to the fixtures
void DmxController::sendData() {
    // Ensure DMX start code is 0
    _dmxData[0] = 0;
    
    // For debugging - print the first few bytes
    Serial.print("Sending DMX data: [0]=");
    Serial.print(_dmxData[0]);
    for (int i = 1; i <= 5; i++) {
        Serial.print(", [");
        Serial.print(i);
        Serial.print("]=");
        Serial.print(_dmxData[i]);
    }
    Serial.println();
    
    // Write the data to the DMX buffer
    dmx_write((dmx_port_t)_dmxPort, _dmxData, DMX_PACKET_SIZE);
    
    // Send the DMX packet
    dmx_send((dmx_port_t)_dmxPort);
    
    // Wait for the data to be sent
    dmx_wait_sent((dmx_port_t)_dmxPort, DMX_TIMEOUT_TICK);
}

// Get a debug-friendly string of current DMX values for a fixture
String DmxController::getDmxValueString(const String& fixtureLabel, int address) {
    if (address <= 0 || address+2 >= DMX_PACKET_SIZE) {
        return "Invalid address";
    }
    
    return fixtureLabel + " (Addr " + String(address) + "-" + String(address+2) + "): R=" + 
           String(_dmxData[address]) + ", G=" + String(_dmxData[address+1]) + 
           ", B=" + String(_dmxData[address+2]);
}

// Clear all DMX channels (set to 0)
void DmxController::clearAllChannels() {
    // Clear all DMX data
    memset(_dmxData, 0, DMX_PACKET_SIZE);
    
    // DMX start code must be 0
    _dmxData[0] = 0;
    
    Serial.println("All DMX channels cleared");
}

// Log DMX values for debugging
void DmxController::logDmxValues(const String& fixture1Label, int fixture1Addr, 
                                 const String& fixture2Label, int fixture2Addr) {
    logMessage("Current DMX Values:");
    logMessage(getDmxValueString(fixture1Label, fixture1Addr));
    
    // If second fixture is provided, log its values too
    if (fixture2Label.length() > 0 && fixture2Addr > 0) {
        logMessage(getDmxValueString(fixture2Label, fixture2Addr));
    }
}

// Convert HSV to RGB color space
void DmxController::HSVtoRGB(float h, float s, float v, float &r, float &g, float &b) {
    // H is in range 0-1 (instead of 0-360)
    int i = int(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    
    switch(i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
}

// Get an array of standard RGB colors
Color* DmxController::getStandardColors(int &numColors) {
    static Color colors[] = {
        {255, 0, 0},    // Red
        {0, 255, 0},    // Green
        {0, 0, 255},    // Blue
        {255, 255, 0},  // Yellow
        {255, 0, 255},  // Purple
        {0, 255, 255},  // Cyan
        {255, 255, 255} // White
    };
    
    numColors = sizeof(colors) / sizeof(Color);
    return colors;
}

// Set fixture color using HSV values
void DmxController::setFixtureColorHSV(int address, float hue, float saturation, float value) {
    // Convert HSV to RGB
    float r, g, b;
    HSVtoRGB(hue, saturation, value, r, g, b);
    
    // Convert to 8-bit color values
    Color color = {
        (uint8_t)(r * 255),
        (uint8_t)(g * 255),
        (uint8_t)(b * 255)
    };
    
    // Set the fixture color
    setFixtureColor(address, color);
}

// Set multiple fixtures to the same color
void DmxController::setMultipleFixtureColors(const int addresses[], int numFixtures, Color color) {
    for (int i = 0; i < numFixtures; i++) {
        setFixtureColor(addresses[i], color);
    }
}

// Cycle colors for two fixtures
void DmxController::cycleColors(int fixture1Addr, int fixture2Addr, bool useStandardColors, 
                               int &colorIndex, int offset) {
    if (useStandardColors) {
        // Use standard color palette
        int numColors;
        Color* colors = getStandardColors(numColors);
        
        // Update with next color
        setFixtureColor(fixture1Addr, colors[colorIndex]);
        setFixtureColor(fixture2Addr, colors[(colorIndex + offset) % numColors]);
        
        // Increment color index
        colorIndex = (colorIndex + 1) % numColors;
    } else {
        // Use smooth HSV color transitions
        float hue1 = (float)colorIndex / 100.0;
        float hue2 = fmod(hue1 + 0.5, 1.0); // Offset by half the color wheel
        
        // Set colors using HSV values
        setFixtureColorHSV(fixture1Addr, hue1, 1.0, 1.0);
        setFixtureColorHSV(fixture2Addr, hue2, 1.0, 1.0);
        
        // Increment hue for next cycle (0-99 range)
        colorIndex = (colorIndex + 1) % 100;
    }
}

// Helper function to log a message
void DmxController::logMessage(const String& message) {
    // By default, print to Serial
    // This can be overridden by the user or extended as needed
    Serial.println(message);
} 