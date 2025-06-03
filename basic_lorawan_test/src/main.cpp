/**
 * LoRaWAN Basic Test for Heltec WiFi LoRa 32 V3.2
 * 
 * This example demonstrates basic LoRaWAN connectivity with the 
 * Heltec WiFi LoRa 32 V3.2 board using the official library.
 * 
 * Created: May 2024
 */

#include <Arduino.h>
#include <heltec_unofficial.h>
#include "secrets.h"  // Contains DEVEUI, APPEUI, APPKEY

// Variables for periodic uplink
#define UPLINK_INTERVAL_MS  60000  // 1 minute
unsigned long lastUplinkTime = 0;

// Flag for join status
bool joined = false;

// Helper functions to convert EUI and key strings to bytes
void stringToEUI(const char* eui_str, uint8_t* eui_bytes) {
  for (int i = 0; i < 8; i++) {
    int offset = i * 2;
    char byte_str[3] = {eui_str[offset], eui_str[offset + 1], 0};
    eui_bytes[i] = strtoul(byte_str, nullptr, 16);
  }
}

void stringToKey(const char* key_str, uint8_t* key_bytes) {
  for (int i = 0; i < 16; i++) {
    int offset = i * 2;
    char byte_str[3] = {key_str[offset], key_str[offset + 1], 0};
    key_bytes[i] = strtoul(byte_str, nullptr, 16);
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== LoRaWAN Basic Test ===\n");
  
  // Initialize Heltec board with display and LoRa
  Heltec.begin(true /*DisplayEnable*/, true /*LoRaEnable*/, true /*SerialEnable*/);
  
  // Set LED pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // Display startup message
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "LoRaWAN Basic Test");
  display.drawString(0, 10, "Initializing...");
  display.display();
  
  // Initialize LoRaWAN - US915 band
  RadioLib::LoRaWANBand* band = new RadioLib::LoRaWANBand_US915();
  RadioLib::LoRaWANNode* node = new RadioLib::LoRaWANNode(&radio, band);
  
  if (!node) {
    Serial.println("Failed to initialize LoRaWAN!");
    display.drawString(0, 20, "LoRaWAN init failed!");
    display.display();
    while (true) { delay(1000); }
  }
  
  // Convert credentials from strings to bytes
  uint8_t devEUI[8];
  uint8_t appEUI[8];  // JoinEUI
  uint8_t appKey[16];
  
  stringToEUI(DEVEUI, devEUI);
  stringToEUI(APPEUI, appEUI);
  stringToKey(APPKEY, appKey);
  
  // Print credentials
  Serial.println("LoRaWAN credentials:");
  Serial.print("  DevEUI: ");
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X", devEUI[i]);
  }
  Serial.println();
  
  Serial.print("  AppEUI: ");
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X", appEUI[i]);
  }
  Serial.println();
  
  Serial.print("  AppKey: ");
  for (int i = 0; i < 16; i++) {
    Serial.printf("%02X", appKey[i]);
  }
  Serial.println();
  
  // Join network
  display.drawString(0, 30, "Joining network...");
  display.display();
  
  Serial.println("Joining LoRaWAN network...");
  int state = node->beginOTAA(appEUI, devEUI, appKey);
  
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Failed to join network, code %d\n", state);
    display.drawString(0, 40, "Join failed!");
    display.display();
    return;
  }
  
  Serial.println("Successfully joined network!");
  joined = true;
  
  // Update display
  display.clear();
  display.drawString(0, 0, "LoRaWAN Basic Test");
  display.drawString(0, 10, "Joined network!");
  display.drawString(0, 20, "DevAddr: " + String(node->getDevAddr(), HEX));
  display.drawString(0, 30, "Waiting for downlink...");
  display.display();
  
  // Send initial uplink
  uint8_t data[] = "Hello from Heltec!";
  node->sendReceive(data, sizeof(data) - 1);
  
  // Update last uplink time
  lastUplinkTime = millis();
  
  // Main loop
  while (true) {
    // Check for new downlinks
    if (node->available()) {
      // Read the received data
      uint8_t data[256];
      size_t len = 0;
      uint8_t port = 0;
      
      int state = node->readData(data, len, &port);
      
      if (state == RADIOLIB_ERR_NONE) {
        // Process the received data
        Serial.printf("Received %d bytes on port %d\n", len, port);
        
        // Print as hex
        Serial.print("Data: ");
        for (size_t i = 0; i < len; i++) {
          Serial.printf("%02X ", data[i]);
        }
        Serial.println();
        
        // Display the downlink
        display.clear();
        display.drawString(0, 0, "Downlink Received!");
        display.drawString(0, 10, String(len) + " bytes on port " + String(port));
        
        // Display first 8 bytes as hex
        String dataStr = "";
        for (size_t i = 0; i < ((len < 8) ? len : 8); i++) {
          char hex[4];
          sprintf(hex, "%02X ", data[i]);
          dataStr += hex;
        }
        display.drawString(0, 20, dataStr);
        display.display();
        
        // Flash LED
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
      }
    }
    
    // Send periodic uplink
    if (joined && (millis() - lastUplinkTime >= UPLINK_INTERVAL_MS)) {
      // Prepare uplink data
      char uplinkMsg[32];
      sprintf(uplinkMsg, "Uplink #%lu", lastUplinkTime / UPLINK_INTERVAL_MS);
      
      // Send uplink
      Serial.printf("Sending uplink: %s\n", uplinkMsg);
      int state = node->sendReceive((uint8_t*)uplinkMsg, strlen(uplinkMsg));
      
      if (state == RADIOLIB_ERR_NONE) {
        Serial.println("Uplink sent successfully!");
      } else {
        Serial.printf("Failed to send uplink, code %d\n", state);
      }
      
      // Update last uplink time
      lastUplinkTime = millis();
    }
    
    // Small delay to prevent CPU hogging
    delay(10);
  }
}

void loop() {
  // Not used - everything is in setup() with an infinite loop
} 