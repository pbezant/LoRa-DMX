#include "LoRaWANManager.h"
#include <Arduino.h>
#include <SPI.h>
#include <esp_task_wdt.h>

// Initialize static member
LoRaWANManager* LoRaWANManager::_instance = nullptr;

// LMIC callback function (static, will call instance method)
void onLmicEventCallback(void *userData, ev_t ev) {
    // Call the instance method to handle the event
    if (LoRaWANManager::_instance) {
        LoRaWANManager::_instance->onLmicEvent(ev);
    }
}

// LMIC OS callbacks (these are required by MCCI LMIC)
void os_getArtEui(u1_t* buf) {
    // AppEUI/JoinEUI should be in little-endian format
    if (LoRaWANManager::_instance) {
        for (int i = 0; i < 8; i++) {
            buf[i] = LoRaWANManager::_instance->_joinEUI[7-i]; // Reverse byte order
        }
    }
}

void os_getDevEui(u1_t* buf) {
    // DevEUI should be in little-endian format
    if (LoRaWANManager::_instance) {
        for (int i = 0; i < 8; i++) {
            buf[i] = LoRaWANManager::_instance->_devEUI[7-i]; // Reverse byte order
        }
    }
}

void os_getDevKey(u1_t* buf) {
    // AppKey should be in big-endian format (no reversal needed)
    if (LoRaWANManager::_instance) {
        memcpy(buf, LoRaWANManager::_instance->_appKey, 16);
    }
}

// Get the singleton instance
LoRaWANManager& LoRaWANManager::getInstance() {
    if (_instance == nullptr) {
        _instance = new LoRaWANManager();
    }
    return *_instance;
}

// LoRaWANManager implementation
LoRaWANManager::LoRaWANManager()
    : _deviceClass(LORAWAN_CLASS_C), _joined(false),
      _lastRssi(0), _lastSnr(0), _region("US915"), _subBand(2),
      _cs(-1), _dio1(-1), _rst(-1), _busy(-1), _initDone(false),
      _txPending(false), _joinPending(false), _classSetupDone(false),
      _lastEventTime(0) {
    
    // Clear EUIs and keys
    memset(_devEUI, 0, sizeof(_devEUI));
    memset(_joinEUI, 0, sizeof(_joinEUI));
    memset(_appKey, 0, sizeof(_appKey));
    
    // Initialize LMIC pins to defaults
    memset(&_lmic_pins, 0, sizeof(_lmic_pins));
}

LoRaWANManager::~LoRaWANManager() {
    // Deinitialize LMIC if it was initialized
    if (_initDone) {
        LMIC_reset();
        _initDone = false;
    }
    
    // Clear the static instance if it's pointing to this instance
    if (_instance == this) {
        _instance = nullptr;
    }
}

bool LoRaWANManager::_hexStringToByteArray(const char* hexString, uint8_t* output, size_t length) {
    size_t hexLen = strlen(hexString);
    if (hexLen != length * 2) {
        Serial.printf("[LoRaWANManager] Hex string length mismatch: %d != %d*2\n", hexLen, length);
        return false;
    }
    
    for (size_t i = 0; i < length; i++) {
        int value;
        if (sscanf(hexString + i*2, "%2x", &value) != 1) {
            Serial.printf("[LoRaWANManager] Failed to parse hex byte at position %d\n", i);
            return false;
        }
        output[i] = (uint8_t)value;
    }
    
    return true;
}

bool LoRaWANManager::begin(int8_t cs, int8_t dio1, int8_t rst, int8_t busy,
                           const char* devEUIHex, const char* joinEUIHex,
                           const char* appKeyHex,
                           uint8_t deviceClass, const char* region, uint8_t subBand) {
    _cs = cs;
    _dio1 = dio1;
    _rst = rst;
    _busy = busy;
    _deviceClass = deviceClass;
    _region = region;
    _subBand = subBand;
    _joined = false;
    _classSetupDone = false;

    // Convert hex strings to byte arrays
    if (!_hexStringToByteArray(devEUIHex, _devEUI, 8)) {
        Serial.println("[LoRaWANManager] Invalid DevEUI format");
        return false;
    }
    
    if (!_hexStringToByteArray(joinEUIHex, _joinEUI, 8)) {
        Serial.println("[LoRaWANManager] Invalid JoinEUI format");
        return false;
    }
    
    if (!_hexStringToByteArray(appKeyHex, _appKey, 16)) {
        Serial.println("[LoRaWANManager] Invalid AppKey format");
        return false;
    }

    Serial.println("[LoRaWANManager] Initializing LMIC...");
    Serial.printf("[LoRaWANManager] Pins - CS: %d, DIO1: %d, RST: %d, BUSY: %d\n", 
                  _cs, _dio1, _rst, _busy);
    
    // Initialize the SPI for LoRa radio
    Serial.println("[LoRaWANManager] Initializing SPI...");
    SPI.begin();
    
    // Reset the radio before LMIC initialization
    _resetRadio();
    
    // Explicitly pet the watchdog timer before LMIC init
    esp_task_wdt_reset();
    yield();
    
    // Initialize LMIC
    if (!_initLMIC()) {
        _notifyEvent(EV_INIT_FAILED);
        Serial.println("[LoRaWANManager] LMIC initialization failed!");
        return false;
    }
    
    // Yield after LMIC initialization and before channel setup
    esp_task_wdt_reset();
    yield();

    if (!_setupChannels()) {
        _notifyEvent(EV_INIT_FAILED);
        Serial.println("[LoRaWANManager] Failed to set up channels!");
        return false;
    }

    Serial.printf("[LoRaWANManager] Device class requested: %s\n", 
                  _deviceClass == LORAWAN_CLASS_C ? "C" : "A");
    
    _notifyEvent(EV_INIT_SUCCESS);
    return true;
}

void LoRaWANManager::_resetRadio() {
    // Initialize reset pin and perform reset sequence
    if (_rst != -1) {
        pinMode(_rst, OUTPUT);
        // Reset the radio
        Serial.println("[LoRaWANManager] Resetting radio...");
        digitalWrite(_rst, LOW);
        delay(10);
        digitalWrite(_rst, HIGH);
        delay(10);
    }
    
    // Initialize BUSY pin if provided
    if (_busy != -1) {
        pinMode(_busy, INPUT);
    }
}

bool LoRaWANManager::_initLMIC() {
    // Configure LMIC pins
    _lmic_pins.nss = _cs;
    _lmic_pins.rst = _rst;
    
    // For SX126x radios, DIO0 in LMIC = DIO1 on SX1262
    _lmic_pins.dio[0] = _dio1;      // Map DIO1 to DIO0
    _lmic_pins.dio[1] = LMIC_UNUSED_PIN;  // SX126x doesn't use DIO1
    _lmic_pins.dio[2] = LMIC_UNUSED_PIN;  // Not used
    
    // Step 1: Initialize LMIC (with extended error handling)
    Serial.println("[LoRaWANManager] Starting LMIC initialization...");
    Serial.flush(); // Make sure log message is sent before potential crash
    
    // Add delay before os_init to let system stabilize
    delay(10);
    esp_task_wdt_reset();
    
    // Initialize LMIC - this is the potential crash point
    os_init_ex(&_lmic_pins);
    Serial.println("[LoRaWANManager] LMIC os_init_ex completed successfully");
    
    // Short delay to let system stabilize
    delay(10);
    esp_task_wdt_reset();
    yield();
    
    // Step 2: Reset the MAC state
    Serial.println("[LoRaWANManager] Performing LMIC_reset...");
    LMIC_reset();
    Serial.println("[LoRaWANManager] LMIC_reset completed");
    
    // Short delay to let system stabilize
    delay(10);
    esp_task_wdt_reset();
    yield();
    
    // Step 3: Set up the callbacks
    Serial.println("[LoRaWANManager] Registering event callback...");
    LMIC_registerEventCb(onLmicEventCallback, nullptr);
    
    // Step 4: Configure LMIC parameters
    // Use a reasonable clock error for ESP32
    LMIC_setClockError(MAX_CLOCK_ERROR * 5 / 100);  // 5% tolerance
    
    // Set DR for initial join
    LMIC_setDrTxpow(DR_SF7, 14);
    
    // Another watchdog reset
    esp_task_wdt_reset();
    yield();
    
    _initDone = true;
    Serial.println("[LoRaWANManager] LMIC initialization successful");
    return true;
}

bool LoRaWANManager::_setupChannels() {
    if (_region == "US915") {
        // US915 setup - use specific sub-band
        Serial.printf("[LoRaWANManager] Setting up US915 channels, sub-band: %d\n", _subBand);
        
        // Disable all channels first
        for (uint8_t i = 0; i < 72; i++) {
            LMIC_disableChannel(i);
            // Yield every few channels to prevent watchdog trigger
            if (i % 8 == 7) {
                esp_task_wdt_reset();
                yield();
            }
        }
        
        // Enable the 8 channels for the specified sub-band (125kHz channels)
        Serial.println("[LoRaWANManager] Enabling sub-band channels...");
        for (uint8_t i = 0; i < 8; i++) {
            LMIC_enableChannel(_subBand * 8 + i);
        }
        
        // Enable the 500kHz channel for this sub-band
        Serial.println("[LoRaWANManager] Enabling 500kHz channel...");
        LMIC_enableChannel(64 + _subBand);
        
    } else if (_region == "EU868") {
        Serial.println("[LoRaWANManager] Setting up EU868 channels");
        // EU868 uses the default LMIC channel setup, nothing to do
        
    } else if (_region == "AU915") {
        // AU915 setup - similar to US915
        Serial.printf("[LoRaWANManager] Setting up AU915 channels, sub-band: %d\n", _subBand);
        
        // Disable all channels first
        for (uint8_t i = 0; i < 72; i++) {
            LMIC_disableChannel(i);
            // Yield every few channels to prevent watchdog trigger
            if (i % 8 == 7) {
                esp_task_wdt_reset();
                yield();
            }
        }
        
        // Enable the 8 channels for the specified sub-band
        for (uint8_t i = 0; i < 8; i++) {
            LMIC_enableChannel(_subBand * 8 + i);
        }
        
        // Enable the 500kHz channel for this sub-band
        LMIC_enableChannel(64 + _subBand);
        
    } else {
        Serial.printf("[LoRaWANManager] Unknown region: %s\n", _region.c_str());
        return false;
    }
    
    // Final watchdog reset
    esp_task_wdt_reset();
    yield();
    
    Serial.printf("[LoRaWANManager] Channels configured for region: %s\n", _region.c_str());
    return true;
}

bool LoRaWANManager::_setupClass() {
    if (_deviceClass == LORAWAN_CLASS_C && !_classSetupDone && _joined) {
        Serial.println("[LoRaWANManager] Setting up Class C...");
        
        // Disable link check mode
        LMIC_setLinkCheckMode(0);
        
        // Set up Class C continuous reception
        // Disable Class B beacons if supported
        #if defined(LMIC_ENABLE_BeaconTracking) && defined(LMIC_DISABLE_BEACONS)
            LMIC_disableTracking();
        #endif
        
        // Enable Class C continuous RX
        LMIC_setClassC(1);
        
        _classSetupDone = true;
        Serial.println("[LoRaWANManager] Class C setup complete");
        return true;
        
    } else if (_deviceClass == LORAWAN_CLASS_A && !_classSetupDone && _joined) {
        Serial.println("[LoRaWANManager] Setting up Class A...");
        
        // For Class A, nothing special needs to be done
        // But ensure Class C is not active
        #if defined(LMIC_ENABLE_DeviceTimeReq)
            LMIC_setClassC(0);
        #endif
        
        _classSetupDone = true;
        Serial.println("[LoRaWANManager] Class A setup complete");
        return true;
    }
    
    return _classSetupDone;
}

void LoRaWANManager::setDownlinkCallback(DownlinkCallback cb) {
    _downlinkCb = cb;
}

void LoRaWANManager::setEventCallback(EventCallback cb) {
    _eventCb = cb;
}

bool LoRaWANManager::joinNetwork() {
    if (!_initDone) {
        Serial.println("[LoRaWANManager] Cannot join: LMIC not initialized");
        return false;
    }
    
    if (_joined) {
        Serial.println("[LoRaWANManager] Already joined to network");
        return true;
    }
    
    if (_joinPending) {
        Serial.println("[LoRaWANManager] Join already in progress");
        return true;
    }
    
    Serial.println("[LoRaWANManager] Starting OTAA join...");
    _notifyEvent(EV_JOIN_STARTED);
    
    // Make sure we're not doing anything else
    LMIC_reset();
    
    // Reapply the channel settings after reset
    if (!_setupChannels()) {
        Serial.println("[LoRaWANManager] Failed to re-setup channels for join");
        return false;
    }
    
    // Start OTAA join
    LMIC_startJoining();
    _joinPending = true;
    
    return true;
}

bool LoRaWANManager::sendData(const uint8_t* data, size_t len, uint8_t port, bool confirmed) {
    if (!_initDone) {
        Serial.println("[LoRaWANManager] Cannot send: LMIC not initialized");
        return false;
    }
    
    if (!_joined) {
        Serial.println("[LoRaWANManager] Cannot send: not joined to network");
        return false;
    }
    
    if (_txPending) {
        Serial.println("[LoRaWANManager] Cannot send: transmission already in progress");
        return false;
    }
    
    if (len > 242) {
        Serial.println("[LoRaWANManager] Data too large for LoRaWAN (max 242 bytes)");
        return false;
    }
    
    // Check if LMIC is busy
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println("[LoRaWANManager] Cannot send: LMIC busy with TX/RX");
        return false;
    }
    
    Serial.printf("[LoRaWANManager] Sending %d bytes on port %d %s\n", 
                 len, port, confirmed ? "(confirmed)" : "(unconfirmed)");
    
    // Notify transmission started
    _notifyEvent(EV_TX_STARTED);
    
    // Queue the data for transmission
    LMIC_setTxData2(port, (uint8_t*)data, len, confirmed ? 1 : 0);
    _txPending = true;
    
    return true;
}

bool LoRaWANManager::sendString(const String& data, uint8_t port, bool confirmed) {
    return sendData((const uint8_t*)data.c_str(), data.length(), port, confirmed);
}

void LoRaWANManager::handleEvents() {
    // This is called in the main loop to handle LMIC events
    os_runloop_once();
    
    // Set up Class C if needed and if we've joined
    if (_joined && !_classSetupDone) {
        _setupClass();
    }
    
    // Feed watchdog timer
    if (millis() - _lastEventTime > 1000) {
        esp_task_wdt_reset();
        _lastEventTime = millis();
    }
}

float LoRaWANManager::getLastRssi() const {
    return _lastRssi;
}

float LoRaWANManager::getLastSnr() const {
    return _lastSnr;
}

uint8_t LoRaWANManager::getDeviceClass() const {
    return _deviceClass;
}

bool LoRaWANManager::isJoined() const {
    return _joined;
}

bool LoRaWANManager::isTxPending() const {
    return _txPending;
}

void LoRaWANManager::_notifyEvent(uint8_t event) {
    if (_eventCb) {
        _eventCb(event);
    }
}

void LoRaWANManager::onLmicEvent(ev_t ev) {
    Serial.print("[LoRaWANManager] LMIC Event: ");
    
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println("SCAN_TIMEOUT");
            break;
            
        case EV_BEACON_FOUND:
            Serial.println("BEACON_FOUND");
            break;
            
        case EV_BEACON_MISSED:
            Serial.println("BEACON_MISSED");
            break;
            
        case EV_BEACON_TRACKED:
            Serial.println("BEACON_TRACKED");
            break;
            
        case EV_JOINING:
            Serial.println("JOINING");
            _joinPending = true;
            break;
            
        case EV_JOINED:
            Serial.println("JOINED");
            _joined = true;
            _joinPending = false;
            
            // Disable link check validation
            LMIC_setLinkCheckMode(0);
            
            // Schedule setup for requested device class
            _setupClass();
            
            // Notify callback
            _notifyEvent(EV_JOIN_SUCCESS);
            break;
            
        case EV_JOIN_FAILED:
            Serial.println("JOIN_FAILED");
            _joinPending = false;
            _notifyEvent(EV_JOIN_FAILED);
            break;
            
        case EV_REJOIN_FAILED:
            Serial.println("REJOIN_FAILED");
            break;
            
        case EV_TXCOMPLETE:
            Serial.println("TXCOMPLETE");
            _txPending = false;
            
            // Check if we received data
            if (LMIC.dataLen > 0) {
                Serial.printf("Received %d bytes on port %d\n", 
                             LMIC.dataLen, LMIC.frame[LMIC.dataBeg - 1]);
                
                // Process the downlink
                _processDownlink(LMIC.frame + LMIC.dataBeg, LMIC.dataLen, LMIC.frame[LMIC.dataBeg - 1]);
            }
            
            // Update RSSI and SNR
            _lastRssi = LMIC.rssi - 137; // Apply RSSI offset
            _lastSnr = LMIC.snr * 0.25;  // Convert LMIC SNR (4*SNR) to float
            
            // Notify TX complete
            _notifyEvent(EV_TX_COMPLETE);
            break;
            
        case EV_LOST_TSYNC:
            Serial.println("LOST_TSYNC");
            break;
            
        case EV_RESET:
            Serial.println("RESET");
            break;
            
        case EV_RXCOMPLETE:
            // Data received in ping slot
            Serial.println("RXCOMPLETE");
            
            // Check if we received data
            if (LMIC.dataLen > 0) {
                Serial.printf("Received %d bytes on port %d\n", 
                             LMIC.dataLen, LMIC.frame[LMIC.dataBeg - 1]);
                
                // Process the downlink
                _processDownlink(LMIC.frame + LMIC.dataBeg, LMIC.dataLen, LMIC.frame[LMIC.dataBeg - 1]);
                
                // Notify RX received
                _notifyEvent(EV_RX_RECEIVED);
            }
            
            // Update RSSI and SNR
            _lastRssi = LMIC.rssi - 137; // Apply RSSI offset
            _lastSnr = LMIC.snr * 0.25;  // Convert LMIC SNR (4*SNR) to float
            
            break;
            
        case EV_LINK_DEAD:
            Serial.println("LINK_DEAD");
            break;
            
        case EV_LINK_ALIVE:
            Serial.println("LINK_ALIVE");
            break;
            
        case EV_TXSTART:
            Serial.println("TXSTART");
            break;
            
        case EV_TXCANCELED:
            Serial.println("TXCANCELED");
            _txPending = false;
            _notifyEvent(EV_TX_FAILED);
            break;
            
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
            
        case EV_JOIN_TXCOMPLETE:
            Serial.println("JOIN TX COMPLETE");
            break;
            
        default:
            Serial.printf("UNKNOWN (0x%02X)\n", ev);
            break;
    }
}

void LoRaWANManager::_processDownlink(uint8_t* payload, size_t size, uint8_t port) {
    // Call the user callback if set
    if (_downlinkCb) {
        _downlinkCb(payload, size, port);
    }
} 