#ifndef MCCI_LMIC_WRAPPER_H
#define MCCI_LMIC_WRAPPER_H

#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>

// Forward declaration
class McciLmicWrapper;

// Define the type for the receive callback function
// It will receive a pointer to the wrapper instance, port, data, and length
typedef void (*lorawan_rx_callback_t)(McciLmicWrapper* pUserData, uint8_t port, const uint8_t *data, int len);

// Define the type for the joined callback function
typedef void (*lorawan_joined_callback_t)(McciLmicWrapper* pUserData);

class McciLmicWrapper {
public:
    McciLmicWrapper();

    // Initialization
    bool begin();
    void loop();

    // OTAA Join
    bool joinOTAA(const char* appEui, const char* devEui, const char* appKey);
    bool joinOTAA(uint8_t* appEui, uint8_t* devEui, uint8_t* appKey);

    // Send data
    bool sendData(uint8_t port, const uint8_t* data, uint8_t len, bool confirmed = false);

    // Status
    bool isJoined();
    bool isTxReady();

    // Class C control
    void enableClassC();
    void disableClassC(); 

    // Callback for received messages
    void onReceive(lorawan_rx_callback_t callback);
    
    // Callback for when OTAA join is successful
    void onJoined(lorawan_joined_callback_t callback);

    // To be called by the static LMIC event callback
    void handleEvent(ev_t ev);

    // Static members need to be public for global C functions to access them
    static McciLmicWrapper* instance;
    static uint8_t s_appEui[8];
    static uint8_t s_devEui[8];
    static uint8_t s_appKey[16];
    static bool s_otaaCredsSet;

private:
    // These are not static methods but rather names of C functions LMIC expects.
    // Their implementations will use the static `instance` pointer.
    // static void os_getArtEui (u1_t* buf); // Remove static
    // static void os_getDevEui (u1_t* buf); // Remove static
    // static void os_getDevKey (u1_t* buf); // Remove static
    // The actual C functions will be defined in the .cpp file.

    lorawan_rx_callback_t rx_callback;
    lorawan_joined_callback_t joined_callback; // Add joined_callback member
    bool classCEnabled = false;
};

#endif // MCCI_LMIC_WRAPPER_H 