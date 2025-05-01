/**
 * DmxController.cpp - Implementation of the DmxController library
 * 
 * This is an updated implementation to work with esp_dmx 4.1.0
 * Enhanced with fixture management functionality
 */

#include "DmxController.h"

// Define the static member
const char* DmxController::CUSTOM_PREFS_NAMESPACE = "dmx_custom";

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
    
    // Clear the DMX data buffer first
    memset(_dmxData, 0, DMX_PACKET_SIZE);
    _dmxData[0] = 0; // Start code must be 0
    
    // Always properly delete any previous driver to avoid "already installed" error
    // We'll ignore any errors here since it might not be installed yet
    dmx_driver_delete((dmx_port_t)_dmxPort);
    delay(500); // Give it time to fully uninstall
    
    Serial.println("Installing DMX driver with hardware UART...");
    
    // Set GPIO pins with direct pinMode - critical for proper operation
    pinMode(_dirPin, OUTPUT);
    digitalWrite(_dirPin, HIGH);  // HIGH = transmit mode
    
    // Configure hardware UART directly instead of relying on the driver
    Serial1.begin(250000, SERIAL_8N2, _rxPin, _txPin);
    delay(100); // Allow UART to stabilize
    
    Serial.println("DMX controller initialized successfully!");
    Serial.print("DMX using pins - TX: ");
    Serial.print(_txPin);
    Serial.print(", RX: ");
    Serial.print(_rxPin);
    Serial.print(", DIR: ");
    Serial.println(_dirPin);
    
    // Flag as initialized even if the driver failed, since we'll use direct UART
    _isInitialized = true;
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
    
    // If the driver failed, use direct UART implementation
    if (_isInitialized) {
        // Store current values for debug comparison
        static bool first_send = true;
        static uint8_t last_values[DMX_PACKET_SIZE];
        
        bool values_changed = first_send;
        if (!first_send) {
            // Check if any values changed by more than 5 (to avoid minor fluctuations)
            for (int i = 1; i < DMX_PACKET_SIZE; i++) {
                if (abs(_dmxData[i] - last_values[i]) > 5) {
                    values_changed = true;
                    break;
                }
            }
        }
        
        // IMPROVED DMX OUTPUT PROTOCOL - More reliable timing
        digitalWrite(_dirPin, HIGH);    // Ensure in transmit mode (DE=HIGH, RE=HIGH)
        
        // Generate DMX break without closing UART - more stable method
        Serial1.flush();                // Wait for all data to be sent
        Serial1.updateBaudRate(90000);  // Temporary baud rate change to create break
        Serial1.write(0);               // Send a zero byte at lower baud rate
        Serial1.flush();                // Wait for completion
        Serial1.updateBaudRate(250000); // Restore DMX baud rate (standard)
        
        // Send DMX data - all 513 bytes (start code + 512 channels)
        Serial1.write(_dmxData, DMX_PACKET_SIZE);
        Serial1.flush();                // Ensure all data is completely sent
        
        // Wait longer to ensure data is fully transmitted (helps with stability)
        delay(3); // Increased from 1ms to 3ms
        
        // Save current values for next comparison
        if (first_send || values_changed) {
            memcpy(last_values, _dmxData, DMX_PACKET_SIZE);
            first_send = false;
            
            // Print DMX data values for debugging, but only when they change
            Serial.println("DMX Output Data Updated:");
            
            // Print active channel values (non-zero channels only)
            bool hasActiveChannels = false;
            for (int i = 1; i < DMX_PACKET_SIZE; i++) {
                if (_dmxData[i] > 0) {
                    if (!hasActiveChannels) {
                        Serial.println("Active channels:");
                        hasActiveChannels = true;
                    }
                    Serial.print("  Ch ");
                    Serial.print(i);
                    Serial.print(": ");
                    Serial.println(_dmxData[i]);
                }
            }
            
            if (!hasActiveChannels) {
                Serial.println("No active channels (all values are 0)");
            }
            
            // Print fixture information if available
            if (_fixtures != NULL && _numFixtures > 0) {
                Serial.println("Fixture Colors:");
                for (int i = 0; i < _numFixtures; i++) {
                    Serial.print("  ");
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
            
            // Log success
            Serial.print("DMX data sent (");
            Serial.print(DMX_PACKET_SIZE);
            Serial.println(" bytes)");
        }
    } else {
        Serial.println("DMX not properly initialized, cannot send data");
    }
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

// Run a rainbow chase test pattern across fixtures
void DmxController::runRainbowChase(int cycles, int speedMs, bool staggered) {
    if (_fixtures == NULL || _numFixtures <= 0) {
        Serial.println("No fixtures configured");
        return;
    }
    
    Serial.println("Starting rainbow chase test pattern...");
    Serial.print("Running ");
    Serial.print(cycles);
    Serial.print(" cycles with ");
    Serial.print(speedMs);
    Serial.print("ms delay. Mode: ");
    Serial.println(staggered ? "Staggered/Chase" : "Synchronized");
    
    // Calculate total steps for all cycles (6 color transitions per cycle × 255 steps per transition)
    const int totalSteps = cycles * 6 * 255;
    
    // Run the pattern for the specified number of cycles
    for (int step = 0; step < totalSteps; step++) {
        // Process this step of the animation
        cycleRainbowStep(step, staggered);
        
        // Print progress every 50 steps
        if (step % 50 == 0) {
            Serial.print("Rainbow chase progress: ");
            Serial.print((step * 100) / totalSteps);
            Serial.println("%");
        }
        
        // Delay for the specified time
        delay(speedMs);
    }
    
    // Clear all channels when done
    clearAllChannels();
    sendData();
    
    Serial.println("Rainbow chase test pattern complete!");
}

// Calculate and set a single step of the rainbow pattern
void DmxController::cycleRainbowStep(uint32_t step, bool staggered) {
    if (_fixtures == NULL || _numFixtures <= 0) {
        return;
    }
    
    // Calculate rainbow colors for each fixture
    for (int i = 0; i < _numFixtures; i++) {
        // Calculate hue, shift it for each fixture if staggered
        uint8_t hue = (step + (staggered ? (i * 256 / _numFixtures) : 0)) % 256;
        
        // Convert HSV to RGB (S and V are fixed at 255)
        RgbwColor color = hsvToRgb(hue, 255, 255);
        
        // Set the fixture's color
        setFixtureColor(i, color.r, color.g, color.b, 0);
    }
    
    // Send the DMX data
    sendData();
}

// Thread-safe version of the rainbow step function for use with FreeRTOS
void DmxController::updateRainbowStep(uint32_t step, bool staggered) {
    if (_fixtures == NULL || _numFixtures <= 0) {
        return;
    }
    
    // Calculate rainbow colors for each fixture
    for (int i = 0; i < _numFixtures; i++) {
        // Calculate hue, shift it for each fixture if staggered
        uint8_t hue = (step + (staggered ? (i * 256 / _numFixtures) : 0)) % 256;
        
        // Convert HSV to RGB (S and V are fixed at 255)
        RgbwColor color = hsvToRgb(hue, 255, 255);
        
        // Set the fixture's color
        setFixtureColor(i, color.r, color.g, color.b, 0);
    }
    
    // Don't send data here - the DMX task will handle it
    // This just updates the DMX buffer with new values
}

// Run a strobe test pattern on all fixtures
void DmxController::runStrobeTest(uint8_t color, int count, int onTimeMs, int offTimeMs, bool alternate) {
    if (_fixtures == NULL || _numFixtures <= 0) {
        Serial.println("No fixtures configured");
        return;
    }
    
    Serial.println("Starting strobe test pattern...");
    Serial.print("Running ");
    Serial.print(count);
    Serial.print(" strobe flashes with ");
    Serial.print(onTimeMs);
    Serial.print("ms on time and ");
    Serial.print(offTimeMs);
    Serial.print("ms off time. Mode: ");
    Serial.println(alternate ? "Alternating" : "All fixtures");
    
    // Create arrays for color values
    uint8_t rValues[4] = {255, 255, 0, 0};     // White, Red, Green, Blue
    uint8_t gValues[4] = {255, 0, 255, 0};     // White, Red, Green, Blue
    uint8_t bValues[4] = {255, 0, 0, 255};     // White, Red, Green, Blue
    uint8_t wValues[4] = {255, 0, 0, 0};       // Only White uses W channel
    
    // Ensure color selection is within bounds
    color = min(color, (uint8_t)3);
    
    // Store selected color values
    uint8_t r = rValues[color];
    uint8_t g = gValues[color];
    uint8_t b = bValues[color];
    uint8_t w = wValues[color];
    
    // Print selected color
    Serial.print("Strobe color: ");
    switch(color) {
        case 0: Serial.println("WHITE"); break;
        case 1: Serial.println("RED"); break;
        case 2: Serial.println("GREEN"); break;
        case 3: Serial.println("BLUE"); break;
    }
    
    // Run the strobe pattern
    for (int i = 0; i < count; i++) {
        // Clear all channels first
        clearAllChannels();
        
        // Set the fixture colors based on the selected mode
        if (alternate) {
            // Alternating mode: turn on either odd or even fixtures
            bool evenPhase = (i % 2 == 0);
            
            for (int f = 0; f < _numFixtures; f++) {
                bool isEvenFixture = (f % 2 == 0);
                
                // Only light up fixtures that match the current phase
                if (isEvenFixture == evenPhase) {
                    setFixtureColor(f, r, g, b, w);
                }
            }
        } else {
            // All fixtures mode: turn on all fixtures
            for (int f = 0; f < _numFixtures; f++) {
                setFixtureColor(f, r, g, b, w);
            }
        }
        
        // Send the DMX data for "on" phase
        sendData();
        
        // Print progress every 5 flashes
        if (i % 5 == 0) {
            Serial.print("Strobe flash ");
            Serial.print(i + 1);
            Serial.print("/");
            Serial.println(count);
        }
        
        // Wait for the "on" time
        delay(onTimeMs);
        
        // Turn all fixtures off
        clearAllChannels();
        sendData();
        
        // Wait for the "off" time
        delay(offTimeMs);
    }
    
    // Clear all channels when done
    clearAllChannels();
    sendData();
    
    Serial.println("Strobe test pattern complete!");
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

// Save the current DMX settings to persistent storage
bool DmxController::saveSettings() {
    // Open the preferences with the namespace "dmx_settings"
    if (!_preferences.begin("dmx_settings", false)) {
        Serial.println("Failed to open preferences");
        return false;
    }
    
    // Store the number of fixtures and channels per fixture for validation on load
    _preferences.putInt("num_fixtures", _numFixtures);
    _preferences.putInt("chan_per_fix", _channelsPerFixture);
    
    // Create a buffer for the DMX data (excluding the start code)
    size_t dataSize = DMX_PACKET_SIZE - 1;  // Exclude the start code
    
    // Store the DMX data (excluding the start code at index 0)
    _preferences.putBytes("dmx_data", &_dmxData[1], dataSize);
    
    // Store fixture configurations
    if (_fixtures != NULL && _numFixtures > 0) {
        for (int i = 0; i < _numFixtures; i++) {
            char keyBuffer[32]; // Buffer for storing key names
            
            // Format key names using snprintf
            snprintf(keyBuffer, sizeof(keyBuffer), "fix_%d_addr", i);
            _preferences.putInt(keyBuffer, _fixtures[i].startAddr);
            
            snprintf(keyBuffer, sizeof(keyBuffer), "fix_%d_red", i);
            _preferences.putInt(keyBuffer, _fixtures[i].redChannel);
            
            snprintf(keyBuffer, sizeof(keyBuffer), "fix_%d_green", i);
            _preferences.putInt(keyBuffer, _fixtures[i].greenChannel);
            
            snprintf(keyBuffer, sizeof(keyBuffer), "fix_%d_blue", i);
            _preferences.putInt(keyBuffer, _fixtures[i].blueChannel);
            
            snprintf(keyBuffer, sizeof(keyBuffer), "fix_%d_white", i);
            _preferences.putInt(keyBuffer, _fixtures[i].whiteChannel);
            // We can't store the name directly as it's a char* pointer
        }
    }
    
    _preferences.end();
    Serial.println("DMX settings saved to persistent storage");
    return true;
}

// Load DMX settings from persistent storage
bool DmxController::loadSettings() {
    bool settingsLoaded = false;
    
    // Open the preferences with the namespace "dmx_settings"
    if (!_preferences.begin("dmx_settings", true)) {
        Serial.println("Failed to open preferences");
        return false;
    }
    
    // Check if we have saved settings
    if (_preferences.isKey("dmx_data")) {
        // Validate the number of fixtures and channels per fixture
        int savedNumFixtures = _preferences.getInt("num_fixtures", 0);
        int savedChannelsPerFixture = _preferences.getInt("chan_per_fix", 0);
        
        // Only load if the configuration is compatible
        if (savedNumFixtures == _numFixtures && savedChannelsPerFixture == _channelsPerFixture) {
            // Create a buffer for the DMX data (excluding the start code)
            size_t dataSize = DMX_PACKET_SIZE - 1;  // Exclude the start code
            
            // Load the DMX data (excluding the start code at index 0)
            _preferences.getBytes("dmx_data", &_dmxData[1], dataSize);
            
            // Ensure the start code is 0
            _dmxData[0] = 0;
            
            Serial.println("DMX settings loaded from persistent storage");
            settingsLoaded = true;
        } else {
            Serial.println("Saved DMX configuration is incompatible with current setup");
        }
    } else {
        Serial.println("No saved DMX settings found");
    }
    
    _preferences.end();
    
    // If no settings were loaded, set the default white color
    if (!settingsLoaded) {
        setDefaultWhite();
    }
    
    return settingsLoaded;
}

// Set all fixtures to default white color
void DmxController::setDefaultWhite() {
    Serial.println("Setting all fixtures to default white color");
    
    // Clear all DMX channels first
    clearAllChannels();
    
    // Set all fixtures to white
    if (_fixtures != NULL && _numFixtures > 0) {
        for (int i = 0; i < _numFixtures; i++) {
            // Setting to full white (0 for RGB, 255 for W)
            setFixtureColor(i, 0, 0, 0, 255);
            
            // Log the values being set
            Serial.print("Setting fixture ");
            Serial.print(i);
            Serial.print(" (");
            Serial.print(_fixtures[i].name);
            Serial.print(") to white: W channel ");
            Serial.print(_fixtures[i].whiteChannel);
            Serial.print(" = 255, at DMX addr ");
            Serial.println(_fixtures[i].startAddr);
        }
        
        // Print DMX data for verification
        Serial.println("DMX Data before sending:");
        for (int i = 0; i < _numFixtures; i++) {
            Serial.print("Fixture ");
            Serial.print(i);
            Serial.print(" values - R:");
            Serial.print(_dmxData[_fixtures[i].redChannel]);
            Serial.print(", G:");
            Serial.print(_dmxData[_fixtures[i].greenChannel]);
            Serial.print(", B:");
            Serial.print(_dmxData[_fixtures[i].blueChannel]);
            Serial.print(", W:");
            Serial.println(_dmxData[_fixtures[i].whiteChannel]);
        }
        
        sendData();  // Send data to fixtures
    } else {
        // No fixtures configured, try setting standard RGBW fixtures
        Serial.println("No fixtures configured, setting default RGBW pattern");
        for (int addr = 1; addr <= 16; addr += 4) {
            setManualFixtureColor(addr, 0, 0, 0, 255);  // Set to full white
            
            Serial.print("Set DMX address ");
            Serial.print(addr);
            Serial.println(" to RGBW: [0, 0, 0, 255]");
        }
        sendData();  // Send data to fixtures
    }
}

// Helper to convert HSV to RGB for rainbow effects
RgbwColor DmxController::hsvToRgb(uint8_t h, uint8_t s, uint8_t v) {
    RgbwColor rgb;
    
    // If saturation is 0, it's a shade of gray
    if (s == 0) {
        rgb.r = rgb.g = rgb.b = v;
        rgb.w = 0;
        return rgb;
    }
    
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6; 
    
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
        case 0:  rgb.r = v; rgb.g = t; rgb.b = p; break;
        case 1:  rgb.r = q; rgb.g = v; rgb.b = p; break;
        case 2:  rgb.r = p; rgb.g = v; rgb.b = t; break;
        case 3:  rgb.r = p; rgb.g = q; rgb.b = v; break;
        case 4:  rgb.r = t; rgb.g = p; rgb.b = v; break;
        default: rgb.r = v; rgb.g = p; rgb.b = q; break;
    }
    
    rgb.w = 0; // No white for rainbow effects
    return rgb;
}

bool DmxController::saveCustomData(const char* key, uint8_t* data, size_t size) {
    if (!customPrefs.begin(CUSTOM_PREFS_NAMESPACE, false)) {
        Serial.println("Failed to initialize custom preferences");
        return false;
    }

    bool success = customPrefs.putBytes(key, data, size);
    customPrefs.end();
    
    if (success) {
        Serial.print("Saved custom data for key: ");
        Serial.println(key);
    } else {
        Serial.print("Failed to save custom data for key: ");
        Serial.println(key);
    }
    
    return success;
}

bool DmxController::loadCustomData(const char* key, uint8_t* data, size_t size) {
    if (!customPrefs.begin(CUSTOM_PREFS_NAMESPACE, true)) {
        Serial.println("Failed to initialize custom preferences");
        return false;
    }

    size_t readSize = customPrefs.getBytes(key, data, size);
    customPrefs.end();
    
    bool success = (readSize == size);
    if (success) {
        Serial.print("Loaded custom data for key: ");
        Serial.println(key);
    } else {
        Serial.print("Failed to load custom data for key: ");
        Serial.println(key);
    }
    
    return success;
}

bool DmxController::clearCustomData(const char* key) {
    if (!customPrefs.begin(CUSTOM_PREFS_NAMESPACE, false)) {
        Serial.println("Failed to initialize custom preferences");
        return false;
    }

    bool success = customPrefs.remove(key);
    customPrefs.end();
    
    if (success) {
        Serial.print("Cleared custom data for key: ");
        Serial.println(key);
    } else {
        Serial.print("Failed to clear custom data for key: ");
        Serial.println(key);
    }
    
    return success;
} 