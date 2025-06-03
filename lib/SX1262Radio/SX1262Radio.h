/**
 * SX1262Radio.h - Integration layer between nopnop2002's SX1262 driver and RadioLib's LoRaWAN stack
 * 
 * This file provides a compatibility layer that allows using nopnop2002's SX1262 driver
 * with RadioLib's LoRaWAN stack for true Class C operation.
 * 
 * Created: May 2024
 */

#ifndef SX1262RADIO_H
#define SX1262RADIO_H

#include <Arduino.h>
#include <Ra01S.h>
#include <RadioLib.h>

// Pin definitions for Heltec WiFi LoRa 32 V3.2
#define NSS_PIN       GPIO_NUM_8
#define RESET_PIN     GPIO_NUM_12
#define BUSY_PIN      GPIO_NUM_13
#define DIO1_PIN      GPIO_NUM_14

// Callback function types
typedef void (*rx_done_callback_t)(void);

class SX1262Radio {
private:
    // nopnop2002 driver instance
    SX126x* radio;
    
    // RadioLib compatibility
    float freq;
    float bw;
    uint8_t sf;
    uint8_t cr;
    uint8_t syncWord;
    uint16_t preambleLength;
    bool crcEnabled;
    
    // Class C configuration
    float rx2_frequency;
    uint8_t rx2_sf;
    float rx2_bw;

    // Interrupt handling
    bool interruptEnabled;
    rx_done_callback_t rxDoneCallback;
    
    // Supported bandwidths
    float supportedBandwidths[10];
    int numSupportedBandwidths;
    
    // Private methods
    bool testBandwidth(float bandwidth);
    void detectSupportedBandwidths();

public:
    // Constructor
    SX1262Radio();
    
    // Initialization
    bool begin();
    
    // Configuration methods
    int16_t setFrequency(float freq);
    int16_t setBandwidth(float bw);
    int16_t setSpreadingFactor(uint8_t sf);
    int16_t setCodingRate(uint8_t cr);
    int16_t setSyncWord(uint8_t syncWord);
    int16_t setPreambleLength(uint16_t preambleLength);
    int16_t setCRC(bool enable);
    
    // RadioLib compatibility methods for LoRaWAN
    int16_t transmit(uint8_t* data, size_t len);
    int16_t receive(uint8_t* data, size_t len);
    int16_t startReceive();
    int16_t standby();
    int16_t sleep();
    
    // Class C methods
    bool enableClassC(float frequency, uint8_t sf, float bw);
    bool disableClassC();
    
    // Interrupt handling
    void setDio1Action(rx_done_callback_t callback);
    bool isReceived();
    int16_t readData(uint8_t* data, size_t len);
    
    // Status methods
    float getRSSI();
    float getSNR();
    bool isChannelFree(float rssiThreshold = -90.0);
    
    // Bandwidth management
    int getNumSupportedBandwidths();
    float getSupportedBandwidth(int index);
    bool isBandwidthSupported(float bw);
};

// DIO1 interrupt handler
void IRAM_ATTR dio1InterruptHandler();

#endif // SX1262RADIO_H 