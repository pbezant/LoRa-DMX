#include <Arduino.h> // Ensure Arduino/ESP32 core definitions are available (e.g., GPIO_NUM_X)

// Core Arduino/ESP includes first if any specific ones were needed beyond what Arduino.h pulls in.
// For now, assuming standard Arduino environment handles it.

// External Libraries - RadioLib needs to be very high for type definitions.
#include <RadioLib.h>
// #include <heltec_unofficial.h> // Provides global 'radio' and board specifics - REMOVED TO PREVENT MULTIPLE DEFINITIONS
#include <LoRaWAN_ESP32.h>    // May provide LoRaWAN specifics, ensure it plays nice with RadioLib direct use
#include <ArduinoJson.h>      // For potential future use

// Project-specific headers
#include "../../include/secrets.h" // For LORAWAN_DEVICE_EUI, etc. - CRITICAL for lorawan_helper_join
#include "LoRaWANHelper.h"       // Our own header, includes LORAWAN_DOWNLINK_MAX_SIZE

// Pin definition needed from heltec_unofficial.h, since we can't include the whole file
// GPIO_NUM_14 should be defined by Arduino/ESP32 core headers included via Arduino.h
#ifndef DIO1
#define DIO1 GPIO_NUM_14 
#endif

// Define LoRaWAN sync word if not defined in RadioLib
#ifndef RADIOLIB_LORAWAN_LORA_SYNC_WORD
#define RADIOLIB_LORAWAN_LORA_SYNC_WORD 0x34
#endif

// --- Global Variables & Static Declarations ---
// These can now use types fully defined by RadioLib.h and constants from secrets.h

// RadioLib-specific globals
LoRaWANNode* node = nullptr;
SX1262* sx1262_radio = nullptr; // Pointer to the global radio from heltec_unofficial.h
const LoRaWANBand_t* current_band = nullptr; // No RadioLib:: prefix for LoRaWANBand_t

// Internal state for LoRaWANHelper
static bool joined_otaa = false; // Tracks OTAA join status specifically
                               // Consider renaming or consolidating with a general 'is_joined' if appropriate

// Downlink data handling
static uint8_t lorawan_downlink_data[LORAWAN_DOWNLINK_MAX_SIZE];
static size_t lorawan_downlink_data_len = 0;
static lorawan_downlink_callback_t lorawan_user_downlink_callback = nullptr;
static SemaphoreHandle_t lorawan_downlink_mutex = nullptr;

// Custom Class C state
volatile bool lorawan_packet_received_flag = false; // ISR sets this (already extern in .h, definition here)
static bool custom_class_c_enabled = false;

// --- Deprecated/Conflicting global cleanup from previous file state ---
// static bool joined = false; // This seemed redundant with joined_otaa or a node->isJoined() check
// static void (*user_downlink_cb)(const uint8_t*, size_t, uint8_t) = nullptr; // Covered by lorawan_user_downlink_callback with correct type
// extern SX1262 radio; // This is WRONG - 'radio' is an object from heltec_unofficial.h, not an extern to be defined here.
                         // sx1262_radio will point to it.

// Forward declaration for the ISR (already in .h, but good for clarity if needed)
// static void IRAM_ATTR lorawan_custom_class_c_isr(); // Already in .h is sufficient

// Forward declaration for internal downlink handler - no longer needed as callback is direct
// static void internal_downlink_cb(uint8_t port, const uint8_t* payload, size_t len);

// Helper to convert EUI strings from secrets.h to uint64_t
uint64_t eui_string_to_u64(const char* eui_str) {
    uint64_t eui = 0;
    char temp[3]; // For holding two chars + null terminator
    temp[2] = '\0';
    for (int i = 0; i < 8; ++i) {
        temp[0] = eui_str[i * 2];
        temp[1] = eui_str[i * 2 + 1];
        eui |= (uint64_t)strtol(temp, nullptr, 16) << (56 - (i * 8));
    }
    return eui;
}

// Helper to convert AppKey string from secrets.h to byte array
void appkey_string_to_bytes(const char* appkey_str, uint8_t* key_bytes) {
    char temp[3];
    temp[2] = '\0';
    for (int i = 0; i < 16; ++i) {
        temp[0] = appkey_str[i * 2];
        temp[1] = appkey_str[i * 2 + 1];
        key_bytes[i] = (uint8_t)strtol(temp, nullptr, 16);
    }
}

void IRAM_ATTR lorawan_custom_class_c_isr() {
    lorawan_packet_received_flag = true;
}

bool lorawan_helper_init(SX1262* radio_ptr, Print* debug_print, uint32_t app_interval, lorawan_downlink_callback_t downlink_callback) {
    if (!radio_ptr) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Radio pointer is null!");
        return false;
    }
    sx1262_radio = radio_ptr; 

    // Select the correct band (US915)
    // The old constant RADIOLIB_LORAWAN_BAND_US915_HYBRID is gone.
    // We now use predefined band structs. For US915, it's &US915.
    // This requires RadioLib.h to have brought in the band definitions.
    current_band = &US915; // US915 is a globally available LoRaWANBand_t struct from RadioLib

    // Create the LoRaWAN node instance
    // The constructor now takes the radio pointer and a pointer to the band structure.
    // Note: sx1262_radio is already an SX1262*, which is a PhysicalLayer*.
    node = new LoRaWANNode(sx1262_radio, current_band);
    if (!node) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to create LoRaWANNode instance.");
        return false;
    }

    // If you need to set a sub-band (e.g., for TTN US915 which often uses FSB 2),
    // you might do it here. For example (check RadioLib docs for exact method if needed):
    // note: setSubBand is not available in this version of RadioLib - the 2nd parameter in constructor handles it
    // node->setSubBand(2); 

    if (debug_print) {
        // node->setDiag(debug_print); // Removed as per prior file state, was not there.
        debug_print->println("[LoRaWANHelper] LoRaWANNode created."); // Keep this for feedback
    }
    
    lorawan_user_downlink_callback = downlink_callback; // Match existing variable name from file context

    lorawan_downlink_mutex = xSemaphoreCreateMutex();
    if (lorawan_downlink_mutex == nullptr) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to create downlink mutex!");
        return false;
    }
    // Remove old node->begin() call, it's not used in the new API for LoRaWAN setup.

    return true;
}

void lorawan_helper_set_downlink_callback(lorawan_downlink_callback_t cb) {
    lorawan_user_downlink_callback = cb;
}

static void internal_downlink_cb(uint8_t port, const uint8_t* payload, size_t len) {
    if (lorawan_user_downlink_callback) lorawan_user_downlink_callback(payload, len, port);
}

int lorawan_helper_send_uplink(const char* data, size_t len, bool confirmed, Print* debug_print) {
    if (!node || !node->isActivated()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot send uplink: Not joined or node not initialized.");
        return -1;
    }

    if (debug_print) debug_print->printf("[LoRaWANHelper] Sending uplink: %s (len: %d), confirmed: %d\n", data, len, confirmed);
    
    // Use node->sendReceive() from RadioLib v7.x
    // Parameters: const uint8_t* dataUp, size_t lenUp, uint8_t fPort = 1, bool isConfirmed = false
    // We'll use the default fPort for now (usually 1).
    int state = RADIOLIB_ERR_NONE;
    uint8_t fPort = 1; // Default fPort value
    
    // Send the data without using the event objects
    state = node->sendReceive((const uint8_t*)data, len, fPort, confirmed);

    if (state == RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Uplink send successful.");
    } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Uplink send timeout reported by RadioLib.");
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT && confirmed) {
        // For confirmed uplinks, RADIOLIB_ERR_RX_TIMEOUT means TX was OK, but ACK not received.
        if (debug_print) debug_print->println("[LoRaWANHelper] Confirmed uplink sent, but no ACK received (timeout).");
    } else {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Uplink send failed, RadioLib error: %d\n", state);
    }
    
    // If custom Class C was enabled, we MUST re-establish it, as TX changes radio state.
    if (custom_class_c_enabled) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Re-enabling custom Class C receive after uplink.");
        bool reinit_ok = lorawan_helper_enable_class_c_receive(debug_print);
        if(!reinit_ok){
             if (debug_print) debug_print->println("[LoRaWANHelper] FAILED to re-enable custom Class C receive after uplink.");
             custom_class_c_enabled = false; // Mark as disabled if re-init fails
        }
    }

    // Return the RadioLib state code. 
    // The caller might interpret RADIOLIB_ERR_RX_TIMEOUT for confirmed messages differently (e.g. as a partial success)
    return state; 
}

void lorawan_helper_process_pending_downlink(Print* debug_print) {
    if (xSemaphoreTake(lorawan_downlink_mutex, (TickType_t)10) == pdTRUE) {
        if (lorawan_downlink_data_len > 0) {
            if (debug_print) {
                debug_print->print("[LoRaWANHelper] Processing downlink: ");
                for (size_t i = 0; i < lorawan_downlink_data_len; i++) {
                    debug_print->printf("%02X ", lorawan_downlink_data[i]);
                }
                debug_print->println();
            }
            if (lorawan_user_downlink_callback) {
                lorawan_user_downlink_callback(lorawan_downlink_data, lorawan_downlink_data_len, 0); // Port is currently unknown in this context
            }
            lorawan_downlink_data_len = 0; // Clear buffer
        }
        xSemaphoreGive(lorawan_downlink_mutex);
    }
}

void lorawan_helper_loop(Print* debug_print) {
    if (custom_class_c_enabled) {
        if (lorawan_packet_received_flag) {
            lorawan_packet_received_flag = false; // Reset flag

            if (debug_print) debug_print->println("[LoRaWANHelper] DIO1 ISR triggered, packet potentially received.");
            
            uint8_t downlink_buffer[LORAWAN_DOWNLINK_MAX_SIZE];
            size_t received_len = 0;
            int state = sx1262_radio->readData(downlink_buffer, LORAWAN_DOWNLINK_MAX_SIZE);

            if (state == RADIOLIB_ERR_NONE) {
                 received_len = sx1262_radio->getPacketLength();
                 if (debug_print) debug_print->printf("[LoRaWANHelper] Raw packet read from radio, len: %d\n", received_len);
                
                // In RadioLib v7.x, we need to parse the downlink manually since we're operating outside
                // the normal node->sendReceive() flow. This is a simplified implementation.
                if (received_len > 0) {
                    if (xSemaphoreTake(lorawan_downlink_mutex, (TickType_t)10) == pdTRUE) {
                        // Just store the raw data - in a real implementation, you'd want to parse
                        // the LoRaWAN packet properly and extract payload, port, etc.
                        memcpy(lorawan_downlink_data, downlink_buffer, received_len > LORAWAN_DOWNLINK_MAX_SIZE ? LORAWAN_DOWNLINK_MAX_SIZE : received_len);
                        lorawan_downlink_data_len = received_len > LORAWAN_DOWNLINK_MAX_SIZE ? LORAWAN_DOWNLINK_MAX_SIZE : received_len;
                        xSemaphoreGive(lorawan_downlink_mutex);
                    }
                }
                
                // Re-enable receive for next packet
                if (!lorawan_helper_enable_class_c_receive(debug_print)) {
                    if (debug_print) debug_print->println("[LoRaWANHelper] Failed to re-enable Class C receive after packet reception!");
                    custom_class_c_enabled = false;
                }
            } else {
                if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to read packet data, error: %d\n", state);
                
                // Try to re-enable receive
                if (!lorawan_helper_enable_class_c_receive(debug_print)) {
                    if (debug_print) debug_print->println("[LoRaWANHelper] Failed to re-enable Class C receive after error!");
                    custom_class_c_enabled = false;
                }
            }
        }
    }
}

bool lorawan_helper_join(Print* debug_print) {
    if (!node) {
        if (debug_print) debug_print->println("[LoRaWANHelper] LoRaWAN node not initialized!");
        return false;
    }

    if (debug_print) debug_print->println("[LoRaWANHelper] Starting OTAA join...");
    
    // Convert EUI strings to appropriate types for RadioLib
    uint64_t join_eui = eui_string_to_u64(APPEUI);
    uint64_t dev_eui = eui_string_to_u64(DEVEUI);

    // Convert AppKey string to byte array for RadioLib
    uint8_t app_key[16];
    appkey_string_to_bytes(APPKEY, app_key);

    // In RadioLib v7.x, we first configure the node with credentials, then activate
    int state = node->beginOTAA(join_eui, dev_eui, app_key, nullptr);
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to configure OTAA, error: %d\n", state);
        return false;
    }

    // Now activate the node (this performs the actual join procedure)
    // Use the simpler version without event object
    state = node->activateOTAA();
    
    if (state == RADIOLIB_LORAWAN_NEW_SESSION || state == RADIOLIB_LORAWAN_SESSION_RESTORED) {
        joined_otaa = true;
        
        if (debug_print) {
            debug_print->println("[LoRaWANHelper] OTAA join successful!");
            debug_print->printf("[LoRaWANHelper] Device address: 0x%08X\n", node->getDevAddr());
        }
        
        // Enable Class C operation by setting up continuous receive
        if (debug_print) debug_print->println("[LoRaWANHelper] Enabling Class C operation...");
        bool class_c_success = lorawan_helper_enable_class_c_receive(debug_print);
        
        if (!class_c_success) {
            if (debug_print) debug_print->println("[LoRaWANHelper] WARNING: Failed to enable Class C operation!");
            // Continue anyway, since the join was successful
        }
        
        return true;
    } else {
        joined_otaa = false;
        if (debug_print) debug_print->printf("[LoRaWANHelper] OTAA join failed, error: %d\n", state);
        return false;
    }
}

bool lorawan_helper_enable_class_c_receive(Print* debug_print) {
    if (!node || !node->isActivated()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot enable Class C: Not joined!");
        return false;
    }
    
    // First, set up the interrupt handler for DIO1
    attachInterrupt(DIO1, lorawan_custom_class_c_isr, RISING);
    
    // Get RX2 parameters from node (currently hardcoded from US915 spec)
    float freq = 923.3; // MHz, default US915 RX2 frequency
    uint8_t sf = 12;    // Default SF12 for RX2
    float bw = 500.0;   // kHz, US915 RX2 uses 500 kHz bandwidth
    
    // Configure radio for continuous receive at RX2 parameters
    int state = sx1262_radio->setFrequency(freq);
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set frequency: %d\n", state);
        return false;
    }
    
    // LoRa configuration for RX2
    state = sx1262_radio->setSpreadingFactor(sf);
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set spreading factor: %d\n", state);
        return false;
    }
    
    state = sx1262_radio->setBandwidth(bw);
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set bandwidth: %d\n", state);
        return false;
    }
    
    // Set coding rate to 4/5 (standard for LoRaWAN)
    state = sx1262_radio->setCodingRate(5);
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set coding rate: %d\n", state);
        return false;
    }
    
    // Configure sync word for LoRaWAN (0x34)
    state = sx1262_radio->setSyncWord(RADIOLIB_LORAWAN_LORA_SYNC_WORD);
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set sync word: %d\n", state);
        return false;
    }
    
    // Start continuous receive mode
    state = sx1262_radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to start receive: %d\n", state);
        return false;
    }
    
    custom_class_c_enabled = true;
    if (debug_print) debug_print->println("[LoRaWANHelper] Class C continuous receive enabled.");
    return true;
}

bool lorawan_helper_is_joined() {
    if (!node) {
        return false;
    }
    return node->isActivated();
}

uint32_t lorawan_helper_get_dev_addr(Print* debug_print) {
    if (!node || !node->isActivated()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot get DevAddr: Not joined!");
        return 0;
    }
    return node->getDevAddr();
} 