/**
 * Basic SX1262 Radio Test
 * 
 * This example tests the basic functionality of the SX1262 radio
 * on the Heltec WiFi LoRa 32 V3.2 board.
 * 
 * Created: June 2024
 */

#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <Ra01S.h>

// Heltec WiFi LoRa 32 V3.2 OLED pins
#define OLED_SDA 39
#define OLED_SCL 40
#define OLED_RST 38

// SX1262 pins for the Heltec WiFi LoRa 32 V3.2
#define PIN_NSS   8
#define PIN_RESET 12
#define PIN_BUSY  13
#define PIN_DIO1  14
#define PIN_TXEN  -1  // Not used
#define PIN_RXEN  -1  // Not used

// LoRa settings
#define RF_FREQUENCY 915000000  // Hz (915 MHz)
#define TX_OUTPUT_POWER 14      // dBm
#define LORA_SPREADING_FACTOR 7 // (options: 6-12)
#define LORA_CODING_RATE 1      // (1 = 4/5, 2 = 4/6, 3 = 4/7, 4 = 4/8)
#define LORA_PREAMBLE_LENGTH 8  // symbols
#define LORA_PAYLOAD_LENGTH 0   // 0 = variable length

// Initialize the OLED display
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);

// Global variables
SX126x* radio = NULL;
unsigned long lastTransmitTime = 0;
volatile bool packetReceived = false;
char message[64];
int counter = 0;

// DIO1 interrupt handler
void IRAM_ATTR dio1Handler() {
  packetReceived = true;
}

// Simple diagnostic function to print the state
void printState(const char* state) {
  Serial.printf("[STATE] %s\n", state);
}

// Setup function
void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000); // Give serial port time to connect
  Serial.println("\n\nSX1262 Radio Test");
  
  // Initialize the OLED display
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
  
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "SX1262 Radio Test");
  display.drawString(0, 16, "Initializing...");
  display.display();
  
  // Initialize the SX1262 radio
  printState("Creating SX126x instance");
  radio = new SX126x(PIN_NSS, PIN_RESET, PIN_BUSY, PIN_TXEN, PIN_RXEN);
  delay(100);
  
  // Start the radio with a longer delay to ensure proper initialization
  printState("Initializing radio");
  radio->begin(RF_FREQUENCY, TX_OUTPUT_POWER);
  delay(1000);  // Longer delay after initialization
  
  // Setup DIO1 interrupt
  printState("Setting up DIO1 interrupt");
  pinMode(PIN_DIO1, INPUT);
  attachInterrupt(PIN_DIO1, dio1Handler, RISING);
  
  // Configure with a single, basic configuration
  printState("Configuring LoRa parameters");
  
  // Using the actual bandwidth values from Ra01S.h
  radio->LoRaConfig(
    LORA_SPREADING_FACTOR,
    0x07,   // 125 kHz bandwidth for SX1262
    LORA_CODING_RATE,
    LORA_PREAMBLE_LENGTH,
    LORA_PAYLOAD_LENGTH,
    true,  // CRC mode
    false  // InvertIQ
  );
  
  delay(1000);  // Longer delay after configuration
  
  // Try a hardware reset
  printState("Performing hardware reset");
  digitalWrite(PIN_RESET, LOW);
  delay(20);
  digitalWrite(PIN_RESET, HIGH);
  delay(1000);
  
  // Try setting a lower TX power
  printState("Setting TX power to 10 dBm");
  radio->SetTxPower(10);
  delay(100);
  
  // Start receive mode
  printState("Starting receive mode");
  radio->ReceiveMode();
  
  // Update display
  display.clear();
  display.drawString(0, 0, "SX1262 Radio Test");
  display.drawString(0, 16, "Radio configured!");
  display.drawString(0, 32, "SF7/BW125/CR4-5");
  display.drawString(0, 48, "Ready!");
  display.display();
  
  Serial.println("Radio ready!");
  lastTransmitTime = 0;  // Force first transmission immediately
}

void loop() {
  // Check if it's time to send a packet (every 10 seconds)
  if (millis() - lastTransmitTime >= 10000 || lastTransmitTime == 0) {
    // Create a simple message
    counter++;
    snprintf(message, sizeof(message), "Hello #%d from SX1262!", counter);
    
    // Display on OLED
    display.clear();
    display.drawString(0, 0, "SX1262 Radio Test");
    display.drawString(0, 16, "Sending packet:");
    display.drawString(0, 32, message);
    display.display();
    
    // Reset the radio before sending
    printState("Reset before sending");
    digitalWrite(PIN_RESET, LOW);
    delay(20);
    digitalWrite(PIN_RESET, HIGH);
    delay(500);
    
    // Initialize again
    printState("Re-initializing radio");
    radio->begin(RF_FREQUENCY, TX_OUTPUT_POWER);
    delay(500);
    
    // Configure again
    printState("Re-configuring radio");
    radio->LoRaConfig(
      LORA_SPREADING_FACTOR,
      0x07,
      LORA_CODING_RATE,
      LORA_PREAMBLE_LENGTH,
      LORA_PAYLOAD_LENGTH,
      true,
      false
    );
    delay(500);
    
    printState("Sending message");
    Serial.printf("Sending: %s\n", message);
    
    // Send the packet using the Ra01S API
    int result = radio->Send((uint8_t*)message, strlen(message), SX126x_TXMODE_SYNC);
    bool success = (result == 0);
    
    if (success) {
      Serial.println("Packet sent successfully!");
      display.drawString(0, 48, "Packet sent!");
    } else {
      Serial.printf("Failed to send packet! Error: %d\n", result);
      display.drawString(0, 48, "Send failed!");
    }
    display.display();
    
    delay(500);  // Add delay after sending
    
    // Start receive mode again
    printState("Returning to receive mode");
    radio->ReceiveMode();
    
    lastTransmitTime = millis();
  }
  
  // Check if a packet has been received
  if (packetReceived) {
    packetReceived = false;
    
    printState("Packet received interrupt triggered");
    
    // Read the received packet
    uint8_t rxData[64] = {0};
    uint8_t rxLen = 0;
    int8_t rssi = -120;  // Default value
    int8_t snr = -10;    // Default value
    
    rxLen = radio->Receive(rxData, sizeof(rxData));
    
    if (rxLen > 0) {
      // Null-terminate the data
      if (rxLen < sizeof(rxData)) {
        rxData[rxLen] = 0;
      } else {
        rxData[sizeof(rxData) - 1] = 0;
      }
      
      // Print to serial
      Serial.println("Packet received!");
      Serial.printf("Data: %s\n", rxData);
      Serial.printf("RSSI: %d dBm, SNR: %d dB (estimated)\n", rssi, snr);
      
      // Display on OLED
      display.clear();
      display.drawString(0, 0, "Packet Received!");
      display.drawString(0, 16, (char*)rxData);
      display.drawString(0, 32, "RSSI: " + String(rssi) + " dBm (est)");
      display.drawString(0, 48, "Length: " + String(rxLen) + " bytes");
      display.display();
    } else {
      Serial.println("Empty packet received!");
    }
    
    delay(500);  // Add delay after receiving
    
    // Reset the radio after receiving
    printState("Reset after receiving");
    digitalWrite(PIN_RESET, LOW);
    delay(20);
    digitalWrite(PIN_RESET, HIGH);
    delay(500);
    
    // Initialize again
    printState("Re-initializing radio after receive");
    radio->begin(RF_FREQUENCY, TX_OUTPUT_POWER);
    delay(500);
    
    // Configure again
    printState("Re-configuring radio after receive");
    radio->LoRaConfig(
      0x07,
      LORA_CODING_RATE,
      LORA_PREAMBLE_LENGTH,
      LORA_PAYLOAD_LENGTH,
      true,
      false
    );
    delay(500);
    
    // Resume receiving
    printState("Resuming receive mode");
    radio->ReceiveMode();
  }
  
  // Small delay to prevent watchdog issues
  delay(10);
} 