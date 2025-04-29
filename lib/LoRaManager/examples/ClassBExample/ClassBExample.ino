/*
 * LoRaWAN Class B Example
 * 
 * This example demonstrates how to use LoRaManager's Class B functionality
 * to enable devices to receive scheduled downlink messages from the network
 * using beacon synchronization and ping slots.
 * 
 * Note: Your LoRaWAN network server must support Class B operations
 * for this functionality to work properly.
 */

#include <Arduino.h>
#include <LoRaManager.h>

// Define LoRa pins for your board
#define LORA_CS   18
#define LORA_DIO1 23
#define LORA_RST  14
#define LORA_BUSY 33

// LED pin for visualization (change as needed for your board)
#define LED_PIN 2

// LoRaWAN credentials - replace with your own
uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI = 0x0000000000000000;
uint8_t appKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t nwkKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Create LoRaManager instance
LoRaManager lora;

// Variables to track beacon status
bool beaconSyncAchieved = false;
unsigned long lastBeaconInfoTime = 0;
const unsigned long beaconInfoInterval = 60000; // Print beacon info every minute

// Callback for downlink data
void handleDownlink(uint8_t* payload, size_t size, uint8_t port) {
  Serial.print("Received downlink on port ");
  Serial.print(port);
  Serial.print(" (");
  Serial.print(size);
  Serial.print(" bytes): ");
  
  for (size_t i = 0; i < size; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Example: If payload[0] is 1, turn on LED, if 0 turn it off
  if (size > 0) {
    if (payload[0] == 0x01) {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED turned ON by downlink command");
    } else if (payload[0] == 0x00) {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED turned OFF by downlink command");
    }
  }
}

// Callback for beacon reception
void handleBeacon(uint8_t* payload, size_t size, float rssi, float snr) {
  Serial.println("=== BEACON RECEIVED ===");
  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.print(" dBm, SNR: ");
  Serial.print(snr);
  Serial.println(" dB");
  
  // In a real implementation, you might want to extract and display
  // time information from the beacon payload
  Serial.print("Beacon payload: ");
  for (size_t i = 0; i < size; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Set flag that beacon sync is achieved
  beaconSyncAchieved = true;
  
  // Blink LED to indicate beacon reception
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(3000); // Give time for the serial console to connect
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("\n==============================================");
  Serial.println("LoRaWAN Class B Example");
  Serial.println("==============================================\n");
  
  // Initialize the LoRa module
  Serial.println("Initializing LoRa module...");
  if (!lora.begin(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY)) {
    Serial.println("Failed to initialize LoRa module!");
    while (1) {
      // Flash LED rapidly to indicate error
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }
  Serial.println("LoRa module initialized successfully");
  
  // Set LoRaWAN credentials
  lora.setCredentials(joinEUI, devEUI, appKey, nwkKey);
  
  // Register callbacks
  lora.setDownlinkCallback(handleDownlink);
  lora.setBeaconCallback(handleBeacon);
  
  // Join the network (this is required before switching to Class B)
  Serial.println("Joining LoRaWAN network...");
  bool joinSuccess = false;
  for (int attempt = 1; attempt <= 3 && !joinSuccess; attempt++) {
    Serial.print("Join attempt ");
    Serial.print(attempt);
    Serial.print("/3... ");
    
    joinSuccess = lora.joinNetwork();
    
    if (joinSuccess) {
      Serial.println("SUCCESS!");
      break;
    } else {
      Serial.println("FAILED");
      delay(5000); // Wait 5 seconds before trying again
    }
  }
  
  if (!joinSuccess) {
    Serial.println("Failed to join network after multiple attempts.");
    Serial.println("The example will continue and retry joining later.");
  } else {
    Serial.println("Successfully joined the LoRaWAN network");
    
    // Switch to Class B mode
    Serial.println("Switching to Class B mode...");
    if (lora.setDeviceClass(DEVICE_CLASS_B)) {
      Serial.println("Device is now in Class B mode");
      
      // Set ping slot periodicity (0 = most frequent, 7 = least frequent)
      // For testing, we use 0 for maximum downlink opportunities
      Serial.println("Setting ping slot periodicity to 0 (most frequent)...");
      lora.setPingSlotPeriodicity(0);
      
      Serial.println("Waiting for beacon acquisition...");
    } else {
      Serial.println("Failed to switch to Class B mode");
    }
  }
  
  Serial.println("\nSetup complete. Entering main loop...");
}

void loop() {
  // Handle LoRaWAN events (this is essential for Class B operation)
  lora.handleEvents();
  
  // If not joined, try to join periodically
  if (!lora.isNetworkJoined()) {
    static unsigned long lastJoinAttempt = 0;
    if (millis() - lastJoinAttempt > 60000) { // Try every minute
      Serial.println("Not joined to network. Attempting to join...");
      if (lora.joinNetwork()) {
        Serial.println("Successfully joined the network!");
        // Switch to Class B mode after joining
        if (lora.setDeviceClass(DEVICE_CLASS_B)) {
          Serial.println("Switched to Class B mode");
          lora.setPingSlotPeriodicity(0);
        }
      } else {
        Serial.println("Join attempt failed. Will try again later.");
      }
      lastJoinAttempt = millis();
    }
  }
  
  // If we're joined but not in Class B mode, try to switch
  if (lora.isNetworkJoined() && lora.getDeviceClass() != DEVICE_CLASS_B) {
    Serial.println("Switching to Class B mode...");
    if (lora.setDeviceClass(DEVICE_CLASS_B)) {
      Serial.println("Now in Class B mode");
    }
  }
  
  // Print beacon state information periodically
  if (millis() - lastBeaconInfoTime > beaconInfoInterval) {
    uint8_t beaconState = lora.getBeaconState();
    Serial.print("Beacon state: ");
    switch (beaconState) {
      case BEACON_STATE_IDLE:
        Serial.println("IDLE (Not tracking beacons)");
        break;
      case BEACON_STATE_ACQUISITION:
        Serial.println("ACQUISITION (Attempting to lock to beacons)");
        break;
      case BEACON_STATE_LOCKED:
        Serial.println("LOCKED (Successfully synchronized to beacons)");
        break;
      case BEACON_STATE_LOST:
        Serial.println("LOST (Lost synchronization, attempting to reacquire)");
        break;
      default:
        Serial.println("UNKNOWN");
    }
    
    // Print ping slot information if we're in Class B with beacon lock
    if (beaconState == BEACON_STATE_LOCKED) {
      Serial.print("Ping slot periodicity: ");
      Serial.println(lora.getPingSlotPeriodicity());
    }
    
    lastBeaconInfoTime = millis();
  }
  
  // Send uplink data periodically
  static unsigned long lastUplinkTime = 0;
  if (lora.isNetworkJoined() && (millis() - lastUplinkTime > 300000)) { // Every 5 minutes
    Serial.println("Sending uplink data...");
    
    // Example sensor reading (replace with real sensor data as needed)
    uint8_t data[4];
    data[0] = 0x01; // Data type: temperature
    int16_t temperature = random(1500, 3000); // Simulated temperature (15.00 to 30.00°C)
    data[1] = (temperature >> 8) & 0xFF;
    data[2] = temperature & 0xFF;
    data[3] = lora.getBeaconState(); // Include beacon state in uplink
    
    // Send data (port 1 for sensor data, unconfirmed transmission)
    if (lora.sendData(data, sizeof(data), 1, false)) {
      Serial.println("Uplink successful");
    } else {
      Serial.println("Uplink failed");
    }
    
    lastUplinkTime = millis();
  }
  
  // Short delay to prevent CPU hogging
  delay(100);
} 