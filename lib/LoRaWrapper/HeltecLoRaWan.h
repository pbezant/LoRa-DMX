#pragma once

#include "ILoRaWanDevice.h"
// We need LoRaWan_APP.h for the specific Heltec LoRaWAN object and its methods.
// This also brings in definitions like DeviceClass_t, LoRaMacRegion_t, etc.,
// which ILoRaWanDevice.h is currently relying on.
#include "LoRaWan_APP.h"

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
    
    // We might not need to store all params if the Heltec LoRaWAN object (from LoRaWan_APP.h)
    // holds them internally after its init or set methods are called.
    // The actual LoRaWAN object from Heltec library will be managed here, 
    // but it's globally accessible as `LoRaWAN` from `LoRaWan_APP.h`
    // so we might not need a member variable for it unless we want to encapsulate it further
    // or if there's a scenario needing multiple instances (unlikely for a single device sketch).

    // Helper to map Heltec device states or manage internal state if needed
    void updateInternalState(); 
    static void onHeltecMacEvent(Mcps_t macMsg, McpsIndication_t *mcpsIndication, McpsReq_t *mcpsReq, MlmeReq_t *mlmeReq, MlmeConfirm_t *mlmeConfirm); // Static callback wrapper
    void handleMacEvent(Mcps_t macMsg, McpsIndication_t *mcpsIndication, McpsReq_t *mcpsReq, MlmeReq_t *mlmeReq, MlmeConfirm_t *mlmeConfirm); // Instance method
    
    // Friend function for callback access
    friend void downLinkDataHandle(McpsIndication_t *mcpsIndication);
}; 