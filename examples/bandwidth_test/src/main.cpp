/**
 * SX1262 Bandwidth Testing Tool for Heltec WiFi LoRa 32 V3.2
 * 
 * This tool tests all possible bandwidths supported by the SX1262 chip
 * to identify which ones work with our specific hardware. The results 
 * will inform our implementation of the hybrid approach using nopnop2002's
 * SX1262 driver with RadioLib's LoRaWAN stack.
 * 
 * Created: June 2024
 */

#include <Arduino.h>
#include <heltec_unofficial.h>

// Test parameters
#define TEST_FREQUENCY 915.0  // MHz (US915 band)
#define TEST_POWER 14         // dBm
#define DELAY_BETWEEN_TESTS 1000 // ms

// Test bandwidths (kHz)
const float bandwidths[] = {
  7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0, 500.0
};
const int numBandwidths = sizeof(bandwidths) / sizeof(float);

// Result storage
bool bandwidthResults[sizeof(bandwidths) / sizeof(float)];

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(2000); // Longer delay for serial to stabilize
  Serial.println("\n\nSX1262 Bandwidth Testing Tool - Simple Version");
  
  // Initialize Heltec board (this initializes the display)
  heltec_setup();
  delay(500); // Extra delay after initialization
  
  // Display startup message
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "SX1262 Bandwidth Test");
  display.drawString(0, 12, "Initializing...");
  display.display();
  
  // Hard reset the radio
  Serial.println("Performing hardware reset of radio...");
  pinMode(RST_LoRa, OUTPUT);
  digitalWrite(RST_LoRa, LOW);
  delay(100);  // longer reset time
  digitalWrite(RST_LoRa, HIGH);
  delay(200);  // wait longer after reset
  
  // Print the pin configuration for diagnostic purposes
  Serial.println("SX1262 Pin Configuration:");
  Serial.printf("NSS: GPIO %d\n", SS);
  Serial.printf("RESET: GPIO %d\n", RST_LoRa);
  Serial.printf("BUSY: GPIO %d\n", BUSY_LoRa);
  Serial.printf("DIO1: GPIO %d\n", DIO1);
  Serial.printf("MOSI: GPIO %d\n", MOSI);
  Serial.printf("MISO: GPIO %d\n", MISO);
  Serial.printf("SCK: GPIO %d\n", SCK);
  
  // Initialize radio with specific settings
  Serial.println("Initializing radio with RadioLib...");
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Radio initialization failed, code %d\n", state);
    display.drawString(0, 24, "Radio init failed!");
    display.display();
    while (1) { delay(1000); }
  }
  
  // Delay after initialization
  delay(500);
  
  // Set basic parameters first
  Serial.println("Setting basic radio parameters...");
  
  state = radio.setFrequency(TEST_FREQUENCY);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Failed to set frequency, code %d\n", state);
  }
  delay(100);
  
  state = radio.setSpreadingFactor(9);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Failed to set spreading factor, code %d\n", state);
  }
  delay(100);
  
  state = radio.setCodingRate(7);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Failed to set coding rate, code %d\n", state);
  }
  delay(100);
  
  state = radio.setOutputPower(TEST_POWER);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Failed to set output power, code %d\n", state);
  }
  delay(100);
  
  state = radio.setSyncWord(0x12);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Failed to set sync word, code %d\n", state);
  }
  delay(100);
  
  Serial.println("Radio initialized successfully with custom parameters");
  display.drawString(0, 24, "Radio initialized");
  display.display();
  delay(1000);
  
  // Initialize result array
  for (int i = 0; i < numBandwidths; i++) {
    bandwidthResults[i] = false;
  }
  
  // Start bandwidth testing
  display.clear();
  display.drawString(0, 0, "SX1262 Bandwidth Test");
  display.drawString(0, 12, "Testing bandwidths...");
  display.display();
  
  Serial.println("\nStarting bandwidth tests:");
  Serial.println("------------------------");
  
  // Test each bandwidth
  for (int i = 0; i < numBandwidths; i++) {
    Serial.printf("Testing bandwidth %.2f kHz... ", bandwidths[i]);
    display.drawString(0, 24, "Testing: " + String(bandwidths[i]) + " kHz");
    display.display();
    
    // Try direct setting
    state = radio.setBandwidth(bandwidths[i]);
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("SUCCESS!");
      bandwidthResults[i] = true;
    } else {
      Serial.printf("FAILED (Error: %d)\n", state);
      bandwidthResults[i] = false;
    }
    
    delay(DELAY_BETWEEN_TESTS);
  }
  
  // Display results
  Serial.println("\nTest Results:");
  Serial.println("------------");
  
  display.clear();
  display.drawString(0, 0, "Test Results:");
  
  int validCount = 0;
  int yPos = 12;
  
  for (int i = 0; i < numBandwidths; i++) {
    if (bandwidthResults[i]) {
      Serial.printf("%.2f kHz: SUPPORTED\n", bandwidths[i]);
      if (yPos < 54) {
        display.drawString(0, yPos, String(bandwidths[i]) + " kHz: YES");
        yPos += 10;
      }
      validCount++;
    } else {
      Serial.printf("%.2f kHz: NOT SUPPORTED\n", bandwidths[i]);
    }
  }
  
  Serial.printf("\nFound %d supported bandwidths out of %d tested.\n", validCount, numBandwidths);
  
  if (validCount == 0) {
    Serial.println("WARNING: No compatible bandwidths found!");
    Serial.println("This might indicate a hardware issue or incompatibility.");
    Serial.println("Possible solutions:");
    Serial.println("1. Check SPI connections between ESP32 and SX1262");
    Serial.println("2. Try different RadioLib version");
    Serial.println("3. Try using the nopnop2002 driver directly");
    display.drawString(0, 54, "No compatible BW found!");
  } else {
    display.drawString(0, 54, String(validCount) + " compatible BW found");
  }
  
  display.display();
}

void loop() {
  // Run the Heltec loop (handles button presses, etc.)
  heltec_loop();
  delay(1000);
} 