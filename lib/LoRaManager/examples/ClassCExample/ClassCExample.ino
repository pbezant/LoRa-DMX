/*
 * LoRaWAN Class C Example
 * 
 * This example demonstrates how to use LoRaManager's Class C functionality
 * to enable devices to receive downlink messages at any time.
 * Class C devices keep their receive window open continuously when not transmitting,
 * which enables low-latency commands from the server but increases power consumption.
 * 
 * This mode is suitable for mains-powered devices or battery-powered devices
 * that need to receive commands with minimal latency.
 * 
 * Note: Your LoRaWAN network server must support Class C operations
 * for this functionality to work properly.
 */

#include <Arduino.h>
#include <LoRaManager.h>

// Define LoRa pins for your board
#define LORA_CS   18
#define LORA_DIO1 23
#define LORA_RST  14
#define LORA_BUSY 33

// LED pins for visualization (change as needed for your board)
#define STATUS_LED 2   // Status LED
#define ACTION_LED 13  // LED for remote control

// LoRaWAN credentials - replace with your own
uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI = 0x0000000000000000;
uint8_t appKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t nwkKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Create LoRaManager instance
LoRaManager lora;

// Command counter to track received commands
unsigned long commandCounter = 0;

// Last received command timestamp
unsigned long lastCommandTime = 0;

// Callback for downlink data
void handleDownlink(uint8_t* payload, size_t size, uint8_t port) {
  // Update command counter and timestamp
  commandCounter++;
  lastCommandTime = millis();
  
  // Blink status LED
  digitalWrite(STATUS_LED, HIGH);
  
  // Log information about the received packet
  Serial.print("\n==== DOWNLINK RECEIVED #");
  Serial.print(commandCounter);
  Serial.println(" ====");
  Serial.print("Port: ");
  Serial.println(port);
  Serial.print("Size: ");
  Serial.print(size);
  Serial.println(" bytes");
  Serial.print("Data: ");
  for (size_t i = 0; i < size; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Process different types of commands
  if (port == 1) {
    // Port 1: Control commands
    if (size > 0) {
      switch (payload[0]) {
        case 0x01: // Turn ON
          digitalWrite(ACTION_LED, HIGH);
          Serial.println("Command: Turn ON");
          break;
          
        case 0x02: // Turn OFF
          digitalWrite(ACTION_LED, LOW);
          Serial.println("Command: Turn OFF");
          break;
          
        case 0x03: // Toggle
          digitalWrite(ACTION_LED, !digitalRead(ACTION_LED));
          Serial.println("Command: Toggle");
          break;
          
        case 0x04: // Blink
          if (size > 1) {
            // Payload[1] contains the number of blinks
            uint8_t blinkCount = payload[1];
            Serial.print("Command: Blink ");
            Serial.print(blinkCount);
            Serial.println(" times");
            
            // Perform the blinking (non-blocking using millis would be better in a real application)
            for (uint8_t i = 0; i < blinkCount; i++) {
              digitalWrite(ACTION_LED, HIGH);
              delay(200);
              digitalWrite(ACTION_LED, LOW);
              delay(200);
            }
          }
          break;
          
        default:
          Serial.println("Unknown command code");
      }
    }
  } else if (port == 2) {
    // Port 2: Configuration commands
    if (size > 0) {
      // Example: Set reporting interval
      // Not implemented in this example, but could be used to change device behavior
      Serial.println("Received configuration command (not implemented in this example)");
    }
  } else if (port == 3) {
    // Port 3: System commands
    if (size > 0) {
      switch (payload[0]) {
        case 0x01: // Restart device
          Serial.println("Command: Restart device");
          Serial.println("Restarting in 3 seconds...");
          delay(3000);
          ESP.restart();
          break;
          
        case 0x02: // Report status
          Serial.println("Command: Report status");
          // Send a status report uplink
          sendStatusReport();
          break;
          
        default:
          Serial.println("Unknown system command");
      }
    }
  }
  
  // Turn off status LED
  delay(50);
  digitalWrite(STATUS_LED, LOW);
}

// Function to send a status report
void sendStatusReport() {
  uint8_t statusData[8];
  
  // Fill with some example status information
  statusData[0] = 0x01;  // Status report type
  statusData[1] = digitalRead(ACTION_LED);  // Current LED state
  
  // Uptime in seconds (32-bit value)
  uint32_t uptime = millis() / 1000;
  statusData[2] = (uptime >> 24) & 0xFF;
  statusData[3] = (uptime >> 16) & 0xFF;
  statusData[4] = (uptime >> 8) & 0xFF;
  statusData[5] = uptime & 0xFF;
  
  // Number of commands received
  statusData[6] = (commandCounter >> 8) & 0xFF;
  statusData[7] = commandCounter & 0xFF;
  
  // Send status report on port 3
  Serial.println("Sending status report...");
  if (lora.sendData(statusData, sizeof(statusData), 3, true)) {
    Serial.println("Status report sent successfully (confirmed)");
  } else {
    Serial.println("Failed to send status report");
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000); // Give time for the serial console to connect
  
  // Initialize LED pins
  pinMode(STATUS_LED, OUTPUT);
  pinMode(ACTION_LED, OUTPUT);
  
  // Initial state
  digitalWrite(STATUS_LED, LOW);
  digitalWrite(ACTION_LED, LOW);
  
  Serial.println("\n==============================================");
  Serial.println("LoRaWAN Class C Example");
  Serial.println("==============================================\n");
  
  // Initialize the LoRa module
  Serial.println("Initializing LoRa module...");
  if (!lora.begin(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY)) {
    Serial.println("Failed to initialize LoRa module!");
    while (1) {
      // Blink status LED to indicate error
      digitalWrite(STATUS_LED, HIGH);
      delay(100);
      digitalWrite(STATUS_LED, LOW);
      delay(100);
    }
  }
  Serial.println("LoRa module initialized successfully");
  
  // Set LoRaWAN credentials
  lora.setCredentials(joinEUI, devEUI, appKey, nwkKey);
  
  // Register downlink callback
  lora.setDownlinkCallback(handleDownlink);
  
  // Join the network
  Serial.println("Joining LoRaWAN network...");
  bool joinSuccess = false;
  for (int attempt = 1; attempt <= 5 && !joinSuccess; attempt++) {
    Serial.print("Join attempt ");
    Serial.print(attempt);
    Serial.print("/5... ");
    
    joinSuccess = lora.joinNetwork();
    
    if (joinSuccess) {
      Serial.println("SUCCESS!");
      
      // Blink LED to indicate successful join
      for (int i = 0; i < 3; i++) {
        digitalWrite(STATUS_LED, HIGH);
        delay(200);
        digitalWrite(STATUS_LED, LOW);
        delay(200);
      }
      
      break;
    } else {
      Serial.println("FAILED");
      
      // Short blink to indicate failure
      digitalWrite(STATUS_LED, HIGH);
      delay(50);
      digitalWrite(STATUS_LED, LOW);
      
      delay(5000); // Wait 5 seconds before trying again
    }
  }
  
  if (!joinSuccess) {
    Serial.println("Failed to join network after multiple attempts.");
    Serial.println("The example will continue and retry joining later.");
  } else {
    Serial.println("Successfully joined the LoRaWAN network");
    
    // Switch to Class C mode for continuous reception
    Serial.println("Switching to Class C mode...");
    if (lora.setDeviceClass(DEVICE_CLASS_C)) {
      Serial.println("Device is now in Class C mode");
      Serial.println("Continuous reception is active");
      
      // Send an initial status report
      sendStatusReport();
    } else {
      Serial.println("Failed to switch to Class C mode");
    }
  }
  
  Serial.println("\nSetup complete. Entering main loop...");
  Serial.println("Device is ready to receive downlink messages at any time.");
}

void loop() {
  // Handle LoRaWAN events (required for Class C continuous reception)
  lora.handleEvents();
  
  // Check if we need to join or re-join the network
  if (!lora.isNetworkJoined()) {
    static unsigned long lastJoinAttempt = 0;
    if (millis() - lastJoinAttempt > 60000) { // Try every minute
      Serial.println("Not joined to network. Attempting to join...");
      if (lora.joinNetwork()) {
        Serial.println("Successfully joined the network!");
        // Switch to Class C mode
        if (lora.setDeviceClass(DEVICE_CLASS_C)) {
          Serial.println("Switched to Class C mode");
        }
      } else {
        Serial.println("Join attempt failed. Will try again later.");
      }
      lastJoinAttempt = millis();
    }
  }
  
  // If joined but not in Class C mode, try to switch
  if (lora.isNetworkJoined() && lora.getDeviceClass() != DEVICE_CLASS_C) {
    Serial.println("Not in Class C mode. Switching...");
    if (lora.setDeviceClass(DEVICE_CLASS_C)) {
      Serial.println("Now in Class C mode");
    } else {
      Serial.println("Failed to switch to Class C mode");
    }
  }
  
  // Send periodic status updates
  static unsigned long lastStatusTime = 0;
  if (lora.isNetworkJoined() && (millis() - lastStatusTime > 900000)) { // Every 15 minutes
    sendStatusReport();
    lastStatusTime = millis();
  }
  
  // Print status message occasionally
  static unsigned long lastStatusMessageTime = 0;
  if (millis() - lastStatusMessageTime > 300000) { // Every 5 minutes
    Serial.println("\n--- Status Update ---");
    Serial.print("Device class: ");
    Serial.println(lora.getDeviceClass());
    Serial.print("Network joined: ");
    Serial.println(lora.isNetworkJoined() ? "Yes" : "No");
    Serial.print("Commands received: ");
    Serial.println(commandCounter);
    
    if (commandCounter > 0) {
      Serial.print("Time since last command: ");
      Serial.print((millis() - lastCommandTime) / 1000);
      Serial.println(" seconds");
    }
    
    Serial.print("Current ACTION_LED state: ");
    Serial.println(digitalRead(ACTION_LED) ? "ON" : "OFF");
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    Serial.println("Ready to receive downlink commands at any time");
    Serial.println("---------------------");
    
    lastStatusMessageTime = millis();
  }
  
  // Short delay to prevent CPU hogging
  delay(100);
} 