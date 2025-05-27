#include <Arduino.h>
#include <RadioLib.h>
#include <heltec_unofficial.h>  // Contains the 'radio' instance for Heltec board

// Uncomment to run this test instead of the main program
//#define RUN_LORA_TEST

#ifdef RUN_LORA_TEST

void setup() {
  Serial.begin(115200);
  delay(300);
  
  Serial.println("Starting SX1262 Radio Test...");
  
  // Initialize display (on Heltec boards)
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawString(0, 0, "SX1262 Radio Test");
  display.drawString(0, 10, "Initializing...");
  display.display();

  // Initialize SX1262 with default settings
  // The heltec_unofficial.h already initializes 'radio' as an SX1262 instance
  Serial.print("Initializing SX1262... ");
  
  // Reset radio to ensure clean state
  pinMode(RST, OUTPUT);
  digitalWrite(RST, LOW);
  delay(50);
  digitalWrite(RST, HIGH);
  delay(100);
  
  // Try different bandwidths to diagnose INVALID_BANDWIDTH error
  // Standard LoRa bandwidths for SX1262: 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500 kHz
  float testBandwidths[] = {125.0, 250.0, 500.0, 62.5, 31.25, 41.7, 20.8, 15.6, 10.4, 7.8};
  
  bool success = false;
  float workingBw = 0;
  
  for (int i = 0; i < sizeof(testBandwidths)/sizeof(float); i++) {
    float bw = testBandwidths[i];
    Serial.print("Testing bandwidth: ");
    Serial.print(bw);
    Serial.print(" kHz... ");
    
    display.clear();
    display.drawString(0, 0, "SX1262 Radio Test");
    display.drawString(0, 10, "Testing BW: " + String(bw) + " kHz");
    display.display();
    
    // Common frequency for US915 - we'll try different LoRa parameters
    int state = radio.begin(915.0, bw, 7, 5);
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("SUCCESS!");
      display.drawString(0, 20, "SUCCESS!");
      display.display();
      success = true;
      workingBw = bw;
      break;
    } else {
      Serial.print("Failed, code: ");
      Serial.println(state);
      display.drawString(0, 20, "Failed: " + String(state));
      display.display();
      delay(1000);
    }
  }
  
  if (success) {
    Serial.println("SX1262 initialized successfully!");
    Serial.print("Working bandwidth: ");
    Serial.print(workingBw);
    Serial.println(" kHz");
    
    display.clear();
    display.drawString(0, 0, "SX1262 SUCCESS!");
    display.drawString(0, 10, "BW: " + String(workingBw) + " kHz");
    display.drawString(0, 20, "Testing TX/RX...");
    display.display();
    
    // Now let's try to transmit a packet
    Serial.println("Sending test packet...");
    
    String testMsg = "SX1262 TEST";
    int txResult = radio.transmit(testMsg);
    
    if (txResult == RADIOLIB_ERR_NONE) {
      Serial.println("Transmission successful!");
      display.drawString(0, 30, "TX: Success!");
    } else {
      Serial.print("Transmission failed, code: ");
      Serial.println(txResult);
      display.drawString(0, 30, "TX Failed: " + String(txResult));
    }
    display.display();
    
    // Switch to receive mode
    Serial.println("Switching to receive mode...");
    display.drawString(0, 40, "Waiting for packets...");
    display.display();
    
    // Start listening with 5-second timeout
    String rxData;
    int rxResult = radio.receive(rxData, 5000);
    
    if (rxResult == RADIOLIB_ERR_NONE) {
      Serial.println("Reception successful!");
      Serial.print("Data: ");
      Serial.println(rxData);
      
      display.drawString(0, 50, "RX: " + rxData);
    } else if (rxResult == RADIOLIB_ERR_RX_TIMEOUT) {
      Serial.println("Reception timed out!");
      display.drawString(0, 50, "RX: Timeout");
    } else {
      Serial.print("Reception failed, code: ");
      Serial.println(rxResult);
      display.drawString(0, 50, "RX Failed: " + String(rxResult));
    }
    display.display();
  } else {
    Serial.println("Could not initialize SX1262 with any bandwidth!");
    display.clear();
    display.drawString(0, 0, "SX1262 FAILED!");
    display.drawString(0, 10, "Could not initialize");
    display.drawString(0, 20, "with any bandwidth");
    display.display();
  }
}

void loop() {
  // Just periodically send a test packet
  static unsigned long lastSend = 0;
  
  if (millis() - lastSend > 10000) {  // Send every 10 seconds
    lastSend = millis();
    
    String message = "PING: " + String(millis());
    Serial.print("Sending: ");
    Serial.println(message);
    
    display.clear();
    display.drawString(0, 0, "SX1262 Test");
    display.drawString(0, 10, "Sending: " + message);
    display.display();
    
    int state = radio.transmit(message);
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("Transmission successful!");
      display.drawString(0, 20, "TX: Success!");
    } else {
      Serial.print("Transmission failed, code: ");
      Serial.println(state);
      display.drawString(0, 20, "TX Failed: " + String(state));
    }
    display.display();
  }
  
  // Check for incoming packets
  String rxData;
  int state = radio.receive(rxData, 1000);  // 1-second timeout
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Received packet!");
    Serial.print("Data: ");
    Serial.println(rxData);
    
    display.clear();
    display.drawString(0, 0, "SX1262 Test");
    display.drawString(0, 10, "Received: " + rxData);
    display.drawString(0, 20, "RSSI: " + String(radio.getRSSI()) + " dBm");
    display.drawString(0, 30, "SNR: " + String(radio.getSNR()) + " dB");
    display.display();
    
    delay(2000);  // Display the received message for a while
  }
}

#else
// Dummy setup/loop functions when not running the test
void lora_test_setup() {}
void lora_test_loop() {}
#endif 