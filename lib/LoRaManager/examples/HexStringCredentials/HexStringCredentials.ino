#include <Arduino.h>
#include <LoRaManager.h>

// Define LoRa pins for HELTEC ESP32 LoRa
#define LORA_CS   18
#define LORA_DIO1 23
#define LORA_RST  14
#define LORA_BUSY 33

// LoRaWAN credentials - Replace with your own
uint64_t joinEUI = 0x0000000000000000;  // Replace with your Join EUI
uint64_t devEUI = 0x0000000000000000;   // Replace with your Device EUI

// Replace with your keys as hex strings
// No need for byte arrays, just use the hex string directly
String appKeyHex = "F30A2F42EAEA8DE5D796A22DBBC86908";  // Example key
String nwkKeyHex = "F30A2F42EAEA8DE5D796A22DBBC86908";  // Example key

// Create LoRaManager instance
LoRaManager lora;

// Timing variables
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 300000;  // 5 minutes in milliseconds

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(3000);  // Give time for the serial monitor to connect
  
  Serial.println("LoRaManager - Hex String Credentials Example");
  Serial.println("===========================================");
  
  // Initialize the LoRa module
  Serial.println("Initializing LoRa module...");
  if (!lora.begin(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY)) {
    Serial.println("Failed to initialize LoRa module!");
    while (1) {
      delay(1000);  // Halt execution
    }
  }
  Serial.println("LoRa module initialized successfully!");
  
  // Set LoRaWAN credentials using hex strings
  Serial.println("Setting credentials from hex strings...");
  if (lora.setCredentialsHex(joinEUI, devEUI, appKeyHex, nwkKeyHex)) {
    Serial.println("Credentials set successfully!");
  } else {
    Serial.println("Failed to set credentials! Check your hex strings.");
    while (1) {
      delay(1000);  // Halt execution
    }
  }
  
  // Join the network
  Serial.println("Joining LoRaWAN network...");
  if (lora.joinNetwork()) {
    Serial.println("Successfully joined the network!");
  } else {
    Serial.println("Failed to join the network. Will retry when sending data.");
  }
}

void loop() {
  unsigned long currentTime = millis();
  
  // Check if it's time to send data
  if (currentTime - lastSendTime >= sendInterval) {
    sendSensorData();
    lastSendTime = currentTime;
  }
  
  // Optional: handle any LoRaWAN events
  lora.handleEvents();
  
  // Other processing or sleep code can go here
  delay(100);
}

void sendSensorData() {
  // Read sensor data (replace with your actual sensor readings)
  float temperature = 25.5;  // Example temperature in Celsius
  float humidity = 65.0;     // Example humidity percentage
  float pressure = 1013.25;  // Example pressure in hPa
  
  // Convert to integers with fixed point precision (1 decimal place)
  int16_t temp_int = (int16_t)(temperature * 10);
  int16_t hum_int = (int16_t)(humidity * 10);
  int16_t press_int = (int16_t)(pressure / 10);  // Reduce to fit in int16_t
  
  // Prepare payload (8 bytes)
  uint8_t payload[8];
  payload[0] = temp_int >> 8;
  payload[1] = temp_int & 0xFF;
  payload[2] = hum_int >> 8;
  payload[3] = hum_int & 0xFF;
  payload[4] = press_int >> 8;
  payload[5] = press_int & 0xFF;
  payload[6] = 0;  // Motion flag bit (set to 0)
  payload[7] = 0;  // Reserved for future use
  
  // Print the payload for debugging
  Serial.print("Sending payload: ");
  for (int i = 0; i < sizeof(payload); i++) {
    if (payload[i] < 16) Serial.print("0");
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Send the data
  Serial.println("Sending data...");
  if (lora.sendData(payload, sizeof(payload), 1, false)) {
    Serial.println("Data sent successfully!");
    
    // Print RSSI and SNR
    Serial.print("RSSI: ");
    Serial.print(lora.getLastRssi());
    Serial.print(" dBm, SNR: ");
    Serial.print(lora.getLastSnr());
    Serial.println(" dB");
  } else {
    Serial.print("Failed to send data! Error code: ");
    Serial.println(lora.getLastErrorCode());
  }
} 