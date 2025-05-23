#include "HeltecLoRaWan.h"
#include <string.h> // For memcpy
#include <esp_task_wdt.h> // For watchdog timer

// Static member to hold the single instance
HeltecLoRaWan* HeltecLoRaWan::_instance = nullptr;

// Our own implementation of downLinkDataHandle, which will override the weak one in LoRaWan_APP.cpp
// This function is called by the Heltec stack when downlink data is received.
extern "C" void downLinkDataHandle(McpsIndication_t *mcpsIndication) {
    if (HeltecLoRaWan::_instance && HeltecLoRaWan::_instance->_callbacks && mcpsIndication) {
        if (mcpsIndication->BufferSize > 0 || mcpsIndication->Port > 0) {
             HeltecLoRaWan::_instance->_callbacks->onDataReceived(
                mcpsIndication->Buffer,
                mcpsIndication->BufferSize,
                mcpsIndication->Port,
                mcpsIndication->Rssi,
                mcpsIndication->Snr
            );
        } 
    }
}

// We can also override downLinkAckHandle if needed, though it's less common for app-level logic
// extern "C" void downLinkAckHandle() {
//     if (HeltecLoRaWan::_instance && HeltecLoRaWan::_instance->_callbacks) {
//         // This is an ACK for a *confirmed downlink* message, not an uplink.
//         // Might not map directly to ILoRaWanCallbacks, or could be a specific callback.
//     }
// }


HeltecLoRaWan::HeltecLoRaWan() : 
    _callbacks(nullptr), 
    _loraWanClass(CLASS_A),
    _isJoining(false),
    _joinTimeoutMillis(600000), // 10 minutes default join timeout for onJoinFailed
    _joinAttemptStartTime(0),
    _isJoinedFlag(false),
    _waitingForTxConfirm(false) 
{
    _instance = this; 
}

HeltecLoRaWan::~HeltecLoRaWan() {
    if (_instance == this) {
        _instance = nullptr;
    }
}

bool HeltecLoRaWan::init(DeviceClass_t deviceClass, LoRaMacRegion_t region, ILoRaWanCallbacks* callbacks) {
    _callbacks = callbacks;
    _loraWanClass = deviceClass;
    _isJoinedFlag = false;
    _isJoining = false;
    _waitingForTxConfirm = false;

    loraWanClass = _loraWanClass; 
    loraWanRegion = region;    
    deviceState = DEVICE_STATE_INIT; 

    // Initialize the Heltec LoRaWAN stack
    LoRaWAN.init(_loraWanClass, loraWanRegion);

    // For US915, configure sub-band 2 (standard for many networks including TTN)
    // Channels 8-15 + 65 (for 500kHz channel)
    if (region == LORAMAC_REGION_US915) {
        // Configure US915 sub-band 2 (channels 8-15 + 65)
        MibRequestConfirm_t mibReq;
        LoRaMacStatus_t status;

        // First disable all channels
        uint16_t channelMask[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
        
        // Enable channels 8-15 (sub-band 2)
        channelMask[0] = 0xFF00;  // Channels 8-15
        channelMask[4] = 0x0002;  // Channel 65
        
        mibReq.Type = MIB_CHANNELS_MASK;
        mibReq.Param.ChannelsMask = channelMask;
        status = LoRaMacMibSetRequestConfirm(&mibReq);
        if (status != LORAMAC_STATUS_OK) {
            Serial.println("[HeltecLoRaWan] Failed to apply channel mask");
        }
    }

    Serial.println("[HeltecLoRaWan] Initialized. Set EUI/Keys and call join().");
    if (region == LORAMAC_REGION_US915) {
        Serial.println("[HeltecLoRaWan] Configured for US915 sub-band 2 (channels 8-15 + 65)");
    }
    return true;
}

void HeltecLoRaWan::setDevEui(const uint8_t* devEui_ptr) {
    memcpy(devEui, devEui_ptr, 8); 
}

void HeltecLoRaWan::setAppEui(const uint8_t* appEui_ptr) {
    memcpy(appEui, appEui_ptr, 8); 
}

void HeltecLoRaWan::setAppKey(const uint8_t* appKey_ptr) {
    memcpy(appKey, appKey_ptr, 16); 
    lwan_dev_params_update();
}

void HeltecLoRaWan::setNwkKey(const uint8_t* nwkKey_ptr) {
    memcpy(nwkSKey, nwkKey_ptr, 16); 
    lwan_dev_params_update();
}

void HeltecLoRaWan::setActivationType(bool isOtaa) {
    overTheAirActivation = isOtaa; 
}

void HeltecLoRaWan::setAdr(bool adrEnabled) {
    loraWanAdr = adrEnabled; 
}

bool HeltecLoRaWan::join() {
    if (_isJoining || _isJoinedFlag) {
        Serial.println("[HeltecLoRaWan] Join request ignored: Already joining or joined.");
        return false;
    }
    if (getDeviceState() == DEVICE_STATE_SLEEP) {
         Serial.println("[HeltecLoRaWan] Join request ignored: Device is sleeping.");
        return false;
    }

    Serial.println("[HeltecLoRaWan] Requesting Join...");
    Serial.println("joining...");
    
    _isJoining = true;
    _isJoinedFlag = false;
    _joinAttemptStartTime = millis();
    deviceState = DEVICE_STATE_JOIN;

    // Feed watchdog before starting join
    esp_task_wdt_reset();
    
    // Start join process but don't block
    LoRaWAN.join();
    
    return true;
}

bool HeltecLoRaWan::send(const uint8_t* data_ptr, uint8_t len, uint8_t port, bool confirmed) {
    if (!_isJoinedFlag || _isJoining) {
        Serial.println("[HeltecLoRaWan] Send failed: Not joined or still joining.");
        return false;
    }
    if (getDeviceState() == DEVICE_STATE_SLEEP) {
        Serial.println("[HeltecLoRaWan] Send failed: Device sleeping.");
        return false;
    }
    if (_waitingForTxConfirm && isTxConfirmed) {
        Serial.println("[HeltecLoRaWan] Send failed: Waiting for previous confirmed TX to complete.");
        return false;
    }

    if (len > LORAWAN_APP_DATA_MAX_SIZE) {
        Serial.printf("[HeltecLoRaWan] Send error: payload size %d exceeds LORAWAN_APP_DATA_MAX_SIZE %d\n", len, LORAWAN_APP_DATA_MAX_SIZE);
        return false;
    }
    
    appPort = port;
    isTxConfirmed = confirmed;
    appDataSize = len;
    memcpy(appData, data_ptr, len);

    _waitingForTxConfirm = confirmed;
    deviceState = DEVICE_STATE_SEND; 
    Serial.printf("[HeltecLoRaWan] Data queued. Port: %d, Confirmed: %s, Size: %d\n", port, confirmed ? "yes" : "no", len);
    return true;
}

void HeltecLoRaWan::process() {
    eDeviceState_LoraWan currentHeltecState = getDeviceState();
    static eDeviceState_LoraWan previousHeltecStateForJoinCheck = currentHeltecState;

    // Feed watchdog during any active operation
    esp_task_wdt_reset();

    // --- Join Logic --- 
    if (_isJoining) {
        // Check for successful join by monitoring state transitions
        if (currentHeltecState == DEVICE_STATE_SEND || currentHeltecState == DEVICE_STATE_CYCLE) {
            if (previousHeltecStateForJoinCheck == DEVICE_STATE_JOIN) {
                _isJoinedFlag = true;
                _isJoining = false;
                if (_callbacks) _callbacks->onJoined();
                Serial.println("[HeltecLoRaWan] Joined successfully.");
            }
        } else if (millis() - _joinAttemptStartTime > _joinTimeoutMillis) {
            _isJoining = false;
            if (_callbacks) _callbacks->onJoinFailed();
            Serial.println("[HeltecLoRaWan] Join failed (timeout).");
            deviceState = DEVICE_STATE_INIT;
        }
    }
    previousHeltecStateForJoinCheck = currentHeltecState;

    // --- Tx Confirmed Logic ---
    if (_waitingForTxConfirm && isTxConfirmed && currentHeltecState == DEVICE_STATE_CYCLE) {
        _waitingForTxConfirm = false;
        if (_callbacks) _callbacks->onSendConfirmed(true);
        Serial.println("[HeltecLoRaWan] Confirmed TX sequence completed.");
    }

    // --- Drive the Heltec State Machine --- 
    switch (currentHeltecState) {
        case DEVICE_STATE_INIT:
            // Only reinitialize if we're not already joining
            if (!_isJoining) {
                LoRaWAN.init(_loraWanClass, loraWanRegion);
                deviceState = DEVICE_STATE_JOIN;
                _joinAttemptStartTime = millis();
                _isJoining = true;
                Serial.println("[HeltecLoRaWan] Initialized and starting join...");
            }
            break;

        case DEVICE_STATE_JOIN:
            // Let the join process run
            // The state machine will handle this automatically
            break;

        case DEVICE_STATE_SEND:
            // Let the send process run
            // The state machine will handle this automatically
            break;

        case DEVICE_STATE_CYCLE:
            // Normal operation state
            // For Class C, we want to keep the receive window open continuously
            break;

        case DEVICE_STATE_SLEEP:
            // Device is sleeping
            // Don't process anything
            return;

        default:
            Serial.printf("[HeltecLoRaWan] Unknown state: %d\n", currentHeltecState);
            break;
    }

    // Let the Heltec stack do its work
    // For Class C, we want to keep the receive window open continuously
    // For Class A, we'll use a duty cycle of 1000ms (1 second)
    uint32_t dutyCycle = (_loraWanClass == CLASS_C) ? 0 : 1000;
    LoRaWAN.cycle(dutyCycle);
}

void HeltecLoRaWan::sleep() {
    deviceState = DEVICE_STATE_SLEEP;
}

bool HeltecLoRaWan::isJoined() {
    return _isJoinedFlag;
}

DeviceClass_t HeltecLoRaWan::getDeviceClass() {
    return _loraWanClass;
}

int16_t HeltecLoRaWan::getRssi() {
    // Return the last RSSI from the Heltec stack
    return 0; // TODO: Implement this
}

int8_t HeltecLoRaWan::getSnr() {
    // Return the last SNR from the Heltec stack
    return 0; // TODO: Implement this
}

eDeviceState_LoraWan HeltecLoRaWan::getDeviceState() {
    return deviceState;
}

// The static MacEventHandler and instance handleMacEvent would go here
// if we find a way to register them with the underlying LoRaMac stack directly.
// For now, we're relying on modifying Heltec's C code or using the extern "C" wrappers. 