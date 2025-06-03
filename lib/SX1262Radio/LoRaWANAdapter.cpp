/**
 * LoRaWANAdapter.cpp - Adapter to connect SX1262Radio with RadioLib's LoRaWAN stack
 * 
 * This file provides an adapter that allows using our SX1262Radio class
 * with RadioLib's LoRaWAN stack for true Class C operation.
 * 
 * Created: May 2024
 */

#include "LoRaWANAdapter.h"

// Global instance for callback handling
LoRaWANAdapter* activeAdapter = nullptr;

// Static callback function
void LoRaWANAdapter::dio1_callback() {
    if (activeAdapter) {
        activeAdapter->processDownlink();
    }
}

// Constructor
LoRaWANAdapter::LoRaWANAdapter() {
    radio = nullptr;
    node = nullptr;
    band = nullptr;
    rxCallback = nullptr;
    classC_enabled = false;
    
    // Register this instance for callback handling
    activeAdapter = this;
}

// Initialize LoRaWAN adapter
bool LoRaWANAdapter::begin(SX1262Radio* radio_ptr) {
    if (!radio_ptr) {
        Serial.println("Radio pointer is null!");
        return false;
    }
    
    radio = radio_ptr;
    
    // Create US915 band
    band = new RadioLib::LoRaWANBand_US915();
    if (!band) {
        Serial.println("Failed to create LoRaWAN band!");
        return false;
    }
    
    // Create LoRaWAN node
    node = new RadioLib::LoRaWANNode(radio, band);
    if (!node) {
        Serial.println("Failed to create LoRaWAN node!");
        delete band;
        band = nullptr;
        return false;
    }
    
    // Configure DIO1 action
    radio->setDio1Action(dio1_callback);
    
    // Set default RX2 parameters for US915
    rx2_frequency = 923.3; // MHz
    rx2_sf = 12;
    rx2_bw = 500.0; // kHz
    
    return true;
}

// Join network using OTAA
bool LoRaWANAdapter::joinOTAA(uint64_t devEUI_val, uint64_t joinEUI_val, uint8_t* nwkKey_val, uint8_t* appKey_val) {
    if (!node) {
        Serial.println("LoRaWAN node not initialized!");
        return false;
    }
    
    // Store credentials
    devEUI = devEUI_val;
    joinEUI = joinEUI_val;
    memcpy(nwkKey, nwkKey_val, 16);
    memcpy(appKey, appKey_val, 16);
    
    // Join network
    Serial.print("Joining LoRaWAN network using OTAA... ");
    int state = node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    
    if (state != ERR_NONE) {
        Serial.printf("Failed, code %d\n", state);
        return false;
    }
    
    // Check if we have a valid device address
    uint32_t deviceAddr = getDevAddr();
    if (deviceAddr == 0) {
        Serial.println("Failed, got zero device address!");
        return false;
    }
    
    Serial.printf("Success! Device address: %08X\n", deviceAddr);
    
    return true;
}

// Send data to the network
bool LoRaWANAdapter::send(uint8_t* data, size_t len, uint8_t fport, bool confirmed) {
    if (!node) {
        Serial.println("LoRaWAN node not initialized!");
        return false;
    }
    
    if (!isJoined()) {
        Serial.println("Not joined to a network!");
        return false;
    }
    
    // Temporarily disable Class C during transmission
    bool wasClassC = classC_enabled;
    if (wasClassC) {
        disableClassC();
    }
    
    // Send data
    Serial.printf("Sending %d bytes on port %d... ", len, fport);
    int state = node->sendReceive(data, len, fport, nullptr, 0, 0, confirmed);
    
    if (state != ERR_NONE) {
        Serial.printf("Failed, code %d\n", state);
        
        // Re-enable Class C if it was enabled
        if (wasClassC) {
            enableClassC();
        }
        
        return false;
    }
    
    Serial.println("Success!");
    
    // Re-enable Class C if it was enabled
    if (wasClassC) {
        enableClassC();
    }
    
    return true;
}

// Enable Class C continuous reception
bool LoRaWANAdapter::enableClassC() {
    if (!node) {
        Serial.println("LoRaWAN node not initialized!");
        return false;
    }
    
    if (!isJoined()) {
        Serial.println("Not joined to a network!");
        return false;
    }
    
    Serial.printf("Enabling Class C on RX2 (%0.1f MHz, SF%d, %0.1f kHz)... ", 
                  rx2_frequency, rx2_sf, rx2_bw);
    
    // Configure radio for continuous reception on RX2 parameters
    bool result = radio->enableClassC(rx2_frequency, rx2_sf, rx2_bw);
    
    if (!result) {
        Serial.println("Failed!");
        return false;
    }
    
    Serial.println("Success!");
    classC_enabled = true;
    
    return true;
}

// Disable Class C continuous reception
bool LoRaWANAdapter::disableClassC() {
    if (!node || !radio) {
        return false;
    }
    
    // Disable continuous reception
    bool result = radio->disableClassC();
    
    if (!result) {
        return false;
    }
    
    classC_enabled = false;
    
    return true;
}

// Set callback for received data
void LoRaWANAdapter::setRxCallback(lorawan_rx_callback_t callback) {
    rxCallback = callback;
}

// Process a received downlink
void LoRaWANAdapter::processDownlink() {
    if (!node || !radio) {
        return;
    }
    
    if (!radio->isReceived()) {
        return;
    }
    
    // Read data from radio
    uint8_t data[256];
    int16_t dataLen = radio->readData(data, 256);
    
    if (dataLen <= 0) {
        Serial.printf("Failed to read data from radio, code %d\n", dataLen);
        
        // Re-enable Class C reception
        if (classC_enabled) {
            radio->enableClassC(rx2_frequency, rx2_sf, rx2_bw);
        }
        
        return;
    }
    
    // Process the packet with RadioLib LoRaWAN stack
    int16_t state = node->processDownlink(data, dataLen);
    
    if (state != ERR_NONE) {
        Serial.printf("Failed to process downlink, code %d\n", state);
        
        // Re-enable Class C reception
        if (classC_enabled) {
            radio->enableClassC(rx2_frequency, rx2_sf, rx2_bw);
        }
        
        return;
    }
    
    // Get downlink data
    uint8_t payload[256];
    size_t payloadLen = 0;
    uint8_t port = 0;
    
    state = node->readData(payload, payloadLen, &port);
    
    if (state == ERR_NONE && payloadLen > 0 && rxCallback) {
        float rssi = radio->getRSSI();
        float snr = radio->getSNR();
        
        Serial.printf("Received %d bytes on port %d (RSSI: %.1f dBm, SNR: %.1f dB)\n",
                     payloadLen, port, rssi, snr);
        
        // Call user callback
        rxCallback(payload, payloadLen, rssi, snr);
    }
    
    // Re-enable Class C reception
    if (classC_enabled) {
        radio->enableClassC(rx2_frequency, rx2_sf, rx2_bw);
    }
}

// Main loop function
void LoRaWANAdapter::loop() {
    if (!node) {
        return;
    }
    
    // Check for received packets
    if (radio && radio->isReceived()) {
        processDownlink();
    }
}

// Check if device is joined to a network
bool LoRaWANAdapter::isJoined() const {
    if (!node) {
        return false;
    }
    
    return (getDevAddr() != 0);
}

// Get device address
uint32_t LoRaWANAdapter::getDevAddr() const {
    if (!node) {
        return 0;
    }
    
    return node->getDevAddr();
}

// Check if Class C is enabled
bool LoRaWANAdapter::isClassCEnabled() const {
    return classC_enabled;
} 