// Heltec_LoRaWAN_Wrapper.h
// This header file defines the LoRaWAN wrapper class for Heltec WiFi LoRa 32 devices.

#ifndef HELTEC_LORAWAN_WRAPPER_H
#define HELTEC_LORAWAN_WRAPPER_H

#include "Arduino.h"
#include "Heltec.h"      // Heltec board utilities (Heltec.begin, Heltec.display)
#include "secrets.h"     // For DEVEUI, APPEUI, APPKEY macros
#include "LoRaWan_APP.h" // For LoRaWAN stack specifics (LoRaWAN object, types, enums, extern vars)

// Preprocessor check for Heltec_Screen -- REMOVING THIS BLOCK
// #ifndef Heltec_Screen
//    #error "Heltec_Screen is NOT defined. Display will not be available."
// #endif

// Define the LoRaWAN class. For this wrapper, we are focusing on Class C.
#ifndef LORAWAN_CLASS_C // Allow override from platformio.ini if needed, though LoRaWan_APP.h might also define it
    #define LORAWAN_CLASS_C CLASS_C // Assuming CLASS_C is defined in LoRaWan_APP.h or its includes
#endif

// Default LoRaWAN parameters (can be overridden by constructor arguments)
#ifndef LORAWAN_ADR_ENABLED
    #define LORAWAN_ADR_ENABLED true
#endif
#ifndef LORAWAN_CONFIRMED_MSG_ENABLED
    #define LORAWAN_CONFIRMED_MSG_ENABLED false
#endif
#ifndef LORAWAN_APP_PORT_DEFAULT
    #define LORAWAN_APP_PORT_DEFAULT 2
#endif
#ifndef LORAWAN_TX_DUTYCYCLE_DEFAULT
    #define LORAWAN_TX_DUTYCYCLE_DEFAULT 10000 // ms
#endif
#ifndef LORAWAN_MAX_RETRANSMISSIONS_DEFAULT
    #define LORAWAN_MAX_RETRANSMISSIONS_DEFAULT 3
#endif

// Forward declare global C-style callbacks before the class definition
void LoRaWAN_Event_Callback(DeviceClass_t deviceClass, eDeviceState_LoraWan event);
void OnRxData(McpsIndication_t *mcpsIndication);

// Forward declaration for the wrapper class to allow gInstance to be defined first.
class HeltecLoRaWANWrapper;
static HeltecLoRaWANWrapper* gInstance = nullptr;

// Define callback function types for user sketch
typedef void (*DownlinkCallback)(uint8_t *payload, uint16_t size, uint8_t port);
typedef void (*ConnectionStatusCallback)(bool isConnected);

class HeltecLoRaWANWrapper {
public:
    HeltecLoRaWANWrapper(LoRaMacRegion_t region = LORAMAC_REGION_US915, // Use LORAMAC_REGION_US915
                         bool otaa = true,
                         bool adrEnabled = LORAWAN_ADR_ENABLED,
                         bool confirmedMsg = LORAWAN_CONFIRMED_MSG_ENABLED,
                         uint8_t appPortVal = LORAWAN_APP_PORT_DEFAULT,
                         uint32_t txDutyCycleVal = LORAWAN_TX_DUTYCYCLE_DEFAULT,
                         uint8_t maxRetransmissionsVal = LORAWAN_MAX_RETRANSMISSIONS_DEFAULT)
        : _configRegion(region),
          _configOtaa(otaa),
          _configAdrEnabled(adrEnabled),
          _configConfirmedMsg(confirmedMsg),
          _configAppPort(appPortVal),
          _configTxDutyCycle(txDutyCycleVal),
          _configMaxRetransmissions(maxRetransmissionsVal),
          _downlinkCallback(nullptr),
          _connectionStatusCallback(nullptr),
          _isJoined(false) {
        
        gInstance = this; // Set the global instance pointer for C-style callbacks
    }

    void begin() {
        // 1. Initialize Heltec Board Hardware
        long bandValue = 915E6; // Default for US915
        if (_configRegion == LORAMAC_REGION_EU868) bandValue = 868E6; // Use LORAMAC_REGION_EU868
        else if (_configRegion == LORAMAC_REGION_AS923) bandValue = 923E6; // Use LORAMAC_REGION_AS923
        // Add other region-to-band mappings as necessary

        Heltec.begin(true /*DisplayEnable*/, true /*LoRaEnable (generic)*/, 
                       true /*SerialEnable*/, true /*PABOOST*/, bandValue);
        Serial.println(F("Heltec.begin() called."));

        // 2. Populate global LoRaWAN parameters (extern variables from LoRaWan_APP.h)
        ::loraWanRegion = _configRegion;
        ::overTheAirActivation = _configOtaa;
        ::loraWanAdr = _configAdrEnabled;
        ::isTxConfirmed = _configConfirmedMsg;
        ::appPort = _configAppPort;
        ::confirmedNbTrials = _configMaxRetransmissions;
        ::appTxDutyCycle = _configTxDutyCycle; 
        ::loraWanClass = LORAWAN_CLASS_C; // Defined at the top of this file

        if (::overTheAirActivation) {
            hexStringToByteArray(DEVEUI, ::devEui, 8); // DEVEUI from secrets.h, global devEui from LoRaWan_APP.h
            hexStringToByteArray(APPEUI, ::appEui, 8); // APPEUI from secrets.h, global appEui from LoRaWan_APP.h
            hexStringToByteArray(APPKEY, ::appKey, 16); // APPKEY from secrets.h, global appKey from LoRaWan_APP.h
            Serial.println(F("OTAA credentials populated into global LoRaWAN variables."));
        } else {
            // For ABP, user must ensure global devAddr, nwkSKey, appSKey are set externally
            // (e.g. via direct assignment in sketch before calling wrapper.begin() or by extending this wrapper)
            Serial.println(F("ABP mode configured. Ensure devAddr, nwkSKey, appSKey globals are set."));
        }

        // 3. Initialize LoRaWAN Stack (from LoRaWan_APP.h)
        LoRaWAN.init(::loraWanClass, ::loraWanRegion);
        Serial.print(F("LoRaWAN.init() called with Class: ")); Serial.print(::loraWanClass);
        Serial.print(F(", Region: ")); Serial.println(::loraWanRegion);
        
        // LoRaWAN.ifskipjoin(); // Consider if this is needed/useful
        LoRaWAN.join();
        Serial.println(F("LoRaWAN.join() initiated."));
    }

    void loop() {
        LoRaWAN.cycle(::appTxDutyCycle); // Changed from loop() to cycle() and pass duty cycle
    }

    bool sendUplink(uint8_t *payload, uint16_t size) {
        if (size > LORAWAN_APP_DATA_MAX_SIZE) { // LORAWAN_APP_DATA_MAX_SIZE from LoRaWan_APP.h
            Serial.println(F("Error: Payload too large for LoRaWAN buffer!"));
            return false;
        }
        memcpy(::appData, payload, size); // appData is global extern from LoRaWan_APP.h
        ::appDataSize = size;             // appDataSize is global extern

        LoRaWAN.send(); // This queues the data; actual send happens in LoRaWAN.loop()
        Serial.print(F("Uplink message queued (")); Serial.print(size); Serial.println(F(" bytes)."));
        return true; // Indicates successfully queued, not necessarily sent & acked.
    }

    void onDownlink(DownlinkCallback callback) {
        _downlinkCallback = callback;
    }

    void onConnectionStatusChange(ConnectionStatusCallback callback) {
        _connectionStatusCallback = callback;
    }

    bool isJoined() const {
        return _isJoined;
    }

// Make global callbacks friends so they can access private members like _downlinkCallback
friend void ::LoRaWAN_Event_Callback(DeviceClass_t deviceClass, eDeviceState_LoraWan event);
friend void ::OnRxData(McpsIndication_t *mcpsIndication);

private:
    // Configuration storage (set by constructor, used in begin())
    LoRaMacRegion_t _configRegion;
    bool _configOtaa;
    bool _configAdrEnabled;
    bool _configConfirmedMsg;
    uint8_t _configAppPort;
    uint32_t _configTxDutyCycle;
    uint8_t _configMaxRetransmissions;

    // User Callbacks
    DownlinkCallback _downlinkCallback;
    ConnectionStatusCallback _connectionStatusCallback;
    bool _isJoined; // Internal flag for join status, managed by LoRaWAN_Event_Callback

    // Helper function to convert a hex string (from secrets.h) to a byte array (for LoRaWAN globals)
    void hexStringToByteArray(const char* hexString, uint8_t* byteArray, int expectedByteLength) {
        int len = strlen(hexString);
        if (len != expectedByteLength * 2) {
            Serial.print(F("Error: Hex string '")); Serial.print(hexString);
            Serial.print(F("' has incorrect length. Expected ")); Serial.print(expectedByteLength * 2);
            Serial.print(F(" chars, got ")); Serial.println(len);
            memset(byteArray, 0, expectedByteLength); // Zero out array on error
            return;
        }

        for (int i = 0; i < expectedByteLength; i++) {
            char byteChars[3] = {hexString[i * 2], hexString[i * 2 + 1], '\0'};
            byteArray[i] = strtol(byteChars, nullptr, 16);
        }
    }
};

// Global C-style callback functions required by LoRaWan_APP.h
// These will call methods on the gInstance of our wrapper.

void LoRaWAN_Event_Callback(DeviceClass_t deviceClass, eDeviceState_LoraWan event) {
    if (gInstance == nullptr) return;

    // Serial.print(F("LoRaWAN Event received: ")); Serial.println(event);

    switch (event) {
        case DEVICE_STATE_INIT:
            Serial.println(F("[Wrapper] LoRaWAN Event: DEVICE_STATE_INIT"));
            gInstance->_isJoined = false;
            if (gInstance->_connectionStatusCallback) {
                gInstance->_connectionStatusCallback(false);
            }
            break;
        case DEVICE_STATE_JOIN:
            Serial.println(F("[Wrapper] LoRaWAN Event: DEVICE_STATE_JOIN (Attempting Join / Join process active)"));
            // This event might signify the start of join attempts or that the join process is ongoing.
            // Actual "joined" status is often confirmed by a successful send or a specific "joined" event if available.
            // For now, we don't change _isJoined here; wait for send success or different event.
            // If using TTN, console will show join request and accept.
            break;
        case DEVICE_STATE_SEND:
            Serial.println(F("[Wrapper] LoRaWAN Event: DEVICE_STATE_SEND (Data send operation)"));
            // If a send operation is triggered, it implies we are (or believe we are) joined.
            if (!gInstance->_isJoined) {
                 gInstance->_isJoined = true; 
                 Serial.println(F("[Wrapper] Status: JOINED (inferred from send event)."));
                 if (gInstance->_connectionStatusCallback) {
                     gInstance->_connectionStatusCallback(true);
                 }
            }
            break;
        case DEVICE_STATE_CYCLE:
            Serial.println(F("[Wrapper] LoRaWAN Event: DEVICE_STATE_CYCLE (Duty cycle wait or ready for next op)"));
            // This state often means the previous operation (like send or join) completed
            // and the stack is now idle or waiting for duty cycle.
            // If we reach here after a join attempt and aren't marked joined, it might imply success.
            // However, LoRaWan_APP.h also has `LoRaWAN.getStatus()` - could be checked here.
            // For now, if we are in CYCLE and not marked joined, but OTAA was attempted, let's assume joined.
            if (!gInstance->_isJoined && ::overTheAirActivation) {
                // This is an assumption: entering CYCLE after join attempt implies success.
                // More robust would be to check Radio.GetStatus() or similar if the underlying LoRaMac provides it.
                // Or, the first successful TX_CONFIRMED event (if using confirmed packets).
                // For unconfirmed, first RX_DONE on a Class C device after join is also a good sign.
                gInstance->_isJoined = true;
                Serial.println(F("[Wrapper] Status: JOINED (inferred from cycle event post-join attempt)."));
                if (gInstance->_connectionStatusCallback) {
                    gInstance->_connectionStatusCallback(true);
                }
            }
            break;
        case DEVICE_STATE_SLEEP:
            Serial.println(F("[Wrapper] LoRaWAN Event: DEVICE_STATE_SLEEP"));
            // For Class C, this state might not be typical unless explicitly invoked.
            // If it occurs, it might mean the LoRa module is powered down or inactive.
            gInstance->_isJoined = false; // Assume disconnected if stack reports sleep
            if (gInstance->_connectionStatusCallback) {
                gInstance->_connectionStatusCallback(false);
            }
            break;
        default:
            Serial.print(F("[Wrapper] LoRaWAN Event: Unknown eDeviceState_LoraWan - "));
            Serial.println(event);
            break;
    }
}

void OnRxData(McpsIndication_t *mcpsIndication) {
    if (gInstance == nullptr || mcpsIndication == nullptr) return;

    Serial.print(F("[Wrapper] Downlink received. Port: ")); Serial.print(mcpsIndication->Port);
    Serial.print(F(", Size: ")); Serial.print(mcpsIndication->BufferSize);
    Serial.print(F(", RSSI: ")); Serial.print(mcpsIndication->Rssi);
    Serial.print(F(", SNR: ")); Serial.println(mcpsIndication->Snr);
    
    if (mcpsIndication->BufferSize > 0 && mcpsIndication->Buffer != NULL) {
        Serial.print(F("  Payload (HEX): "));
        for (int i = 0; i < mcpsIndication->BufferSize; i++) {
            if (mcpsIndication->Buffer[i] < 0x10) Serial.print("0");
            Serial.print(mcpsIndication->Buffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        if (gInstance->_downlinkCallback != nullptr) {
            gInstance->_downlinkCallback(mcpsIndication->Buffer, mcpsIndication->BufferSize, mcpsIndication->Port);
        }
    } else {
        Serial.println(F("[Wrapper] Downlink: No payload or NULL buffer."));
        if (gInstance->_downlinkCallback != nullptr) {
            // Still call callback, önemlidir, as port might be relevant or it's an empty ack.
            gInstance->_downlinkCallback(nullptr, 0, mcpsIndication->Port);
        }
    }
}

#endif // HELTEC_LORAWAN_WRAPPER_H
