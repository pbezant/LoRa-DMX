#include "HeltecLoRaWan.h"
#include <string.h> // For memcpy

// Static member to hold the single instance
HeltecLoRaWan* HeltecLoRaWan::_instance = nullptr;

// Our own implementation of downLinkDataHandle, which will override the weak one in LoRaWan_APP.cpp
// This function is called by the Heltec stack when downlink data is received.
extern "C" void downLinkDataHandle(McpsIndication_t *mcpsIndication) {
    if (HeltecLoRaWan::_instance && HeltecLoRaWan::_instance->_callbacks && mcpsIndication) {
        // Check if the downlink is an ACK for a confirmed message
        // This is a bit of a heuristic. A true ACK for a confirmed uplink would typically be an empty payload on port 0.
        // However, mcpsIndication is for application payloads. Ack is handled in McpsConfirm.
        // For now, we assume any data on an app port is app data.
        if (mcpsIndication->BufferSize > 0 || mcpsIndication->Port > 0) {
             HeltecLoRaWan::_instance->_callbacks->onDataReceived(
                mcpsIndication->Buffer,
                mcpsIndication->BufferSize,
                mcpsIndication->Port,
                mcpsIndication->Rssi,
                mcpsIndication->Snr
            );
        } 
        // else it might be a MAC command or an empty ACK on port 0, which onDataReceived might not be interested in.
        // The LoRaMac stack itself processes MAC commands. We could add onMacCommand callback if needed.
    }
    // The original weak downLinkDataHandle in Heltec LoRaWan_APP.cpp prints to Serial.
    // We can replicate that if desired, or leave it clean.
    // printf("+REV DATA:%s,RXSIZE %d,PORT %d\r\n",mcpsIndication->RxSlot?"RXWIN2":"RXWIN1",mcpsIndication->BufferSize,mcpsIndication->Port);
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

    Serial.println("[HeltecLoRaWan] Initialized. Set EUI/Keys and call join().");
    return true;
}

void HeltecLoRaWan::setDevEui(const uint8_t* devEui_ptr) {
    memcpy(devEui, devEui_ptr, 8); 
}

void HeltecLoRaWan::setAppEui(const uint8_t* appEui_ptr) {
    memcpy(appEui, appEui_ptr, 8); 
}

void HeltecLoRaWan::setAppKey(const uint8_t* appKey) {
    // Implementation: Store or pass to Heltec LoRaWAN object
    memcpy(::appKey, appKey, 16); // Example: copying to global Heltec appKey
    lwan_dev_params_update();
}

void HeltecLoRaWan::setNwkKey(const uint8_t* nwkKey) {
    // Implementation: Store or pass to Heltec LoRaWAN object (e.g., nwkSKey for Heltec)
    // For LoRaWAN 1.0.x, NwkKey is often the same as AppKey and Heltec might only use appKey for OTAA.
    // For LoRaWAN 1.1.x, this is distinct. Heltec's library uses nwkSKey and appSKey for session keys.
    // This might need to map to nwkSKey if that's what Heltec's LoRaWAN object uses after join, 
    // or be ignored if Heltec handles it via AppKey for 1.0.x OTAA.
    // For now, let's assume it might map to the global nwkSKey for direct use, similar to appKey.
    memcpy(::nwkSKey, nwkKey, 16); // Example: copying to global Heltec nwkSKey
    lwan_dev_params_update();
}

void HeltecLoRaWan::setActivationType(bool isOtaa) {
    // Implementation
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
    _isJoining = true;
    _isJoinedFlag = false;
    _joinAttemptStartTime = millis();
    deviceState = DEVICE_STATE_JOIN; // Set Heltec state to trigger join in its process loop
    // process(); // Optionally kick process, or rely on main loop calling it
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
    // Check if ready for next TX based on Heltec's `nextTx` flag and if not already waiting for confirm
    // `nextTx` is static RTC_DATA_ATTR bool nextTx in LoRaWan_APP.cpp - unfortunately not directly accessible without modifying code.
    // We have to rely on deviceState or infer from McpsConfirm behavior via process().
    if (_waitingForTxConfirm && isTxConfirmed) { // isTxConfirmed is the global from Heltec
        Serial.println("[HeltecLoRaWan] Send failed: Waiting for previous confirmed TX to complete.");
        return false;
    }

    if (len > LORAWAN_APP_DATA_MAX_SIZE) {
        Serial.printf("[HeltecLoRaWan] Send error: payload size %d exceeds LORAWAN_APP_DATA_MAX_SIZE %d\n", len, LORAWAN_APP_DATA_MAX_SIZE);
        return false;
    }
    
    appPort = port;
    isTxConfirmed = confirmed; // Set Heltec global
    appDataSize = len;
    memcpy(appData, data_ptr, len); // Set Heltec global

    _waitingForTxConfirm = confirmed;
    deviceState = DEVICE_STATE_SEND; 
    // process(); // Optionally kick process
    Serial.printf("[HeltecLoRaWan] Data queued. Port: %d, Confirmed: %s, Size: %d\n", port, confirmed ? "yes" : "no", len);
    return true;
}

void HeltecLoRaWan::process() {
    eDeviceState_LoraWan currentHeltecState = getDeviceState(); // Heltec's global state
    static eDeviceState_LoraWan previousHeltecStateForJoinCheck = currentHeltecState;

    // --- Join Logic --- 
    if (_isJoining) {
        if (currentHeltecState == DEVICE_STATE_SEND && previousHeltecStateForJoinCheck == DEVICE_STATE_JOIN) {
            // Successful join inferred: Heltec stack moved from JOIN to SEND state.
            _isJoinedFlag = true;
            _isJoining = false;
            if (_callbacks) _callbacks->onJoined();
            Serial.println("[HeltecLoRaWan] Joined successfully (inferred).");
        } else if (millis() - _joinAttemptStartTime > _joinTimeoutMillis) {
            _isJoining = false;
            if (_callbacks) _callbacks->onJoinFailed();
            Serial.println("[HeltecLoRaWan] Join failed (timeout).");
            // Optionally reset to init state for another attempt later
            // deviceState = DEVICE_STATE_INIT; 
        }
    }
    previousHeltecStateForJoinCheck = currentHeltecState;

    // --- Tx Confirmed Logic --- (Heuristic)
    // `nextTx` is a static global in LoRaWan_APP.cpp. McpsConfirm sets it to true.
    // If we were waiting for a confirm, and the state is now CYCLE (implying send process completed)
    // and if we assume nextTx would have been set true by McpsConfirm, we can *infer* completion.
    // This is weak. A better way would be to observe some flag McpsConfirm sets if AckReceived is true.
    static bool lastNextTxState = true; // Requires access to Heltec's nextTx or similar flag. Placeholder.
                                        // For now, we can only act on unconfirmed or assume confirmed worked if no error.

    if (_waitingForTxConfirm && isTxConfirmed && currentHeltecState == DEVICE_STATE_CYCLE) {
        // If Heltec stack has finished sending (now in CYCLE state)
        // We previously assumed that if it's confirmed, an ACK makes it transition smoothly.
        // The problem is knowing if ACK was *actually* received.
        // For now, if it reaches CYCLE after a confirmed send, we'll assume it completed.
        // This doesn't differentiate between ACK received or max retries hit.
        // This is a simplification.
        _waitingForTxConfirm = false;
        if (_callbacks) _callbacks->onSendConfirmed(true); // Cannot determine actual ACK status easily here.
        Serial.println("[HeltecLoRaWan] Confirmed TX sequence completed (ACK status not definitively known without hooks).");
    }

    // --- Drive the Heltec State Machine --- 
    // This is essential for the Heltec library to do its work.
    switch (currentHeltecState) {
        case DEVICE_STATE_INIT:
            LoRaWAN.init(_loraWanClass, loraWanRegion);
            // deviceState is a global, LoRaWAN.init might change it or expect it to be changed by subsequent calls.
            // The original example sets deviceState = DEVICE_STATE_JOIN right after.
            if (!_isJoining) { // If we are not already in a join process initiated by our join() method
                 // This path means Heltec stack somehow reset itself to INIT.
                 // We should probably re-initiate our join logic if we want to auto-rejoin.
                 // For now, let it transition as Heltec's original sketch does.
                 deviceState = DEVICE_STATE_JOIN; 
                 _joinAttemptStartTime = millis(); // Restart join timer if Heltec resets to INIT
                 _isJoining = true; // Mark that we are now in a joining process driven by Heltec's auto-transition
            }
            break;
        case DEVICE_STATE_JOIN:
            if (_isJoining) { // Only call LoRaWAN.join if our wrapper initiated the join
                LoRaWAN.join();
            } else {
                // If Heltec is in JOIN state but our wrapper didn't ask to join (e.g. after init auto-transition)
                // we should probably align our state.
                _isJoining = true;
                _isJoinedFlag = false;
                _joinAttemptStartTime = millis();
                LoRaWAN.join();
            }
            break;
        case DEVICE_STATE_SEND:
            // Our send() method has already populated appData, appPort, etc.
            // and set isTxConfirmed.
            LoRaWAN.send(); 
            // Heltec stack automatically transitions to DEVICE_STATE_CYCLE after calling LoRaWAN.send()
            // For unconfirmed, fire callback immediately as transmission is initiated.
            if (!isTxConfirmed && !_waitingForTxConfirm) { // ensure _waitingForTxConfirm is also false for new unconf send
                if (_callbacks) _callbacks->onSendConfirmed(true);
                 Serial.println("[HeltecLoRaWan] Unconfirmed TX initiated.");
            } else if (isTxConfirmed) {
                _waitingForTxConfirm = true; // Mark that we are now waiting for this confirmed send.
            }
            // The Heltec stack itself changes deviceState to DEVICE_STATE_CYCLE after LoRaWAN.send()
            break;
        case DEVICE_STATE_CYCLE:
            // Idle state. Waiting for next send command or downlink.
            // If _waitingForTxConfirm was true, the logic at the start of process() handles it.
            break; 
        case DEVICE_STATE_SLEEP:
            // Device is sleeping. process() shouldn't do much here unless waking it up.
            break;
        default:
            deviceState = DEVICE_STATE_INIT; // Default to re-init if unknown state
            break;
    }
}

void HeltecLoRaWan::sleep() {
    if (_isJoining) {
        Serial.println("[HeltecLoRaWan] Sleep request ignored: Currently trying to join.");
        return;
    }
    // TODO: Check if waiting for TX confirm? Usually, sleep aborts ongoing ops.
    Serial.println("[HeltecLoRaWan] Entering sleep mode...");
    LoRaWAN.sleep(_loraWanClass); 
    // After LoRaWAN.sleep(), Heltec's deviceState might change to DEVICE_STATE_SLEEP or similar.
    // Our process() loop will then see it in SLEEP state.
}

bool HeltecLoRaWan::isJoined() {
    return _isJoinedFlag;
}

DeviceClass_t HeltecLoRaWan::getDeviceClass() {
    // For now, it returns the stored _loraWanClass. Ensure _loraWanClass is set correctly in init.
    return _loraWanClass;
}

int16_t HeltecLoRaWan::getRssi() {
    // TODO: Implementation needed - Get RSSI from Heltec LoRaWAN object/radio
    return 0; // Placeholder
}

int8_t HeltecLoRaWan::getSnr() {
    // TODO: Implementation needed - Get SNR from Heltec LoRaWAN object/radio
    return 0; // Placeholder
}

eDeviceState_LoraWan HeltecLoRaWan::getDeviceState() {
    // Return our interpretation or the direct Heltec state.
    // For now, returning Heltec's state as it drives the process() switch.
    return deviceState; 
}

// The static MacEventHandler and instance handleMacEvent would go here
// if we find a way to register them with the underlying LoRaMac stack directly.
// For now, we're relying on modifying Heltec's C code or using the extern "C" wrappers. 