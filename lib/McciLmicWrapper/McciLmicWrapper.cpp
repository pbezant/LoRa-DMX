#include "McciLmicWrapper.h"
#include <cstring> // For memcpy
#include <SPI.h>   // For SPI.begin()
#include "arduino_lmic_hal_configuration.h" // Added for HalPinmap_t

// Required for lmic_hal_init_ex, radio_init, LMIC_init, LMIC_reset
// These are C functions from the LMIC library. 
// lmic.h should bring them in (it includes oslmic.h and hal.h)
// No, lmic.h only has os_init. We need to be more specific or ensure arduino_lmic.h is enough.
// arduino_lmic.h includes lmic.h. lmic.h includes hal/hal.h (for lmic_hal_pin_rxtx etc.)
// and oslmic.h (for os_init, radio_init, LMIC_init etc.)
// Let's assume lmic.h (pulled in by McciLmicWrapper.h) is sufficient for now.

// Make the global lmic_pins structure from main.cpp visible here
// It's defined in src/main.cpp
extern const lmic_pinmap lmic_pins; // This is used by LMIC_queryHALPinmap_Helvetic() if lmic_hal_init_ex is called with NULL

// Initialize static members
McciLmicWrapper* McciLmicWrapper::instance = nullptr;
// Static EUI/Key storage for LMIC callbacks
uint8_t McciLmicWrapper::s_appEui[8];
uint8_t McciLmicWrapper::s_devEui[8];
uint8_t McciLmicWrapper::s_appKey[16];
bool McciLmicWrapper::s_otaaCredsSet = false;

// Helper function to convert hex string to byte array
static void hexStringToByteArray(const char* hexString, uint8_t* byteArray, int byteLen) {
    for (int i = 0; i < byteLen; i++) {
        char byteChars[3] = {hexString[i*2], hexString[i*2+1], '\0'};
        byteArray[i] = strtol(byteChars, nullptr, 16);
    }
}

// LMIC HAL C-style callbacks
// These are C functions, not class methods. LMIC calls these.
extern "C" {
    void os_getArtEui (u1_t* buf) { 
        if (McciLmicWrapper::s_otaaCredsSet) {
            memcpy(buf, McciLmicWrapper::s_appEui, 8);
        } else {
            // Optional: Fill with a default or error value if not set
            memset(buf, 0, 8);
        }
    }
    void os_getDevEui (u1_t* buf) {
        if (McciLmicWrapper::s_otaaCredsSet) {
            memcpy(buf, McciLmicWrapper::s_devEui, 8);
        } else {
            // memset(buf, 0, 8);
        }
    }
    void os_getDevKey (u1_t* buf) {
        if (McciLmicWrapper::s_otaaCredsSet) {
            memcpy(buf, McciLmicWrapper::s_appKey, 16);
        } else {
            // memset(buf, 0, 16);
        }
    }

    // Global event handler for LMIC
    // This function is registered with LMIC (implicitly by being named onEvent)
    // and calls the instance's handler.
    void onEvent (ev_t ev) {
        if (McciLmicWrapper::instance != nullptr) {
            McciLmicWrapper::instance->handleEvent(ev);
        }
    }
} // extern "C"

// Static storage for the custom HAL configuration
// static HalConfigurationAndPinmap_t customHalConfig; // Old incorrect type
static Arduino_LMIC::HalPinmap_t customHalPinmap; // Correct type for pin mapping

McciLmicWrapper::McciLmicWrapper() : rx_callback(nullptr), joined_callback(nullptr), classCEnabled(false) {
    if (instance == nullptr) {
        instance = this;
    }
    // else: Consider logging an error if an instance already exists, as this is a singleton pattern.

    // Prepare the custom HAL pinmap configuration
    // Copy from the global lmic_pins.
    // The global lmic_pins is of type const lmic_pinmap, which is an alias for Arduino_LMIC::HalPinmap_t.
    memcpy(&customHalPinmap, &lmic_pins, sizeof(Arduino_LMIC::HalPinmap_t));
    customHalPinmap.pConfig = nullptr; // Use default for other HAL settings (HalConfiguration_t)

    // Old initialization:
    // customHalConfig.pPinmap = &lmic_pins; // Point to our globally defined lmic_pins
    // customHalConfig.pConfiguration = nullptr; // Use default for other HAL settings
}

bool McciLmicWrapper::begin() {
    Serial.println(F("McciLmicWrapper::begin() entered."));
    Serial.println(F("McciLmicWrapper::begin() - Verifying extern lmic_pins (used by default HAL query):"));
    Serial.print(F("  Address of extern lmic_pins: 0x")); Serial.println((uintptr_t)&lmic_pins, HEX);
    Serial.print(F("  lmic_pins.nss: ")); Serial.println(lmic_pins.nss);
    Serial.print(F("  lmic_pins.rst: ")); Serial.println(lmic_pins.rst);
    Serial.print(F("  lmic_pins.dio[0]: ")); Serial.println(lmic_pins.dio[0]);
    Serial.print(F("  lmic_pins.dio[1]: ")); Serial.println(lmic_pins.dio[1]);
    Serial.print(F("  lmic_pins.dio[2]: ")); Serial.println(lmic_pins.dio[2]);
    Serial.print(F("  Value of LMIC_UNUSED_PIN: ")); Serial.println(LMIC_UNUSED_PIN);    

    Serial.println(F("McciLmicWrapper: Explicitly initializing SPI..."));
    SPI.begin(); // Initialize SPI with default pins
    delay(100); // Short delay after SPI init

    Serial.println(F("McciLmicWrapper: Initializing LMIC stack directly..."));
    Serial.println(F("  Calling lmic_hal_init_ex(&customHalPinmap)..."));
    lmic_hal_init_ex(&customHalPinmap); // Initialize HAL with our custom pinmap config
    Serial.println(F("  lmic_hal_init_ex(&customHalPinmap) RETURNED."));
    
    Serial.println(F("  Calling radio_init()..."));
    if (!radio_init()) {
        Serial.println(F("  radio_init() FAILED!"));
        return false; // Critical failure
    }
    Serial.println(F("  radio_init() OK."));

    Serial.println(F("  Calling LMIC_init()..."));
    LMIC_init(); // Initialize LMIC MAC state
    Serial.println(F("  LMIC_init() OK."));

    Serial.println(F("  Calling LMIC_reset()..."));
    LMIC_reset(); // Reset the MAC state
    LMIC_setAdrMode(0); // Try disabling ADR initially
    LMIC_setLinkCheckMode(0); // Disable link checks initially
    Serial.println(F("  LMIC_reset() OK. ADR and Link Check Disabled for initial join."));

    Serial.println(F("McciLmicWrapper::begin() - SUCCESSFULLY INITIALIZED LMIC STACK."));
    return true;
}

void McciLmicWrapper::loop() {
    os_runloop_once();
}

bool McciLmicWrapper::joinOTAA(const char* appEuiStr, const char* devEuiStr, const char* appKeyStr) {
    if (!appEuiStr || !devEuiStr || !appKeyStr) return false;
    hexStringToByteArray(appEuiStr, s_appEui, 8);
    hexStringToByteArray(devEuiStr, s_devEui, 8);
    hexStringToByteArray(appKeyStr, s_appKey, 16);
    s_otaaCredsSet = true;
    
    LMIC_startJoining(); // Start the OTAA join procedure
    return true;
}

bool McciLmicWrapper::joinOTAA(uint8_t* appEui, uint8_t* devEui, uint8_t* appKey) {
    if (!appEui || !devEui || !appKey) return false;
    memcpy(s_appEui, appEui, 8);
    memcpy(s_devEui, devEui, 8);
    memcpy(s_appKey, appKey, 16);
    s_otaaCredsSet = true;

    LMIC_startJoining(); // Start the OTAA join procedure
    return true;
}

bool McciLmicWrapper::sendData(uint8_t port, const uint8_t* data, uint8_t len, bool confirmed) {
    if (isTxReady()) {
        LMIC_setTxData2(port, (uint8_t*)data, len, confirmed ? 1 : 0);
        return true;
    }
    return false;
}

bool McciLmicWrapper::isJoined() {
    // Check if the JOINED flag is set in opmode. This is a common way for MCCI LMIC.
    // Alternatively, LMIC.devaddr != 0 can be used if OP_JOINED causes issues.
    return LMIC.devaddr != 0; // Check if a device address has been assigned
}

bool McciLmicWrapper::isTxReady() {
    // Check if LMIC is not busy with a pending TX/RX operation or already has data queued.
    return (LMIC.opmode & (OP_TXRXPEND | OP_TXDATA)) == 0;
}

void McciLmicWrapper::enableClassC() {
    classCEnabled = true;
    if (isJoined()) {
        LMIC_setLinkCheckMode(0); // Disable link checks (ADR) for Class C
        Serial.println(F("McciLmicWrapper: Class C enabled (link check mode disabled)."));
    } else {
        Serial.println(F("McciLmicWrapper: Device not joined. Class C will be fully enabled upon joining."));
    }
}

void McciLmicWrapper::disableClassC() {
    classCEnabled = false;
    LMIC_setLinkCheckMode(1); // Re-enable link checks (ADR)
    Serial.println(F("McciLmicWrapper: Class C disabled (link check mode enabled)."));
}

void McciLmicWrapper::onReceive(lorawan_rx_callback_t callback) {
    rx_callback = callback;
}

void McciLmicWrapper::onJoined(lorawan_joined_callback_t callback) {
    joined_callback = callback;
}

void McciLmicWrapper::handleEvent(ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            // LMIC_setAdrMode(0); // Disable ADR for stability, especially useful for testing Class C
            // Re-enable ADR after successful join if desired for normal operation
            LMIC_setAdrMode(1); // Enable ADR after join
            LMIC_setLinkCheckMode(1); // Enable link checks

            if (classCEnabled) {
                LMIC_setLinkCheckMode(0); // Ensure link checks are off for Class C
                Serial.println(F("McciLmicWrapper: Class C fully enabled post-join."));
            } else {
                LMIC_setLinkCheckMode(1); // If not Class C, normal link check mode (ADR on)
            }
            if (joined_callback) {
                joined_callback(this); // Pass instance pointer as pUserData
            }
            break;
        case EV_RFU1: // Should not occur in normal operation
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            // Consider re-scheduling join attempt here
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE: // Transmission complete, including RX windows for confirmed uplinks
            Serial.println(F("EV_TXCOMPLETE"));
            if (LMIC.txrxFlags & TXRX_ACK) {
              Serial.println(F("Received ack"));
            }
            if (LMIC.dataLen) { // If data was received
              Serial.print(F("Received "));
              Serial.print(LMIC.dataLen);
              Serial.print(F(" bytes of payload on port: "));
              Serial.println(LMIC.frame[LMIC.dataBeg-1]);
              if (rx_callback) {
                  // Pass instance, port, data pointer, and length to the callback
                  rx_callback(this, LMIC.frame[LMIC.dataBeg-1], LMIC.frame+LMIC.dataBeg, LMIC.dataLen);
              }
            }
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE: // Data received in a scheduled RX window (e.g., Class B ping slot)
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        case EV_TXCANCELED:
            Serial.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART: // Radio is preparing to receive, do not block with long operations
            // Serial.println(F("EV_RXSTART")); // Can be very verbose
            break;
        case EV_JOIN_TXCOMPLETE: // Join Request sent, no Join Accept received yet
            Serial.println(F("EV_JOIN_TXCOMPLETE"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
} 