/*
 * SX1262 Bandwidth Compatibility Test
 * 
 * This sketch tests different bandwidths supported by the SX1262 radio
 * on the Heltec WiFi LoRa 32 v3.2 board to identify which ones work
 * with our specific hardware.
 * 
 * Results are reported via Serial.
 */

#include <Arduino.h>
#include <heltec_unofficial.h>  // Provides global 'radio' and board specifics

// Pin definitions
#define NSS       GPIO_NUM_8
#define DIO1      GPIO_NUM_14 
#define NRST      GPIO_NUM_12
#define BUSY      GPIO_NUM_13

// Test frequencies
#define FREQ_915      915.0  // MHz
#define FREQ_868      868.0  // MHz

// SX1262 bandwidth options to test (in kHz)
const float bandwidths[] = {
  7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0, 500.0
};
const int num_bandwidths = sizeof(bandwidths) / sizeof(float);

// Variables to track results
bool bandwidth_supported[sizeof(bandwidths) / sizeof(float)];
String error_messages[sizeof(bandwidths) / sizeof(float)];

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== SX1262 Bandwidth Compatibility Test ===\n");
  Serial.println("Testing which bandwidths are supported by this specific SX1262 hardware.");
  Serial.println("This will help diagnose RADIOLIB_ERR_INVALID_BANDWIDTH errors.");
  
  // Initialize display
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawString(0, 0, "SX1262 Bandwidth Test");
  display.drawString(0, 10, "Testing...");
  display.display();
  
  // Initialize radio
  Serial.println("Initializing SX1262 radio...");
  
  if (radio.begin() != RADIOLIB_ERR_NONE) {
    Serial.println("Failed to initialize radio!");
    display.drawString(0, 20, "Radio init failed!");
    display.display();
    while (true) { delay(1000); }
  }
  
  Serial.println("Radio initialized successfully!");
  display.drawString(0, 20, "Radio OK!");
  display.display();
  delay(1000);
  
  // Test different bandwidths
  Serial.println("\nTesting bandwidths at 915 MHz:");
  display.clear();
  display.drawString(0, 0, "Testing 915 MHz:");
  display.display();
  
  testAllBandwidths(FREQ_915);
  
  Serial.println("\nTesting bandwidths at 868 MHz:");
  display.clear();
  display.drawString(0, 0, "Testing 868 MHz:");
  display.display();
  
  testAllBandwidths(FREQ_868);
  
  // Display summary
  Serial.println("\n=== Summary of Results ===");
  Serial.println("Bandwidths supported at 915 MHz:");
  for (int i = 0; i < num_bandwidths; i++) {
    if (bandwidth_supported[i]) {
      Serial.print("✓ ");
    } else {
      Serial.print("✗ ");
    }
    Serial.print(bandwidths[i], 2);
    Serial.println(" kHz");
    if (!bandwidth_supported[i] && error_messages[i].length() > 0) {
      Serial.print("  Error: ");
      Serial.println(error_messages[i]);
    }
  }
  
  // Show results on display
  display.clear();
  display.drawString(0, 0, "Results (915 MHz):");
  int y = 10;
  for (int i = 0; i < num_bandwidths && y < 55; i++) {
    String result = bandwidth_supported[i] ? "✓" : "✗";
    display.drawString(0, y, result + " " + String(bandwidths[i], 1) + " kHz");
    y += 10;
  }
  display.display();
}

void loop() {
  // Nothing to do in loop
  delay(1000);
}

void testAllBandwidths(float frequency) {
  for (int i = 0; i < num_bandwidths; i++) {
    Serial.print("Testing bandwidth ");
    Serial.print(bandwidths[i], 2);
    Serial.print(" kHz... ");
    
    display.drawString(0, 10, "Testing " + String(bandwidths[i], 1) + " kHz");
    display.display();
    
    // Set frequency first (always works)
    int state = radio.setFrequency(frequency);
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print("Failed to set frequency! Error: ");
      Serial.println(state);
      bandwidth_supported[i] = false;
      error_messages[i] = "Frequency error: " + String(state);
      continue;
    }
    
    // Try setting bandwidth
    state = radio.setBandwidth(bandwidths[i]);
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("OK!");
      bandwidth_supported[i] = true;
      error_messages[i] = "";
    } else {
      Serial.print("Failed! Error code: ");
      Serial.println(state);
      bandwidth_supported[i] = false;
      error_messages[i] = "Error: " + String(state);
    }
    
    // Test if we can actually receive/transmit with this bandwidth
    if (bandwidth_supported[i]) {
      // Set other parameters
      state = radio.setSpreadingFactor(7);
      state |= radio.setCodingRate(5);
      state |= radio.setSyncWord(0x12);
      
      if (state != RADIOLIB_ERR_NONE) {
        Serial.println("  Warning: Failed to set other parameters!");
        error_messages[i] = "Param error: " + String(state);
      } else {
        // Try to actually configure the radio for RX
        state = radio.startReceive();
        if (state != RADIOLIB_ERR_NONE) {
          Serial.print("  Warning: Failed to start receiver! Error: ");
          Serial.println(state);
          error_messages[i] = "RX error: " + String(state);
          bandwidth_supported[i] = false;
        } else {
          // Successfully configured for RX
          Serial.println("  Verified RX mode works!");
          
          // Cancel RX
          radio.standby();
        }
      }
    }
    
    delay(100);
  }
} 