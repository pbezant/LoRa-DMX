#ifndef DMX_LORAWAN_H
#define DMX_LORAWAN_H

#include <Arduino.h>
// We will likely need to include the Heltec LoRaWAN library header here
// For example: #include "LoRaWan_APP.h" 
// This depends on how we integrate the Heltec library (copying vs. referencing)

// Forward declaration if needed, or include necessary Heltec headers
// enum DeviceClass_t { CLASS_A, CLASS_C }; // Placeholder, actual type from Heltec lib

class DMXLoRaWAN {
public:
    // Constructor
    DMXLoRaWAN();

    // Initialization function
    // region: LoRaWAN region (e.g., LORAMAC_REGION_US915)
    // deviceClass: CLASS_A or CLASS_C
    // otaa: true for OTAA, false for ABP
    bool begin(uint8_t region, uint8_t deviceClass, bool otaa = true);

    // Set OTAA credentials
    void setOTAAKeys(const char* devEui, const char* appEui, const char* appKey);

    // Set ABP credentials
    void setABPKeys(const char* nwkSKey, const char* appSKey, const char* devAddr);
    
    // Function to join the LoRaWAN network
    bool join();

    // Function to send LoRaWAN data
    // port: Application port
    // data: Pointer to the data buffer
    // length: Length of the data
    // confirmed: true for confirmed uplink, false for unconfirmed
    bool sendData(uint8_t port, uint8_t* data, uint8_t length, bool confirmed = true);

    // Function to handle LoRaWAN stack events and processing
    void loop();

    // Check if the device has successfully joined the network
    bool isJoined();

    // Get current device state (e.g., sleeping, sending, joining)
    // You'll need to define what these states mean in your context
    // int getDeviceState(); // Placeholder

private:
    // Internal variables to store credentials
    char _devEui[32]; // Ensure sizes are appropriate
    char _appEui[32];
    char _appKey[64];
    char _nwkSKey[64];
    char _appSKey[64];
    char _devAddr[16];

    bool _otaa;
    uint8_t _deviceClass; // To store CLASS_A or CLASS_C

    // Placeholder for the actual Heltec LoRaWAN object or state
    // For example: LoRaWanClass* _heltecLoRa; (if Heltec provides such a class)
    // Or manage state directly if using their more C-style API

    // Internal state variables
    bool _joined;
    // int _currentDeviceState; // Placeholder for device state
};

#endif // DMX_LORAWAN_H 