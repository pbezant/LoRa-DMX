/**
 * DMX Controller Test Program
 * 
 * This program tests DMX initialization with simple error handling
 * Updated to work with esp_dmx 4.1.0
 * Converted to 4-channel RGBW mode (removed strobe channel)
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

// FIXTURE 1 CONFIGURATION
#define FIXTURE1_NAME "Light 1"
#define FIXTURE1_START_ADDR 1  // First fixture starts at channel 1

// Direct 4-channel light configuration for fixture 1
#define FIXTURE1_RED_CHANNEL 1     // Red dimmer
#define FIXTURE1_GREEN_CHANNEL 2   // Green dimmer
#define FIXTURE1_BLUE_CHANNEL 3    // Blue dimmer
#define FIXTURE1_WHITE_CHANNEL 4   // White dimmer

// FIXTURE 2 CONFIGURATION
#define FIXTURE2_NAME "Light 2"
#define FIXTURE2_START_ADDR 5  // Second fixture starts at channel 5 (right after fixture 1)

// Direct 4-channel light configuration for fixture 2
#define FIXTURE2_RED_CHANNEL 5     // Red dimmer
#define FIXTURE2_GREEN_CHANNEL 6   // Green dimmer
#define FIXTURE2_BLUE_CHANNEL 7    // Blue dimmer
#define FIXTURE2_WHITE_CHANNEL 8   // White dimmer

// FIXTURE 2 ADDRESS SCAN - Quickly sweep through possible addresses to find fixture 2
// Only test every 4 channels as that's the fixture size
#define SCAN_START_ADDR 1
#define SCAN_END_ADDR 61   // Check addresses 1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61
#define SCAN_STEP 4

// Common settings
#define MAX_BRIGHTNESS 255        // Maximum brightness (0-255)

// Global variables
bool dmxInitialized = false;
DmxController* dmx = NULL;

// Forward declarations
void blinkLED(int times, int delayMs);
void printFixtureValues();
void testAllChannels();
void testBothFixtures();
void scanForFixture2();
void setFixtureColor(int fixtureNum, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
void setManualFixtureColor(int startAddr, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);

// Define a more complete fixture configuration
struct FixtureConfig {
  const char* name;
  int startAddr;
  int redChannel;
  int greenChannel;
  int blueChannel;
  int whiteChannel;
};

// Fixture configurations
const FixtureConfig fixture1 = {
  FIXTURE1_NAME,
  FIXTURE1_START_ADDR,
  FIXTURE1_RED_CHANNEL,
  FIXTURE1_GREEN_CHANNEL,
  FIXTURE1_BLUE_CHANNEL,
  FIXTURE1_WHITE_CHANNEL
};

const FixtureConfig fixture2 = {
  FIXTURE2_NAME,
  FIXTURE2_START_ADDR,
  FIXTURE2_RED_CHANNEL,
  FIXTURE2_GREEN_CHANNEL,
  FIXTURE2_BLUE_CHANNEL,
  FIXTURE2_WHITE_CHANNEL
};

void setup() {
  // Initialize serial first
  Serial.begin(SERIAL_BAUD);
  delay(3000);
  
  // Set up LED pin
  pinMode(LED_PIN, OUTPUT);
  
  // Set the DIR pin high to enable transmit mode on MAX485
  pinMode(DMX_DIR_PIN, OUTPUT);
  digitalWrite(DMX_DIR_PIN, HIGH);  // Set HIGH for transmit mode
  
  // Initial blink to indicate we're alive
  blinkLED(2, 500);
  
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
    blinkLED(5, 100);  // Error indicator
    dmx = NULL;
    return;
  }
  
  // Print DMX configuration
  Serial.println("DMX Configuration:");
  delay(100);
  Serial.print("DMX Port: ");
  Serial.println(DMX_PORT);
  delay(100);
  
  // Print fixture configuration
  Serial.println("\nFixture 1 Configuration (4-channel RGBW):");
  Serial.print("Name: ");
  Serial.println(fixture1.name);
  Serial.print("Start Address: ");
  Serial.println(fixture1.startAddr);
  Serial.print("R: Ch");
  Serial.print(fixture1.redChannel);
  Serial.print(", G: Ch");
  Serial.print(fixture1.greenChannel);
  Serial.print(", B: Ch");
  Serial.print(fixture1.blueChannel);
  Serial.print(", W: Ch");
  Serial.println(fixture1.whiteChannel);
  
  Serial.println("\nFixture 2 Configuration (4-channel RGBW):");
  Serial.print("Name: ");
  Serial.println(fixture2.name);
  Serial.print("Start Address: ");
  Serial.println(fixture2.startAddr);
  Serial.print("R: Ch");
  Serial.print(fixture2.redChannel);
  Serial.print(", G: Ch");
  Serial.print(fixture2.greenChannel);
  Serial.print(", B: Ch");
  Serial.print(fixture2.blueChannel);
  Serial.print(", W: Ch");
  Serial.println(fixture2.whiteChannel);
  
  Serial.println("\nTROUBLESHOOTING: Added address scan mode (test mode 5) to help find fixture 2");
  Serial.println("If fixture 2 isn't responding, it may be set to a different DMX address.");
  Serial.println("Test mode 5 will cycle through possible addresses (1, 5, 9, 13, 17, 21, 25, etc.)");
  Serial.println("When you see fixture 2 respond, note the address and update FIXTURE2_START_ADDR in the code.");
  delay(100);
  
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
      blinkLED(5, 100);  // Error indicator
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
    blinkLED(2, 500);  // Success indicator
    
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
          testAllChannels();
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
    
    // Now run a test for both fixtures to verify our configuration
    Serial.println("Testing both fixtures with new 4-channel RGBW configuration...");
    testBothFixtures();
    
    // Special message about the address scanner
    Serial.println("\n========= FIXTURE 2 TROUBLESHOOTING =========");
    Serial.println("If fixture 2 isn't responding, wait for test mode 5 (address scanner)");
    Serial.println("The scanner will try different addresses every few seconds.");
    Serial.println("Watch for fixture 2 to light up, and note the address shown in the serial monitor.");
    Serial.println("=======================================");
  } else {
    blinkLED(10, 100);  // Error indicator
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
        testMode = (testMode + 1) % 6;  // Now 6 test modes (removed strobe mode)
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
            setFixtureColor(1, 255, 0, 0);
            Serial.println("Setting Fixture 1 to RED (Ch1)");
          } else {
            // BLUE on fixture 1
            setFixtureColor(1, 0, 0, 255);
            Serial.println("Setting Fixture 1 to BLUE (Ch3)");
          }
          break;
          
        case 1:
          // Mode 1: Alternate Fixture 2 between red and blue
          if (counter % 2 == 0) {
            // RED on fixture 2
            setFixtureColor(2, 255, 0, 0);
            Serial.println("Setting Fixture 2 to RED (Ch5)");
          } else {
            // BLUE on fixture 2
            setFixtureColor(2, 0, 0, 255);
            Serial.println("Setting Fixture 2 to BLUE (Ch7)");
          }
          break;
          
        case 2:
          // Mode 2: Cycle through RGB on fixture 1
          switch (counter % 3) {
            case 0:
              setFixtureColor(1, 255, 0, 0);
              Serial.println("Setting Fixture 1 to RED (Ch1)");
              break;
            case 1:
              setFixtureColor(1, 0, 255, 0);
              Serial.println("Setting Fixture 1 to GREEN (Ch2)");
              break;
            case 2:
              setFixtureColor(1, 0, 0, 255);
              Serial.println("Setting Fixture 1 to BLUE (Ch3)");
              break;
          }
          break;
          
        case 3:
          // Mode 3: Both fixtures to WHITE
          // Set both fixtures to white using all RGB and White
          setFixtureColor(1, 255, 255, 255, 255);
          setFixtureColor(2, 255, 255, 255, 255);
          Serial.println("Setting BOTH fixtures to WHITE (RGBW)");
          break;
          
        case 4:
          // Mode 4: Complementary colors
          if (counter % 2 == 0) {
            // Fixture 1 RED, Fixture 2 CYAN
            setFixtureColor(1, 255, 0, 0);
            setFixtureColor(2, 0, 255, 255);
            Serial.println("Setting Fixture 1 to RED (Ch1), Fixture 2 to CYAN (Ch6+7)");
          } else {
            // Fixture 1 GREEN, Fixture 2 MAGENTA
            setFixtureColor(1, 0, 255, 0);
            setFixtureColor(2, 255, 0, 255);
            Serial.println("Setting Fixture 1 to GREEN (Ch2), Fixture 2 to MAGENTA (Ch5+7)");
          }
          break;
          
        case 5:
          // Mode 5: Address scan for fixture 2
          // This mode tries different possible starting addresses to find fixture 2
          scanForFixture2();
          break;
      }
      
      // Send the updated DMX data
      dmx->sendData();
      
      // Print diagnostic information
      printFixtureValues();
      
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
  
  delay(5000);  // Slowed down to 5 seconds per step (was 1 second)
}

// Helper function to scan through possible DMX addresses for fixture 2
void scanForFixture2() {
  static int currentAddr = SCAN_START_ADDR;
  static int currentColor = 0;
  
  // Always keep fixture 1 on with RED to have a reference
  setFixtureColor(1, 255, 0, 0);
  
  // Set 4-channel fixture values at the current test address
  // First clear any old values
  for (int i = 1; i <= 512; i++) {
    if (i < FIXTURE1_START_ADDR || i > FIXTURE1_WHITE_CHANNEL) {  // Skip fixture 1 addresses
      dmx->getDmxData()[i] = 0;
    }
  }
  
  // Set colors
  uint8_t r = 0, g = 0, b = 0;
  switch (currentColor) {
    case 0: r = 255; break; // RED
    case 1: g = 255; break; // GREEN
    case 2: b = 255; break; // BLUE
  }
  
  // Set color at test address
  setManualFixtureColor(currentAddr, r, g, b);
  
  // Print debug info
  Serial.print("FIXTURE 2 SCAN - Testing address ");
  Serial.print(currentAddr);
  Serial.print(" with ");
  Serial.print(currentColor == 0 ? "RED" : (currentColor == 1 ? "GREEN" : "BLUE"));
  Serial.println(" - Watch for fixture 2 response");
  Serial.println("This test will run for 5 seconds...");
  
  // Update for next cycle
  currentColor = (currentColor + 1) % 3;
  if (currentColor == 0) {
    currentAddr += SCAN_STEP;
    if (currentAddr > SCAN_END_ADDR) {
      currentAddr = SCAN_START_ADDR;
    }
  }
}

// Helper function to set a fixture's color with direct RGBW handling at any address
void setManualFixtureColor(int startAddr, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  // Set RGBW values directly to channels starting at startAddr
  dmx->getDmxData()[startAddr] = r;     // Red channel
  dmx->getDmxData()[startAddr + 1] = g; // Green channel
  dmx->getDmxData()[startAddr + 2] = b; // Blue channel
  dmx->getDmxData()[startAddr + 3] = w; // White channel
}

// Helper function to set a fixture's color with direct RGBW handling
void setFixtureColor(int fixtureNum, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  const FixtureConfig* fixture = (fixtureNum == 1) ? &fixture1 : &fixture2;
  
  // Set RGBW values directly to their respective channels
  dmx->getDmxData()[fixture->redChannel] = r;
  dmx->getDmxData()[fixture->greenChannel] = g;
  dmx->getDmxData()[fixture->blueChannel] = b;
  dmx->getDmxData()[fixture->whiteChannel] = w;
}

// Helper function to blink the LED a specific number of times
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);
    delay(delayMs);
  }
}

// Helper function to print fixture values
void printFixtureValues() {
  // Print the first 8 DMX channels for debugging (covers both fixtures)
  Serial.print("DMX Data: [0]=");
  Serial.print(dmx->getDmxData()[0]);  // Start code (should be 0)
  
  for (int i = 1; i <= 8; i++) {
    Serial.print(", [");
    Serial.print(i);
    Serial.print("]=");
    Serial.print(dmx->getDmxData()[i]);
  }
  Serial.println();
  
  // Print fixture 1 RGBW info
  Serial.print(fixture1.name);
  Serial.print(": R=");
  Serial.print(dmx->getDmxData()[fixture1.redChannel]);
  Serial.print(", G=");
  Serial.print(dmx->getDmxData()[fixture1.greenChannel]);
  Serial.print(", B=");
  Serial.print(dmx->getDmxData()[fixture1.blueChannel]);
  Serial.print(", W=");
  Serial.println(dmx->getDmxData()[fixture1.whiteChannel]);
  
  // Print fixture 2 RGBW info
  Serial.print(fixture2.name);
  Serial.print(": R=");
  Serial.print(dmx->getDmxData()[fixture2.redChannel]);
  Serial.print(", G=");
  Serial.print(dmx->getDmxData()[fixture2.greenChannel]);
  Serial.print(", B=");
  Serial.print(dmx->getDmxData()[fixture2.blueChannel]);
  Serial.print(", W=");
  Serial.println(dmx->getDmxData()[fixture2.whiteChannel]);
}

// Run a channel test at startup to help identify the correct channels
void testAllChannels() {
  Serial.println("Starting channel test sequence...");
  
  // Test each channel individually
  for (int channel = 1; channel <= 8; channel++) {  // Now just testing 8 channels for both fixtures
    // Clear all channels
    dmx->clearAllChannels();
    
    // Set this channel to maximum
    dmx->getDmxData()[channel] = 255;
    
    // Send the data
    dmx->sendData();
    
    // Print diagnostic information
    Serial.print("Testing DMX channel ");
    Serial.print(channel);
    Serial.println(" - set to 255");
    
    // Channel interpretation help
    if (channel == fixture1.redChannel) {
      Serial.println("  This is Fixture 1 Red Channel");
    } else if (channel == fixture1.greenChannel) {
      Serial.println("  This is Fixture 1 Green Channel");
    } else if (channel == fixture1.blueChannel) {
      Serial.println("  This is Fixture 1 Blue Channel");
    } else if (channel == fixture1.whiteChannel) {
      Serial.println("  This is Fixture 1 White Channel");
    } else if (channel == fixture2.redChannel) {
      Serial.println("  This is Fixture 2 Red Channel");
    } else if (channel == fixture2.greenChannel) {
      Serial.println("  This is Fixture 2 Green Channel");
    } else if (channel == fixture2.blueChannel) {
      Serial.println("  This is Fixture 2 Blue Channel");
    } else if (channel == fixture2.whiteChannel) {
      Serial.println("  This is Fixture 2 White Channel");
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
  dmx->clearAllChannels();
  dmx->sendData();
  Serial.println("Channel test complete, all channels cleared");
}

// Test both fixtures with the new configuration
void testBothFixtures() {
  Serial.println("Testing both fixtures with 4-channel RGBW configuration...");
  Serial.println("Channel mapping: Ch1/5=Red, Ch2/6=Green, Ch3/7=Blue, Ch4/8=White");
  Serial.println("Tests slowed down to 8 seconds per step");
  
  // TestStep struct with exactly 9 fields total (string + 8 uint8_t values)
  struct TestStep {
    const char* description; // 1 field (string)
    uint8_t f1r;            // 2
    uint8_t f1g;            // 3
    uint8_t f1b;            // 4
    uint8_t f1w;            // 5
    uint8_t f2r;            // 6
    uint8_t f2g;            // 7
    uint8_t f2b;            // 8
    uint8_t f2w;            // 9
  };
  
  // Each initializer must have exactly 9 values (matching the 9 fields above)
  TestStep testSteps[] = {
    // Format: {description, f1r, f1g, f1b, f1w, f2r, f2g, f2b, f2w}
    {"Both fixtures RED",             255, 0,   0,   0,   255, 0,   0,   0},
    {"Both fixtures GREEN",           0,   255, 0,   0,   0,   255, 0,   0},
    {"Both fixtures BLUE",            0,   0,   255, 0,   0,   0,   255, 0},
    {"Both fixtures WHITE (W only)",  0,   0,   0,   255, 0,   0,   0,   255},
    {"Both fixtures WHITE (RGB)",     255, 255, 255, 0,   255, 255, 255, 0},
    {"Both fixtures WHITE (RGBW)",    255, 255, 255, 255, 255, 255, 255, 255},
    {"Fixture 1 RED, Fixture 2 GREEN",255, 0,   0,   0,   0,   255, 0,   0},
    {"Fixture 1 GREEN, Fixture 2 BLUE",0,  255, 0,   0,   0,   0,   255, 0},
    {"Fixture 1 BLUE, Fixture 2 RED", 0,   0,   255, 0,   255, 0,   0,   0},
    {"Half brightness test",          128, 128, 128, 0,   128, 128, 128, 0},
    {"All channels OFF",              0,   0,   0,   0,   0,   0,   0,   0}
  };
  
  int numSteps = sizeof(testSteps) / sizeof(TestStep);
  
  for (int i = 0; i < numSteps; i++) {
    // Clear all channels
    dmx->clearAllChannels();
    
    // Set up fixture 1
    dmx->getDmxData()[fixture1.redChannel] = testSteps[i].f1r;
    dmx->getDmxData()[fixture1.greenChannel] = testSteps[i].f1g;
    dmx->getDmxData()[fixture1.blueChannel] = testSteps[i].f1b;
    dmx->getDmxData()[fixture1.whiteChannel] = testSteps[i].f1w;
    
    // Set up fixture 2
    dmx->getDmxData()[fixture2.redChannel] = testSteps[i].f2r;
    dmx->getDmxData()[fixture2.greenChannel] = testSteps[i].f2g;
    dmx->getDmxData()[fixture2.blueChannel] = testSteps[i].f2b;
    dmx->getDmxData()[fixture2.whiteChannel] = testSteps[i].f2w;
    
    // Send the data
    dmx->sendData();
    
    // Print information
    Serial.print("Test step ");
    Serial.print(i+1);
    Serial.print("/");
    Serial.print(numSteps);
    Serial.print(": ");
    Serial.println(testSteps[i].description);
    Serial.print("Test will run for 8 seconds...");
    
    // Explain what colors we should actually see based on the channel mapping
    Serial.print("Expected colors - ");
    
    // Describe what fixture 1 should show based on actual channel mapping
    Serial.print("Fixture 1: ");
    if (dmx->getDmxData()[fixture1.whiteChannel] > 0 ||
        (dmx->getDmxData()[fixture1.redChannel] > 0 && 
         dmx->getDmxData()[fixture1.greenChannel] > 0 && 
         dmx->getDmxData()[fixture1.blueChannel] > 0)) {
      Serial.print("WHITE");
    } else if (dmx->getDmxData()[fixture1.redChannel] > 0 && dmx->getDmxData()[fixture1.greenChannel] > 0) {
      Serial.print("YELLOW");
    } else if (dmx->getDmxData()[fixture1.redChannel] > 0 && dmx->getDmxData()[fixture1.blueChannel] > 0) {
      Serial.print("MAGENTA");
    } else if (dmx->getDmxData()[fixture1.greenChannel] > 0 && dmx->getDmxData()[fixture1.blueChannel] > 0) {
      Serial.print("CYAN");
    } else if (dmx->getDmxData()[fixture1.redChannel] > 0) {
      Serial.print("RED");
    } else if (dmx->getDmxData()[fixture1.greenChannel] > 0) {
      Serial.print("GREEN");
    } else if (dmx->getDmxData()[fixture1.blueChannel] > 0) {
      Serial.print("BLUE");
    } else {
      Serial.print("OFF");
    }
    
    // Describe what fixture 2 should show based on actual channel mapping
    Serial.print(", Fixture 2: ");
    if (dmx->getDmxData()[fixture2.whiteChannel] > 0 ||
        (dmx->getDmxData()[fixture2.redChannel] > 0 && 
         dmx->getDmxData()[fixture2.greenChannel] > 0 && 
         dmx->getDmxData()[fixture2.blueChannel] > 0)) {
      Serial.print("WHITE");
    } else if (dmx->getDmxData()[fixture2.redChannel] > 0 && dmx->getDmxData()[fixture2.greenChannel] > 0) {
      Serial.print("YELLOW");
    } else if (dmx->getDmxData()[fixture2.redChannel] > 0 && dmx->getDmxData()[fixture2.blueChannel] > 0) {
      Serial.print("MAGENTA");
    } else if (dmx->getDmxData()[fixture2.greenChannel] > 0 && dmx->getDmxData()[fixture2.blueChannel] > 0) {
      Serial.print("CYAN");
    } else if (dmx->getDmxData()[fixture2.redChannel] > 0) {
      Serial.print("RED");
    } else if (dmx->getDmxData()[fixture2.greenChannel] > 0) {
      Serial.print("GREEN");
    } else if (dmx->getDmxData()[fixture2.blueChannel] > 0) {
      Serial.print("BLUE");
    } else {
      Serial.print("OFF");
    }
    
    Serial.println();
    
    printFixtureValues();
    
    // Wait for visual confirmation
    delay(8000);  // Slowed down to 8 seconds per step (was 2 seconds)
  }
  
  Serial.println("Fixture test complete!");
}
