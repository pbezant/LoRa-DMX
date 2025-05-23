#pragma once

#include <stdint.h>
// Assuming LoRaWan_APP.h will be in the include path via the Heltec_ESP32 library
// This path might need adjustment based on your PlatformIO include paths or if you restructure Heltec_ESP32 lib
#include "LoRaWan_APP.h" // Provides DeviceClass_t, LoRaMacRegion_t, etc.

// Forward declaration for callback handler
class ILoRaWanCallbacks;

class ILoRaWanDevice {
public:
    virtual ~ILoRaWanDevice() = default;

    // Initialization and Configuration
    virtual bool init(DeviceClass_t deviceClass, LoRaMacRegion_t region, ILoRaWanCallbacks* callbacks) = 0;
    virtual void setDevEui(const uint8_t* devEui) = 0;
    virtual void setAppEui(const uint8_t* appEui) = 0;
    virtual void setAppKey(const uint8_t* appKey) = 0;
    virtual void setNwkKey(const uint8_t* nwkKey) = 0; // For LoRaWAN 1.1.x, may be same as AppKey for 1.0.x
    virtual void setActivationType(bool isOtaa) = 0;
    virtual void setAdr(bool adrEnabled) = 0;
    // Removed: setRegion, setClass as they are part of init now

    // LoRaWAN Operations
    virtual bool join() = 0;
    virtual bool send(const uint8_t* data, uint8_t len, uint8_t port, bool confirmed) = 0;
    virtual void process() = 0; // Handles LoRaWAN stack processing, callbacks, etc.
    virtual void sleep() = 0;   // Placeholder for low-power mode

    // Status & Information
    virtual bool isJoined() = 0;
    virtual DeviceClass_t getDeviceClass() = 0;
    virtual int16_t getRssi() = 0;
    virtual int8_t getSnr() = 0;
    virtual eDeviceState_LoraWan getDeviceState() = 0; // Using Heltec's eDeviceState_LoraWan for now

    // Callback registration (already part of init)
    // virtual void registerCallbacks(ILoRaWanCallbacks* callbacks) = 0;

    // Optional: Direct access to underlying radio for non-LoRaWAN operations or advanced control
    // virtual RadioLibHal* getRadio() = 0; // Example if using RadioLib directly
};

// Interface for LoRaWAN Callbacks
class ILoRaWanCallbacks {
public:
    virtual ~ILoRaWanCallbacks() {}
    virtual void onJoined() = 0;
    virtual void onJoinFailed() = 0;
    virtual void onDataReceived(const uint8_t* data, uint8_t len, uint8_t port, int16_t rssi, int8_t snr) = 0;
    virtual void onSendConfirmed(bool success) = 0;
    virtual void onMacCommand(uint8_t cmd, uint8_t* payload, uint8_t len) = 0; // For MAC commands if needed
}; 