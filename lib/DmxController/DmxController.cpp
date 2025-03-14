/**
 * DmxController.cpp - Implementation of the DmxController library
 * 
 * This is an updated implementation to work with esp_dmx 4.1.0
 * Enhanced with fixture management functionality
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
    
    // Initialize member variables
    _fixtures = NULL;
    _numFixtures = 0;
    _channelsPerFixture = 0;
    
    // Initialize scanner variables
    _scanCurrentAddr = 1;
    _scanCurrentColor = 0;
}

// Initialize the DMX controller
void DmxController::begin() {
    // Configure DMX with default config
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    
    // Define DMX personality for RGBW control
    dmx_personality_t personality;
    personality.footprint = 4; // 4 channels for RGBW
    strcpy(personality.description, "RGBW");
    
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

// Initialize fixtures array with the given configuration
void DmxController::initializeFixtures(int numFixtures, int channelsPerFixture) {
    // Free previous array if it exists
    if (_fixtures != NULL) {
        delete[] _fixtures;
    }
    
    // Store configuration
    _numFixtures = numFixtures;
    _channelsPerFixture = channelsPerFixture;
    
    // Allocate new array
    _fixtures = new FixtureConfig[numFixtures];
    
    Serial.print("Initialized for ");
    Serial.print(numFixtures);
    Serial.print(" fixtures with ");
    Serial.print(channelsPerFixture);
    Serial.println(" channels per fixture");
}

// Set fixture configuration
void DmxController::setFixtureConfig(int index, const char* name, int startAddr, 
                                    int rChan, int gChan, int bChan, int wChan) {
    if (index >= 0 && index < _numFixtures && _fixtures != NULL) {
        _fixtures[index].name = name;
        _fixtures[index].startAddr = startAddr;
        _fixtures[index].redChannel = rChan;
        _fixtures[index].greenChannel = gChan;
        _fixtures[index].blueChannel = bChan;
        _fixtures[index].whiteChannel = wChan;
        
        Serial.print("Configured fixture ");
        Serial.print(index + 1);
        Serial.print(" (");
        Serial.print(name);
        Serial.print("): Start=");
        Serial.print(startAddr);
        Serial.print(", R=Ch");
        Serial.print(rChan);
        Serial.print(", G=Ch");
        Serial.print(gChan);
        Serial.print(", B=Ch");
        Serial.print(bChan);
        Serial.print(", W=Ch");
        Serial.println(wChan);
    }
}

// Get a fixture's configuration
FixtureConfig* DmxController::getFixture(int index) {
    if (index >= 0 && index < _numFixtures && _fixtures != NULL) {
        return &_fixtures[index];
    }
    return NULL;
}

// Helper function to set a fixture's color with direct RGBW handling
void DmxController::setFixtureColor(int fixtureIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    // Check if the fixture index is valid
    if (fixtureIndex >= 0 && fixtureIndex < _numFixtures && _fixtures != NULL) {
        // Set RGBW values directly to their respective channels
        _dmxData[_fixtures[fixtureIndex].redChannel] = r;
        _dmxData[_fixtures[fixtureIndex].greenChannel] = g;
        _dmxData[_fixtures[fixtureIndex].blueChannel] = b;
        _dmxData[_fixtures[fixtureIndex].whiteChannel] = w;
    }
}

// Helper function to set a fixture's color with direct RGBW handling at any address
void DmxController::setManualFixtureColor(int startAddr, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    // Set RGBW values directly to channels starting at startAddr
    _dmxData[startAddr] = r;     // Red channel
    _dmxData[startAddr + 1] = g; // Green channel
    _dmxData[startAddr + 2] = b; // Blue channel
    _dmxData[startAddr + 3] = w; // White channel
}

// Send the current DMX data to the fixtures
void DmxController::sendData() {
    // Ensure DMX start code is 0
    _dmxData[0] = 0;
    
    // Write the data to the DMX buffer
    dmx_write((dmx_port_t)_dmxPort, _dmxData, DMX_PACKET_SIZE);
    
    // Send the DMX packet
    dmx_send((dmx_port_t)_dmxPort);
    
    // Wait for the data to be sent
    dmx_wait_sent((dmx_port_t)_dmxPort, DMX_TIMEOUT_TICK);
}

// Clear all DMX channels (set to 0)
void DmxController::clearAllChannels() {
    // Clear all DMX data
    memset(_dmxData, 0, DMX_PACKET_SIZE);
    
    // DMX start code must be 0
    _dmxData[0] = 0;
    
    Serial.println("All DMX channels cleared");
}

// Helper function to print fixture values
void DmxController::printFixtureValues() {
    if (_fixtures == NULL || _numFixtures <= 0) {
        Serial.println("No fixtures configured");
        return;
    }
    
    // Calculate total channels to display (up to 32 channels max)
    int channelsToShow = min(_numFixtures * _channelsPerFixture, 32);
    
    // Print the DMX start code and first set of channels for debugging
    Serial.print("DMX Data: [0]=");
    Serial.print(_dmxData[0]);  // Start code (should be 0)
    
    for (int i = 1; i <= channelsToShow; i++) {
        Serial.print(", [");
        Serial.print(i);
        Serial.print("]=");
        Serial.print(_dmxData[i]);
    }
    Serial.println();
    
    // Print RGBW info for each fixture
    for (int i = 0; i < _numFixtures; i++) {
        Serial.print(_fixtures[i].name);
        Serial.print(": R=");
        Serial.print(_dmxData[_fixtures[i].redChannel]);
        Serial.print(", G=");
        Serial.print(_dmxData[_fixtures[i].greenChannel]);
        Serial.print(", B=");
        Serial.print(_dmxData[_fixtures[i].blueChannel]);
        Serial.print(", W=");
        Serial.println(_dmxData[_fixtures[i].whiteChannel]);
    }
}

// Helper function to scan through possible DMX addresses for fixtures
void DmxController::scanForFixtures(int scanStartAddr, int scanEndAddr, int scanStep) {
    // Always keep fixture 1 on with RED to have a reference point
    if (_numFixtures > 0) {
        setFixtureColor(0, 255, 0, 0);
    }
    
    // Clear other DMX channels (except fixture 1)
    for (int i = 1; i <= DMX_PACKET_SIZE - 1; i++) {
        bool shouldSkip = false;
        
        // Skip fixture 1's channels
        if (_numFixtures > 0) {
            if (i >= _fixtures[0].startAddr && 
                i <= _fixtures[0].startAddr + _channelsPerFixture - 1) {
                shouldSkip = true;
            }
        }
        
        if (!shouldSkip) {
            _dmxData[i] = 0;
        }
    }
    
    // Set colors for the test address
    uint8_t r = 0, g = 0, b = 0;
    switch (_scanCurrentColor) {
        case 0: r = 255; break; // RED
        case 1: g = 255; break; // GREEN
        case 2: b = 255; break; // BLUE
    }
    
    // Set color at test address
    setManualFixtureColor(_scanCurrentAddr, r, g, b);
    
    // Print debug info
    Serial.print("FIXTURE SCAN - Testing address ");
    Serial.print(_scanCurrentAddr);
    Serial.print(" with ");
    Serial.print(_scanCurrentColor == 0 ? "RED" : (_scanCurrentColor == 1 ? "GREEN" : "BLUE"));
    Serial.println(" - Watch for fixture response");
    Serial.println("This test will run for 5 seconds...");
    
    // Update for next cycle
    _scanCurrentColor = (_scanCurrentColor + 1) % 3;
    if (_scanCurrentColor == 0) {
        _scanCurrentAddr += scanStep;
        if (_scanCurrentAddr > scanEndAddr) {
            _scanCurrentAddr = scanStartAddr;
        }
    }
}

// Run a channel test at startup to help identify the correct channels
void DmxController::testAllChannels() {
    if (_fixtures == NULL || _numFixtures <= 0) {
        Serial.println("No fixtures configured");
        return;
    }
    
    Serial.println("Starting channel test sequence...");
    
    // Calculate total channels to test (up to 32 max)
    int channelsToTest = min(_numFixtures * _channelsPerFixture, 32);
    
    // Test each channel individually
    for (int channel = 1; channel <= channelsToTest; channel++) {
        // Clear all channels
        clearAllChannels();
        
        // Set this channel to maximum
        _dmxData[channel] = 255;
        
        // Send the data
        sendData();
        
        // Print diagnostic information
        Serial.print("Testing DMX channel ");
        Serial.print(channel);
        Serial.println(" - set to 255");
        
        // Figure out which fixture and which channel this is
        bool foundChannel = false;
        for (int i = 0; i < _numFixtures; i++) {
            if (channel == _fixtures[i].redChannel) {
                Serial.print("  This is Fixture ");
                Serial.print(i+1);
                Serial.println(" Red Channel");
                foundChannel = true;
            } else if (channel == _fixtures[i].greenChannel) {
                Serial.print("  This is Fixture ");
                Serial.print(i+1);
                Serial.println(" Green Channel");
                foundChannel = true;
            } else if (channel == _fixtures[i].blueChannel) {
                Serial.print("  This is Fixture ");
                Serial.print(i+1);
                Serial.println(" Blue Channel");
                foundChannel = true;
            } else if (channel == _fixtures[i].whiteChannel) {
                Serial.print("  This is Fixture ");
                Serial.print(i+1);
                Serial.println(" White Channel");
                foundChannel = true;
            }
        }
        
        if (!foundChannel) {
            Serial.println("  This channel is not mapped to any fixture");
        }
        
        // User feedback prompt
        Serial.println("What effect do you see? (Type a description and press Enter)");
        
        // Wait for user input or timeout after 3 seconds
        unsigned long startTime = millis();
        String response = "";
        while (millis() - startTime < 3000) {
            if (Serial.available() > 0) {
                char c = Serial.read();
                if (c == '\n' || c == '\r') {
                    break;  // End of input
                }
                response += c;
            }
            delay(10);
        }
        
        // If we got a response, print it
        if (response.length() > 0) {
            Serial.print("Channel ");
            Serial.print(channel);
            Serial.print(" response: ");
            Serial.println(response);
        }
        
        // Small delay before next channel
        delay(1000);
    }
    
    // Turn all channels off at the end
    clearAllChannels();
    sendData();
    Serial.println("Channel test complete, all channels cleared");
}

// Test all fixtures with color patterns
void DmxController::testAllFixtures() {
    if (_fixtures == NULL || _numFixtures <= 0) {
        Serial.println("No fixtures configured");
        return;
    }
    
    Serial.println("Testing all fixtures with RGBW configuration...");
    Serial.print("Channel mapping: ");
    for (int i = 0; i < _numFixtures; i++) {
        Serial.print("Fixture ");
        Serial.print(i+1);
        Serial.print(": R=Ch");
        Serial.print(_fixtures[i].redChannel);
        Serial.print(", G=Ch");
        Serial.print(_fixtures[i].greenChannel);
        Serial.print(", B=Ch");
        Serial.print(_fixtures[i].blueChannel);
        Serial.print(", W=Ch");
        Serial.print(_fixtures[i].whiteChannel);
        if (i < _numFixtures - 1) {
            Serial.print(" | ");
        }
    }
    Serial.println();
    Serial.println("Tests slowed down to 8 seconds per step");
    
    // Define test step structure for variable number of fixtures
    struct TestStep {
        const char* description;
        uint8_t* r;
        uint8_t* g;
        uint8_t* b;
        uint8_t* w;
        
        // Constructor to allocate arrays
        TestStep(int numFixtures) {
            r = new uint8_t[numFixtures];
            g = new uint8_t[numFixtures];
            b = new uint8_t[numFixtures];
            w = new uint8_t[numFixtures];
        }
        
        // Destructor to free arrays
        ~TestStep() {
            delete[] r;
            delete[] g;
            delete[] b;
            delete[] w;
        }
    };
    
    // Create array of test steps
    const int numSteps = 11;
    TestStep** testSteps = new TestStep*[numSteps];
    
    // Allocate and initialize each test step
    for (int step = 0; step < numSteps; step++) {
        testSteps[step] = new TestStep(_numFixtures);
    }
    
    // Set descriptions for each test step
    testSteps[0]->description = "All fixtures RED";
    testSteps[1]->description = "All fixtures GREEN";
    testSteps[2]->description = "All fixtures BLUE";
    testSteps[3]->description = "All fixtures WHITE (W only)";
    testSteps[4]->description = "All fixtures WHITE (RGB)";
    testSteps[5]->description = "All fixtures WHITE (RGBW)";
    testSteps[6]->description = "Odd fixtures RED, Even fixtures GREEN";
    testSteps[7]->description = "Odd fixtures GREEN, Even fixtures RED";
    testSteps[8]->description = "Odd fixtures BLUE, Even fixtures RED";
    testSteps[9]->description = "Half brightness test";
    testSteps[10]->description = "All channels OFF";
    
    // Set color values for each test step and fixture
    for (int i = 0; i < _numFixtures; i++) {
        // Step 0: All fixtures RED
        testSteps[0]->r[i] = 255;
        testSteps[0]->g[i] = 0;
        testSteps[0]->b[i] = 0;
        testSteps[0]->w[i] = 0;
        
        // Step 1: All fixtures GREEN
        testSteps[1]->r[i] = 0;
        testSteps[1]->g[i] = 255;
        testSteps[1]->b[i] = 0;
        testSteps[1]->w[i] = 0;
        
        // Step 2: All fixtures BLUE
        testSteps[2]->r[i] = 0;
        testSteps[2]->g[i] = 0;
        testSteps[2]->b[i] = 255;
        testSteps[2]->w[i] = 0;
        
        // Step 3: All fixtures WHITE (W only)
        testSteps[3]->r[i] = 0;
        testSteps[3]->g[i] = 0;
        testSteps[3]->b[i] = 0;
        testSteps[3]->w[i] = 255;
        
        // Step 4: All fixtures WHITE (RGB)
        testSteps[4]->r[i] = 255;
        testSteps[4]->g[i] = 255;
        testSteps[4]->b[i] = 255;
        testSteps[4]->w[i] = 0;
        
        // Step 5: All fixtures WHITE (RGBW)
        testSteps[5]->r[i] = 255;
        testSteps[5]->g[i] = 255;
        testSteps[5]->b[i] = 255;
        testSteps[5]->w[i] = 255;
        
        // Step 6-8: Odd fixtures RED, even fixtures other colors
        testSteps[6]->r[i] = (i % 2 == 0) ? 255 : 0;
        testSteps[6]->g[i] = (i % 2 != 0) ? 255 : 0;
        testSteps[6]->b[i] = 0;
        testSteps[6]->w[i] = 0;
        
        testSteps[7]->r[i] = (i % 2 == 0) ? 0 : 255;
        testSteps[7]->g[i] = (i % 2 == 0) ? 255 : 0;
        testSteps[7]->b[i] = 0;
        testSteps[7]->w[i] = 0;
        
        testSteps[8]->r[i] = (i % 2 == 0) ? 0 : 255;
        testSteps[8]->g[i] = 0;
        testSteps[8]->b[i] = (i % 2 == 0) ? 255 : 0;
        testSteps[8]->w[i] = 0;
        
        // Step 9: Half brightness test
        testSteps[9]->r[i] = 128;
        testSteps[9]->g[i] = 128;
        testSteps[9]->b[i] = 128;
        testSteps[9]->w[i] = 0;
        
        // Step 10: All channels OFF
        testSteps[10]->r[i] = 0;
        testSteps[10]->g[i] = 0;
        testSteps[10]->b[i] = 0;
        testSteps[10]->w[i] = 0;
    }
    
    // Run all test steps
    for (int step = 0; step < numSteps; step++) {
        // Clear all channels
        clearAllChannels();
        
        // Set color for each fixture according to test step
        for (int i = 0; i < _numFixtures; i++) {
            _dmxData[_fixtures[i].redChannel] = testSteps[step]->r[i];
            _dmxData[_fixtures[i].greenChannel] = testSteps[step]->g[i];
            _dmxData[_fixtures[i].blueChannel] = testSteps[step]->b[i];
            _dmxData[_fixtures[i].whiteChannel] = testSteps[step]->w[i];
        }
        
        // Send the data
        sendData();
        
        // Print information
        Serial.print("Test step ");
        Serial.print(step+1);
        Serial.print("/");
        Serial.print(numSteps);
        Serial.print(": ");
        Serial.println(testSteps[step]->description);
        Serial.println("Test will run for 8 seconds...");
        
        // Explain what colors we should actually see based on the channel mapping
        Serial.print("Expected colors - ");
        
        // For each fixture, describe what color should be displayed
        for (int i = 0; i < _numFixtures; i++) {
            Serial.print("Fixture ");
            Serial.print(i+1);
            Serial.print(": ");
            
            // Determine color based on RGB values
            uint8_t r = _dmxData[_fixtures[i].redChannel];
            uint8_t g = _dmxData[_fixtures[i].greenChannel];
            uint8_t b = _dmxData[_fixtures[i].blueChannel];
            uint8_t w = _dmxData[_fixtures[i].whiteChannel];
            
            if (w > 0 || (r > 0 && g > 0 && b > 0)) {
                Serial.print("WHITE");
            } else if (r > 0 && g > 0) {
                Serial.print("YELLOW");
            } else if (r > 0 && b > 0) {
                Serial.print("MAGENTA");
            } else if (g > 0 && b > 0) {
                Serial.print("CYAN");
            } else if (r > 0) {
                Serial.print("RED");
            } else if (g > 0) {
                Serial.print("GREEN");
            } else if (b > 0) {
                Serial.print("BLUE");
    } else {
                Serial.print("OFF");
            }
            
            // Add separator between fixtures except for the last one
            if (i < _numFixtures - 1) {
                Serial.print(", ");
            }
        }
        
        Serial.println();
        
        printFixtureValues();
        
        // Wait for visual confirmation
        delay(8000);  // 8 seconds per step
    }
    
    // Clean up
    for (int step = 0; step < numSteps; step++) {
        delete testSteps[step];
    }
    delete[] testSteps;
    
    Serial.println("Fixture test complete!");
}

// Helper function to blink an LED a specific number of times
void DmxController::blinkLED(int ledPin, int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(ledPin, HIGH);
        delay(delayMs);
        digitalWrite(ledPin, LOW);
        delay(delayMs);
    }
} 