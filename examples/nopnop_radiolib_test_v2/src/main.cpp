/**
 * RadioLib LoRaWAN Class C Test for Heltec WiFi LoRa 32 V3.2
 * 
 * This example demonstrates how to implement LoRaWAN Class C operation
 * using the RadioLib library for the Heltec WiFi LoRa 32 V3.2 board.
 * 
 * Class C devices listen continuously for downlinks when not transmitting.
 * 
 * Created: May 2024
 */

#include <Arduino.h>
#include <heltec_unofficial.h>
#include "secrets.h"  // Contains DEVEUI, APPEUI, APPKEY
#include <RadioLib.h>

// Pin definitions for SX1262 on Heltec WiFi LoRa 32 V3.2
// These are defined by the Heltec_ESP32_LoRa_V3 library
SX1262 radio = NULL;  // Will be initialized in setup()

// LoRaWAN settings
#define LORAWAN_REGION_US915    // US915 region
#define LORAWAN_ADR_ENABLED true
#define LORAWAN_UPLINK_INTERVAL_MS 60000  // 1 minute
#define LORAWAN_CLASS RadioLib::LoRaWANClass::CLASS_C  // Class C operation

// Global variables
RadioLib::LoRaWANNode* node = NULL;
bool joined = false;
unsigned long lastUplinkTime = 0;
volatile bool receivedFlag = false;

// Callback for packet reception
void setFlag(void) {
  receivedFlag = true;
}

// Convert hex strings to byte arrays
void hexStringToByte(const char* hexString, uint8_t* byteArray, size_t length) {
  for (size_t i = 0; i < length; i++) {
    sscanf(hexString + 2 * i, "%2hhx", &byteArray[i]);
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  while (!Serial && millis() < 5000);
  Serial.println("\n\nRadioLib LoRaWAN Class C Test");
  
  // Initialize Heltec board
  Heltec.begin(true, false, true);
  Heltec.display->clear();
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0, 0, "LoRaWAN Class C Test");
  Heltec.display->drawString(0, 12, "RadioLib");
  Heltec.display->display();
  
  // Initialize the SX1262 radio
  Serial.println("Initializing SX1262...");
  LoRa.beginPacket();
  LoRa.endPacket();
  
  // Get a reference to the RadioLib module from the Heltec library
  radio = LoRa._radio;
  
  if (!radio) {
    Serial.println("Failed to get radio reference!");
    Heltec.display->drawString(0, 24, "Radio init failed!");
    Heltec.display->display();
    while (1);
  }
  
  // Set radio parameters for US915
  Serial.println("Setting radio parameters...");
  
  // Initialize the LoRaWAN stack with US915 band
  Serial.println("Initializing LoRaWAN stack...");
  RadioLib::LoRaWANBand_US915 band;
  node = new RadioLib::LoRaWANNode(&radio, &band);
  
  // Set the device class to Class C
  node->setDeviceClass(LORAWAN_CLASS);
  
  // Set ADR if enabled
  node->setADR(LORAWAN_ADR_ENABLED);
  
  // Use channel mask to limit channels to base subband (0-7)
  node->setChannelMask(0, 0x00FF);
  for(int i = 1; i < 8; i++) {
    node->setChannelMask(i, 0x0000);
  }
  
  // Convert keys from hex strings to byte arrays
  uint8_t devEui[8];
  uint8_t appEui[8];
  uint8_t appKey[16];
  
  hexStringToByte(DEVEUI, devEui, 8);
  hexStringToByte(APPEUI, appEui, 8);
  hexStringToByte(APPKEY, appKey, 16);
  
  // Need to reverse byte order for EUIs
  uint64_t devEuiReversed = 0;
  uint64_t appEuiReversed = 0;
  
  for(int i = 0; i < 8; i++) {
    devEuiReversed = (devEuiReversed << 8) | devEui[i];
    appEuiReversed = (appEuiReversed << 8) | appEui[i];
  }
  
  // Set the OTAA parameters
  Serial.println("Setting OTAA parameters...");
  int state = node->beginOTAA(appEuiReversed, devEuiReversed, appKey);
  
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("LoRaWAN initialization failed, code: ");
    Serial.println(state);
    Heltec.display->drawString(0, 24, "LoRaWAN init failed!");
    Heltec.display->display();
    while (1);
  }
  
  // Register the interrupt
  radio.setDio1Action(setFlag);
  
  // Attempt to join the network
  Serial.println("Joining LoRaWAN network...");
  Heltec.display->drawString(0, 24, "Joining network...");
  Heltec.display->display();
  
  // Try to join (with 10 second timeout)
  state = node->joinOTAA();
  
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("LoRaWAN join failed, code: ");
    Serial.println(state);
    Heltec.display->drawString(0, 36, "Join failed!");
    Heltec.display->display();
    while (1);
  }
  
  // Get device address (sanity check for successful join)
  uint32_t devAddr = node->getDevAddr();
  Serial.print("Device address: 0x");
  Serial.println(devAddr, HEX);
  
  // If we get here, join was successful
  joined = true;
  Serial.println("LoRaWAN network joined!");
  Heltec.display->drawString(0, 36, "Network joined!");
  Heltec.display->drawString(0, 48, "Class C active");
  Heltec.display->display();
  
  // Force first uplink immediately
  lastUplinkTime = 0;
}

void loop() {
  // Check if it's time to send a periodic uplink
  if (millis() - lastUplinkTime >= LORAWAN_UPLINK_INTERVAL_MS || lastUplinkTime == 0) {
    // Create a simple uplink message
    uint8_t payload[] = {'C', 'L', 'A', 'S', 'S', '_', 'C'};
    
    // Display uplink status
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "LoRaWAN Class C Test");
    Heltec.display->drawString(0, 12, "Sending uplink...");
    Heltec.display->display();
    
    Serial.println("Sending uplink...");
    
    // Send the uplink on port 1 (unconfirmed)
    int state = node->sendReceive(payload, sizeof(payload), 1);
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("Uplink successful!");
      Heltec.display->drawString(0, 24, "Uplink sent!");
      Heltec.display->display();
    } else {
      Serial.print("Uplink failed, code: ");
      Serial.println(state);
      Heltec.display->drawString(0, 24, "Uplink failed!");
      Heltec.display->display();
    }
    
    lastUplinkTime = millis();
  }
  
  // Check for received downlink packets
  if (receivedFlag) {
    receivedFlag = false;
    
    // Process the received downlink
    Serial.println("Downlink received!");
    
    uint8_t payload[256];
    size_t payloadLen = sizeof(payload);
    int fport = node->getDownlinkFPort(payload, &payloadLen);
    
    if (fport >= 0) {
      Serial.print("Downlink on FPort: ");
      Serial.println(fport);
      Serial.print("Payload (hex): ");
      
      for (size_t i = 0; i < payloadLen; i++) {
        Serial.print(payload[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      
      // Display downlink info
      Heltec.display->drawString(0, 36, "Downlink received!");
      Heltec.display->drawString(0, 48, "FPort: " + String(fport));
      Heltec.display->display();
    } else {
      Serial.print("Failed to process downlink, code: ");
      Serial.println(fport);
    }
  }
  
  // Short delay to prevent CPU hogging
  delay(10);
} 