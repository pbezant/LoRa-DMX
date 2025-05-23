#pragma once

#include "ILoRaWanDevice.h"
#include <Arduino.h>
#include <SPI.h>
#include "ESP32_LoRaWan_102.h"
#include "LoRaWan_APP.h"
#include "loramac/LoRaMac.h"
#include "loramac/region/Region.h"

class HeltecLoRaWan : public ILoRaWanDevice {
public:
    HeltecLoRaWan();
    ~HeltecLoRaWan() override;

    // Initialization and Configuration
    bool init(DeviceClass_t deviceClass, LoRaMacRegion_t region, ILoRaWanCallbacks* callbacks) override;
    void setDevEui(const uint8_t* devEui) override;
    void setAppEui(const uint8_t* appEui) override;
    void setAppKey(const uint8_t* appKey) override;
    void setNwkKey(const uint8_t* nwkKey) override;
    void setActivationType(bool isOtaa) override;
    void setAdr(bool adrEnabled) override;

    // LoRaWAN Operations
    bool join() override;
    bool send(const uint8_t* data, uint8_t len, uint8_t port, bool confirmed) override;
    void process() override; 
    void sleep() override;   

    // Status
    bool isJoined() override;
    DeviceClass_t getDeviceClass() override;
    int16_t getRssi() override;
    int8_t getSnr() override;
    eDeviceState_LoraWan getDeviceState() override; 

private:
    ILoRaWanCallbacks* _callbacks; // Store the pointer to the callback handler
    DeviceClass_t _loraWanClass;   // Store configured class
    
    // State tracking
    bool _isJoining;
    bool _isJoinedFlag;
    bool _waitingForTxConfirm;
    unsigned long _joinAttemptStartTime;
    unsigned long _joinTimeoutMillis;
    
    // Static instance for callbacks
    static HeltecLoRaWan* _instance;
    
    // Helper to map Heltec device states or manage internal state if needed
    void updateInternalState(); 
    static void onHeltecMacEvent(Mcps_t macMsg, McpsIndication_t *mcpsIndication, McpsReq_t *mcpsReq, MlmeReq_t *mlmeReq, MlmeConfirm_t *mlmeConfirm); // Static callback wrapper
    void handleMacEvent(Mcps_t macMsg, McpsIndication_t *mcpsIndication, McpsReq_t *mcpsReq, MlmeReq_t *mlmeReq, MlmeConfirm_t *mlmeConfirm); // Instance method
    
    // Friend function for callback access
    friend void downLinkDataHandle(McpsIndication_t *mcpsIndication);
}; 