/**
 * LoRaWAN Class C Test for Heltec WiFi LoRa 32 V3.2 using nopnop2002's SX1262 driver
 * 
 * This example demonstrates how to use the SX1262Radio and LoRaWANAdapter classes
 * to implement true LoRaWAN Class C operation with the Heltec WiFi LoRa 32 V3.2 board.
 * 
 * Created: May 2024
 */

#include <Arduino.h>
#include <heltec_unofficial.h>  // For display and LED
#include "SX1262Radio.h"
#include "LoRaWANAdapter.h"
#include "secrets.h"  // Contains DEVEUI, APPEUI, APPKEY

// LoRaWAN credentials
#define APPEUI_MSB                      0x00, 0x00, 0x00, 0x00
#define APPEUI_LSB                      0x00, 0x00, 0x00, 0x00
#define DEVEUI_MSB                      0x00, 0x00, 0x00, 0x00
#define DEVEUI_LSB                      0x00, 0x00, 0x00, 0x00
#define APPKEY_MSB                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define APPKEY_LSB                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

// Create instances
SX1262Radio* radio;
LoRaWANAdapter* lorawan;

// Variables for periodic uplink
#define UPLINK_INTERVAL_MS              60000  // 1 minute
unsigned long lastUplinkTime = 0;

// Flag for join status
bool joined = false;

// Helper functions to convert EUI and key strings to bytes
uint64_t eui_string_to_uint64(const char* eui_str) {
    uint64_t eui = 0;
    for (int i = 0; i < 16; i += 2) {
        char byte_str[3] = {eui_str[i], eui_str[i + 1], 0};
        uint8_t byte = strtoul(byte_str, nullptr, 16);
        eui |= static_cast<uint64_t>(byte) << (8 * (7 - (i / 2)));
    }
    return eui;
}

void appkey_string_to_bytes(const char* appkey_str, uint8_t* appkey_bytes) {
    for (int i = 0; i < 32; i += 2) {
        char byte_str[3] = {appkey_str[i], appkey_str[i + 1], 0};
        appkey_bytes[i / 2] = strtoul(byte_str, nullptr, 16);
    }
}

// Downlink callback
void process_downlink(uint8_t* buffer, uint8_t length, int16_t rssi, float snr) {
    Serial.printf("Downlink received: %d bytes, RSSI: %d dBm, SNR: %.1f dB\n", length, rssi, snr);
    
    // Print data as hex
    Serial.print("Data: ");
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();
    
    // Try to interpret as ASCII
    Serial.print("ASCII: ");
    for (int i = 0; i < length; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            Serial.print((char)buffer[i]);
        } else {
            Serial.print('.');
        }
    }
    Serial.println();
    
    // Update display
    display.clear();
    display.drawString(0, 0, "Downlink Received!");
    display.drawString(0, 10, String(length) + " bytes");
    display.drawString(0, 20, "RSSI: " + String(rssi) + " dBm");
    display.drawString(0, 30, "SNR: " + String(snr) + " dB");
    
    // Display first 8 bytes as hex
    String dataStr = "";
    for (int i = 0; i < min(8, length); i++) {
        char hex[4];
        sprintf(hex, "%02X ", buffer[i]);
        dataStr += hex;
    }
    display.drawString(0, 40, dataStr);
    
    display.display();
    
    // Flash LED
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
    // Initialize serial
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=== LoRaWAN Class C Test ===\n");
    
    // Initialize Heltec board
    Heltec.begin(true /*DisplayEnable*/, false /*LoRaEnable*/, true /*SerialEnable*/);
    
    // Set LED pin
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    // Display startup message
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "LoRaWAN Class C Test");
    display.drawString(0, 10, "Initializing...");
    display.display();
    
    // Create radio instance
    radio = new SX1262Radio();
    if (!radio) {
        Serial.println("Failed to create radio instance!");
        display.drawString(0, 20, "Radio init failed!");
        display.display();
        while (true) { delay(1000); }
    }
    
    // Initialize radio
    Serial.println("Initializing SX1262 radio...");
    if (!radio->begin()) {
        Serial.println("Failed to initialize radio!");
        display.drawString(0, 20, "Radio init failed!");
        display.display();
        while (true) { delay(1000); }
    }
    
    Serial.println("Radio initialized successfully!");
    display.drawString(0, 20, "Radio OK!");
    display.display();
    
    // Print supported bandwidths
    Serial.println("Supported bandwidths:");
    for (int i = 0; i < radio->getNumSupportedBandwidths(); i++) {
        Serial.printf("  %.2f kHz\n", radio->getSupportedBandwidth(i));
    }
    
    // Create LoRaWAN instance
    lorawan = new LoRaWANAdapter();
    if (!lorawan) {
        Serial.println("Failed to create LoRaWAN instance!");
        display.drawString(0, 30, "LoRaWAN init failed!");
        display.display();
        while (true) { delay(1000); }
    }
    
    // Initialize LoRaWAN
    Serial.println("Initializing LoRaWAN...");
    if (!lorawan->begin(radio)) {
        Serial.println("Failed to initialize LoRaWAN!");
        display.drawString(0, 30, "LoRaWAN init failed!");
        display.display();
        while (true) { delay(1000); }
    }
    
    Serial.println("LoRaWAN initialized successfully!");
    display.drawString(0, 30, "LoRaWAN OK!");
    display.display();
    
    // Set downlink callback
    lorawan->setRxCallback(process_downlink);
    
    // Get LoRaWAN credentials from secrets.h
    uint64_t joinEUI = eui_string_to_uint64(APPEUI);
    uint64_t devEUI = eui_string_to_uint64(DEVEUI);
    uint8_t nwkKey[16];
    uint8_t appKey[16];
    appkey_string_to_bytes(APPKEY, nwkKey);  // In LoRaWAN 1.1, both are derived from the same key
    appkey_string_to_bytes(APPKEY, appKey);
    
    // Print credentials
    Serial.println("LoRaWAN credentials:");
    Serial.printf("  JoinEUI: %016llX\n", joinEUI);
    Serial.printf("  DevEUI: %016llX\n", devEUI);
    Serial.print("  AppKey: ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02X", appKey[i]);
    }
    Serial.println();
    
    // Join network
    display.drawString(0, 40, "Joining network...");
    display.display();
    
    Serial.println("Joining LoRaWAN network...");
    if (!lorawan->joinOTAA(devEUI, joinEUI, nwkKey, appKey)) {
        Serial.println("Failed to join network!");
        display.drawString(0, 50, "Join failed!");
        display.display();
        return;
    }
    
    Serial.println("Successfully joined network!");
    joined = true;
    
    // Enable Class C
    Serial.println("Enabling Class C mode...");
    if (!lorawan->enableClassC()) {
        Serial.println("Failed to enable Class C mode!");
        display.drawString(0, 50, "Class C failed!");
        display.display();
        return;
    }
    
    Serial.println("Class C mode enabled!");
    
    // Update display
    display.clear();
    display.drawString(0, 0, "LoRaWAN Class C Test");
    display.drawString(0, 10, "Joined network!");
    display.drawString(0, 20, "Class C enabled");
    display.drawString(0, 30, "DevAddr: " + String(lorawan->getDevAddr(), HEX));
    display.drawString(0, 40, "Waiting for downlink...");
    display.display();
    
    // Send initial uplink
    uint8_t data[] = "Hello from Heltec!";
    lorawan->send(data, sizeof(data) - 1, 1, false);
    
    // Update last uplink time
    lastUplinkTime = millis();
}

void loop() {
    // Process LoRaWAN events
    if (lorawan) {
        lorawan->loop();
    }
    
    // Send periodic uplink if joined
    if (joined && (millis() - lastUplinkTime >= UPLINK_INTERVAL_MS)) {
        // Prepare uplink data (status message)
        char uplinkMsg[32];
        sprintf(uplinkMsg, "Uplink #%lu", lastUplinkTime / UPLINK_INTERVAL_MS);
        
        // Send uplink
        Serial.printf("Sending uplink: %s\n", uplinkMsg);
        if (lorawan->send((uint8_t*)uplinkMsg, strlen(uplinkMsg), 1, false)) {
            Serial.println("Uplink sent successfully!");
        } else {
            Serial.println("Failed to send uplink!");
        }
        
        // Update last uplink time
        lastUplinkTime = millis();
    }
    
    // Small delay to prevent CPU hogging
    delay(10);
} 