/**
 * LoRaWANAdapter.h - Adapter to connect SX1262Radio with RadioLib's LoRaWAN stack
 * 
 * This file provides an adapter that allows using our SX1262Radio class
 * with RadioLib's LoRaWAN stack for true Class C operation.
 * 
 * Created: May 2024
 */

#ifndef LORAWANADAPTER_H
#define LORAWANADAPTER_H

#include <Arduino.h>
#include <RadioLib.h>
#include "SX1262Radio.h"

// Callback function types
typedef void (*lorawan_rx_callback_t)(uint8_t* buffer, uint8_t length, int16_t rssi, float snr);

class LoRaWANAdapter {
private:
    // Hardware interface
    SX1262Radio* radio;
    
    // RadioLib LoRaWAN node
    RadioLib::LoRaWANNode* node;
    
    // LoRaWAN band
    RadioLib::LoRaWANBand* band;
    
    // Credentials
    uint64_t devEUI;
    uint64_t joinEUI;
    uint8_t nwkKey[16];
    uint8_t appKey[16];
    
    // Callback
    lorawan_rx_callback_t rxCallback;
    
    // Class C
    bool classC_enabled;
    float rx2_frequency;
    uint8_t rx2_sf;
    float rx2_bw;
    
    // DIO1 callback
    static void dio1_callback();
    
public:
    // Constructor
    LoRaWANAdapter();
    
    // Initialization
    bool begin(SX1262Radio* radio);
    
    // LoRaWAN functions
    bool joinOTAA(uint64_t devEUI, uint64_t joinEUI, uint8_t* nwkKey, uint8_t* appKey);
    bool send(uint8_t* data, size_t len, uint8_t fport = 1, bool confirmed = false);
    
    // Class C functions
    bool enableClassC();
    bool disableClassC();
    
    // Event handling
    void setRxCallback(lorawan_rx_callback_t callback);
    void processDownlink();
    
    // Loop function to handle events
    void loop();
    
    // Status functions
    bool isJoined() const;
    uint32_t getDevAddr() const;
    bool isClassCEnabled() const;
};

#endif // LORAWANADAPTER_H 