/**
 * DMX Controller Test Program
 * 
 * This program tests DMX initialization with simple error handling
 * Updated to work with esp_dmx 4.1.0
 * Converted to 4-channel RGBW mode (removed strobe channel)
 * Refactored to support a variable number of fixtures
 * Refactored DMX functionality into DmxController library
 */

#include <Arduino.h>
#include "DmxController.h"

// Debug output
#define SERIAL_BAUD 115200

// Diagnostic LED pin
#define LED_PIN 35  // Built-in LED on Heltec LoRa 32 V3

// DMX configuration for Heltec LoRa 32 V3
#define DMX_PORT 1
#define DMX_TX_PIN 19  // TX pin for DMX
#define DMX_RX_PIN 20  // RX pin for DMX
#define DMX_DIR_PIN 5  // DIR pin for DMX (connect to both DE and RE on MAX485)

// FIXTURE CONFIGURATION
#define NUM_FIXTURES 2  // Number of DMX fixtures to control (can be changed)
#define CHANNELS_PER_FIXTURE 4  // Using 4-channel RGBW mode

// FIXTURE 1 CONFIGURATION
#define FIXTURE1_NAME "Light 1"
#define FIXTURE1_START_ADDR 1  // First fixture starts at channel 1
#define FIXTURE1_RED_CHANNEL 1     // Red dimmer
#define FIXTURE1_GREEN_CHANNEL 2   // Green dimmer
#define FIXTURE1_BLUE_CHANNEL 3    // Blue dimmer
#define FIXTURE1_WHITE_CHANNEL 4   // White dimmer

// FIXTURE 2 CONFIGURATION
#define FIXTURE2_NAME "Light 2"
#define FIXTURE2_START_ADDR 5  // Second fixture starts at channel 5 (right after fixture 1)
#define FIXTURE2_RED_CHANNEL 5     // Red dimmer
#define FIXTURE2_GREEN_CHANNEL 6   // Green dimmer
#define FIXTURE2_BLUE_CHANNEL 7    // Blue dimmer
#define FIXTURE2_WHITE_CHANNEL 8   // White dimmer

// ADDRESS SCAN CONFIGURATION
#define SCAN_START_ADDR 1
#define SCAN_END_ADDR 61   // Check addresses at CHANNELS_PER_FIXTURE intervals
#define SCAN_STEP CHANNELS_PER_FIXTURE

// Common settings
#define MAX_BRIGHTNESS 255        // Maximum brightness (0-255)

// Global variables
bool dmxInitialized = false;
DmxController* dmx = NULL;

void setup() {
  // Initialize serial first
  Serial.begin(SERIAL_BAUD);
  delay(3000);
  
  // Set up LED pin
  pinMode(LED_PIN, OUTPUT);
  
  // Initial blink to indicate we're alive
  DmxController::blinkLED(LED_PIN, 2, 500);
  
  // Print startup message with delay between lines to ensure complete transmission
  Serial.println("\n\n===== DMX Test Program =====");
  delay(100);
  Serial.println("Testing serial output...");
  delay(100);
  
  // Print ESP-IDF version only
  Serial.print("ESP-IDF Version: ");
  Serial.println(ESP.getSdkVersion());
  delay(100);
  
  // Print memory info
  Serial.print("Free heap before DMX: ");
  Serial.println(ESP.getFreeHeap());
  delay(100);
  
  // Print pin information
  Serial.println("DMX Pin Configuration:");
  Serial.print("TX Pin: ");
  Serial.print(DMX_TX_PIN);
  Serial.print(" - Function: ");
  Serial.println("DMX Data Output (connect to DI on MAX485)");
  
  Serial.print("RX Pin: ");
  Serial.print(DMX_RX_PIN);
  Serial.print(" - Function: ");
  Serial.println("DMX Data Input (connect to RO on MAX485 if receiving)");
  
  Serial.print("DIR Pin: ");
  Serial.print(DMX_DIR_PIN);
  Serial.print(" - Function: ");
  Serial.println("Direction control (connect to both DE and RE on MAX485)");
  Serial.println("Set HIGH for transmit mode, LOW for receive mode");
  delay(100);
  
  // Create DMX controller with explicit pin configuration
  Serial.println("Creating DMX controller object");
  delay(100);
  
  try {
    dmx = new DmxController(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_DIR_PIN);
    Serial.println("DMX controller object created");
    delay(100);
  } catch (...) {
    Serial.println("ERROR: Failed to create DMX controller object");
    DmxController::blinkLED(LED_PIN, 5, 100);  // Error indicator
    dmx = NULL;
    return;
  }
  
  // Initialize fixtures
  dmx->initializeFixtures(NUM_FIXTURES, CHANNELS_PER_FIXTURE);
  
  // Configure fixture 1
  dmx->setFixtureConfig(0, FIXTURE1_NAME, FIXTURE1_START_ADDR, 
                        FIXTURE1_RED_CHANNEL, FIXTURE1_GREEN_CHANNEL, 
                        FIXTURE1_BLUE_CHANNEL, FIXTURE1_WHITE_CHANNEL);
                        
  // Configure fixture 2
  dmx->setFixtureConfig(1, FIXTURE2_NAME, FIXTURE2_START_ADDR, 
                        FIXTURE2_RED_CHANNEL, FIXTURE2_GREEN_CHANNEL, 
                        FIXTURE2_BLUE_CHANNEL, FIXTURE2_WHITE_CHANNEL);
  
  // Initialize DMX with error handling
  Serial.println("\nInitializing DMX controller...");
  delay(100);
  
  if (dmx != NULL) {
    try {
      dmx->begin();
      dmxInitialized = true;
      Serial.println("DMX controller initialized successfully!");
      delay(100);
      
      // Set initial values to zero and send to make sure fixtures are clear
      dmx->clearAllChannels();
      dmx->sendData();
      Serial.println("DMX channels cleared");
      delay(100);
    } catch (...) {
      Serial.println("ERROR: Exception during DMX initialization!");
      delay(100);
      DmxController::blinkLED(LED_PIN, 5, 100);  // Error indicator
    }
  }
  
  // Print memory after initialization
  Serial.print("Free heap after DMX: ");
  Serial.println(ESP.getFreeHeap());
  delay(100);
  
  Serial.println("Setup complete.");
  delay(100);
  
  // Indicate completion with LED
  if (dmxInitialized) {
    DmxController::blinkLED(LED_PIN, 2, 500);  // Success indicator
    
    // Ask the user if they want to run a channel test
    Serial.println("\nDo you want to run a channel test to identify the correct channels?");
    Serial.println("Send 'y' to start the test, any other key to skip.");
    
    // Wait for user input for up to 5 seconds
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) {
      if (Serial.available() > 0) {
        char input = Serial.read();
        if (input == 'y' || input == 'Y') {
          Serial.println("Running channel test sequence...");
          dmx->testAllChannels();
          break;
        } else {
          Serial.println("Skipping channel test.");
          break;
        }
      }
      delay(100);
    }
    
    // If no input received, skip the test
    if (millis() - startTime >= 5000) {
      Serial.println("No input received, skipping channel test.");
    }
    
    // Now run a test for all fixtures to verify our configuration
    Serial.println("Testing all fixtures with 4-channel RGBW configuration...");
    dmx->testAllFixtures();
    
    // Special message about the address scanner
    Serial.println("\n========= FIXTURE TROUBLESHOOTING =========");
    Serial.println("If fixtures aren't responding, wait for test mode 5 (address scanner)");
    Serial.println("The scanner will try different addresses every few seconds.");
    Serial.println("Watch for fixtures to light up, and note the address shown in the serial monitor.");
    Serial.println("=======================================");
  } else {
    DmxController::blinkLED(LED_PIN, 10, 100);  // Error indicator
  }
}

// Our main loop will iterate through different test patterns
void loop() {
  static int testMode = 0;
  static int counter = 0;
  
  // If DMX initialized successfully, try sending a test pattern
  if (dmxInitialized && dmx != NULL) {
    try {
      // Every 10 cycles, change the test mode
      if (counter % 10 == 0) {
        testMode = (testMode + 1) % 6;  // 6 test modes
        Serial.print("Switching to test mode ");
        Serial.println(testMode);
      }
      
      // Clear all channels first
      dmx->clearAllChannels();
      
      // Apply the current test mode
      switch (testMode) {
        case 0:
          // Mode 0: Alternate Fixture 1 between red and blue
          if (counter % 2 == 0) {
            // RED on fixture 1
            dmx->setFixtureColor(0, 255, 0, 0);
            Serial.print("Setting Fixture 1 to RED (Ch");
            Serial.print(dmx->getFixture(0)->redChannel);
            Serial.println(")");
          } else {
            // BLUE on fixture 1
            dmx->setFixtureColor(0, 0, 0, 255);
            Serial.print("Setting Fixture 1 to BLUE (Ch");
            Serial.print(dmx->getFixture(0)->blueChannel);
            Serial.println(")");
          }
          break;
          
        case 1:
          // Mode 1: If there's at least 2 fixtures, alternate Fixture 2 between red and blue
          if (dmx->getNumFixtures() >= 2) {
            if (counter % 2 == 0) {
              // RED on fixture 2
              dmx->setFixtureColor(1, 255, 0, 0);
              Serial.print("Setting Fixture 2 to RED (Ch");
              Serial.print(dmx->getFixture(1)->redChannel);
              Serial.println(")");
            } else {
              // BLUE on fixture 2
              dmx->setFixtureColor(1, 0, 0, 255);
              Serial.print("Setting Fixture 2 to BLUE (Ch");
              Serial.print(dmx->getFixture(1)->blueChannel);
              Serial.println(")");
            }
          } else {
            // If there's only one fixture, do something with it
            dmx->setFixtureColor(0, 0, 255, 0); // GREEN on fixture 1
            Serial.println("Only one fixture configured - setting Fixture 1 to GREEN");
          }
          break;
          
        case 2:
          // Mode 2: Cycle through RGB on fixture 1
          switch (counter % 3) {
            case 0:
              dmx->setFixtureColor(0, 255, 0, 0);
              Serial.print("Setting Fixture 1 to RED (Ch");
              Serial.print(dmx->getFixture(0)->redChannel);
              Serial.println(")");
              break;
            case 1:
              dmx->setFixtureColor(0, 0, 255, 0);
              Serial.print("Setting Fixture 1 to GREEN (Ch");
              Serial.print(dmx->getFixture(0)->greenChannel);
              Serial.println(")");
              break;
            case 2:
              dmx->setFixtureColor(0, 0, 0, 255);
              Serial.print("Setting Fixture 1 to BLUE (Ch");
              Serial.print(dmx->getFixture(0)->blueChannel);
              Serial.println(")");
              break;
          }
          break;
          
        case 3:
          // Mode 3: All fixtures to WHITE
          Serial.println("Setting ALL fixtures to WHITE (RGBW)");
          for (int i = 0; i < dmx->getNumFixtures(); i++) {
            dmx->setFixtureColor(i, 255, 255, 255, 255);
          }
          break;
          
        case 4:
          // Mode 4: Complementary colors if there are at least 2 fixtures
          if (dmx->getNumFixtures() >= 2) {
            if (counter % 2 == 0) {
              // Fixture 1 RED, Fixture 2 CYAN
              dmx->setFixtureColor(0, 255, 0, 0);
              dmx->setFixtureColor(1, 0, 255, 255);
              Serial.print("Setting Fixture 1 to RED (Ch");
              Serial.print(dmx->getFixture(0)->redChannel);
              Serial.print("), Fixture 2 to CYAN (Ch");
              Serial.print(dmx->getFixture(1)->greenChannel);
              Serial.print("+Ch");
              Serial.print(dmx->getFixture(1)->blueChannel);
              Serial.println(")");
            } else {
              // Fixture 1 GREEN, Fixture 2 MAGENTA
              dmx->setFixtureColor(0, 0, 255, 0);
              dmx->setFixtureColor(1, 255, 0, 255);
              Serial.print("Setting Fixture 1 to GREEN (Ch");
              Serial.print(dmx->getFixture(0)->greenChannel);
              Serial.print("), Fixture 2 to MAGENTA (Ch");
              Serial.print(dmx->getFixture(1)->redChannel);
              Serial.print("+Ch");
              Serial.print(dmx->getFixture(1)->blueChannel);
              Serial.println(")");
            }
          } else {
            // If only one fixture, alternate colors
            if (counter % 2 == 0) {
              dmx->setFixtureColor(0, 255, 255, 0); // YELLOW
              Serial.println("Only one fixture - setting to YELLOW");
            } else {
              dmx->setFixtureColor(0, 0, 255, 255); // CYAN
              Serial.println("Only one fixture - setting to CYAN");
            }
          }
          break;
          
        case 5:
          // Mode 5: Address scan to help find fixtures
          dmx->scanForFixtures(SCAN_START_ADDR, SCAN_END_ADDR, SCAN_STEP);
          break;
      }
      
      // Send the updated DMX data
      dmx->sendData();
      
      // Print diagnostic information
      dmx->printFixtureValues();
      
      // Blink the onboard LED to show we're running
      digitalWrite(LED_PIN, (counter % 2) ? HIGH : LOW);
      
      // Increment counter
      counter++;
      
    } catch (...) {
      Serial.println("ERROR: Exception in loop while sending DMX data");
    }
  } else {
    // Just blink the LED if DMX failed to initialize
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(900);
  }
  
  delay(5000);  // Slowed down to 5 seconds per step
}
