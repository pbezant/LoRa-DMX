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

// Downlink handling
static uint8_t lorawan_downlink_data[LORAWAN_DOWNLINK_MAX_SIZE];
static size_t lorawan_downlink_data_len = 0;
static lorawan_downlink_callback_t user_downlink_callback = nullptr;
static SemaphoreHandle_t lorawan_downlink_mutex = nullptr;

// Class C configuration
static volatile bool custom_class_c_enabled = false;    // Flag to track if Class C is enabled
volatile bool lorawan_packet_received_flag = false; // Flag set by ISR when packet is received
static uint32_t application_interval_ms = 60000; // Default to 1 minute if not specified

// --- Helper Functions ---

// Print EUI in the format used by LoRaWAN (MSB first)
static void print_eui(Print* debug_print, const char* eui_str) {
    debug_print->print("0x");
    for (int i = 0; i < 16; i += 2) {
        debug_print->print(eui_str[i]);
        debug_print->print(eui_str[i+1]);
    }
    debug_print->println();
}

// Print AppKey in the format used by LoRaWAN
static void print_appkey(Print* debug_print, const char* key_str) {
    debug_print->print("0x");
    for (int i = 0; i < 32; i += 2) {
        debug_print->print(key_str[i]);
        debug_print->print(key_str[i+1]);
    }
    debug_print->println();
}

// --- Forward Declarations ---

// Helper to convert EUI strings from secrets.h to uint64_t
static uint64_t eui_string_to_u64(const char* eui_str);

// Helper to convert AppKey strings from secrets.h to byte array
static void appkey_string_to_bytes(const char* appkey_str, uint8_t* output_array);

// Internal downlink handler callback
static void internal_downlink_cb(uint8_t port, const uint8_t* payload, size_t len);

void IRAM_ATTR lorawan_custom_class_c_isr() {
    lorawan_packet_received_flag = true;
}

bool lorawan_helper_init(SX1262* radio_ptr, Print* debug_print, uint32_t app_interval, lorawan_downlink_callback_t downlink_callback) {
    if (!radio_ptr) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Radio pointer is null!");
        return false;
    }
    sx1262_radio = radio_ptr; 

    // Before starting LoRaWAN, verify that the radio can work with various bandwidths
    if (debug_print) debug_print->println("[LoRaWANHelper] Checking radio configuration with various bandwidths...");
    
    // Set radio to standby
    int state = sx1262_radio->standby();
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set radio to standby: %d\n", state);
        return false;
    }
    
    // Try different bandwidths to find one that works
    // SX1262 supports 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500 kHz
    float test_bandwidths[] = {125.0, 250.0, 500.0, 62.5, 31.25, 41.7, 20.8};
    bool found_valid_bw = false;
    float working_bw = 0;
    
    for (int i = 0; i < 7; i++) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Testing bandwidth %.2f kHz...\n", test_bandwidths[i]);
        state = sx1262_radio->setBandwidth(test_bandwidths[i]);
        if (state == RADIOLIB_ERR_NONE) {
            found_valid_bw = true;
            working_bw = test_bandwidths[i];
            if (debug_print) debug_print->printf("[LoRaWANHelper] Success! Radio supports %.2f kHz bandwidth.\n", working_bw);
            break;
        } else {
            if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set bandwidth to %.2f kHz: %d\n", test_bandwidths[i], state);
        }
    }
    
    if (!found_valid_bw) {
        if (debug_print) debug_print->println("[LoRaWANHelper] WARNING: Could not find a supported bandwidth! LoRaWAN may not work correctly.");
    } else if (working_bw != 125.0) {
        if (debug_print) {
            debug_print->println("[LoRaWANHelper] WARNING: Radio does not support 125 kHz bandwidth required by LoRaWAN!");
            debug_print->printf("[LoRaWANHelper] Using %.2f kHz instead, but this may cause network compatibility issues.\n", working_bw);
        }
    }
    
    // Use US915 band for LoRaWAN in the USA (Helium/Chirpstack)
    if (debug_print) debug_print->println("[LoRaWANHelper] Setting US915 band for LoRaWAN...");
    current_band = &US915;
    
    // Create the LoRaWAN node
    node = new LoRaWANNode(sx1262_radio, current_band);
    if (!node) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to create LoRaWANNode instance.");
        return false;
    }

    if (debug_print) {
        debug_print->println("[LoRaWANHelper] LoRaWANNode created.");
    }

    // Store user callback for downlink data
    user_downlink_callback = downlink_callback;
    application_interval_ms = app_interval;
    
    // Initialize mutex for thread-safe downlink handling
    lorawan_downlink_mutex = xSemaphoreCreateMutex();
    
    return true;
}

void lorawan_helper_set_downlink_callback(lorawan_downlink_callback_t cb) {
    user_downlink_callback = cb;
}

static void internal_downlink_cb(uint8_t port, const uint8_t* payload, size_t len) {
    if (user_downlink_callback) user_downlink_callback(payload, len, port);
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
            if (user_downlink_callback) {
                user_downlink_callback(lorawan_downlink_data, lorawan_downlink_data_len, 0); // Port is currently unknown in this context
            }
            lorawan_downlink_data_len = 0; // Clear buffer
        }
        xSemaphoreGive(lorawan_downlink_mutex);
    }
}

// New function to check and re-enable Class C if needed
bool lorawan_helper_check_class_c(Print* debug_print) {
    if (!node || !node->isActivated()) {
        return false;
    }

    if (custom_class_c_enabled && !lorawan_packet_received_flag) {
        // Just periodically re-enable Class C receive mode
        if (debug_print) {
            debug_print->println("[LoRaWANHelper] Refreshing Class C receive mode");
        }
        return lorawan_helper_enable_class_c_receive(debug_print);
    }
    
    return custom_class_c_enabled;
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
        
        // Periodically check and re-enable Class C if needed (every 5 minutes)
        static unsigned long last_check_time = 0;
        unsigned long now = millis();
        if (now - last_check_time > 300000) { // 5 minutes
            lorawan_helper_check_class_c(debug_print);
            last_check_time = now;
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

    // Debug: Print values
    if (debug_print) {
        debug_print->println("[LoRaWANHelper] OTAA parameters:");
        debug_print->print("  JoinEUI: "); print_eui(debug_print, APPEUI);
        debug_print->print("  DevEUI:  "); print_eui(debug_print, DEVEUI);
        debug_print->print("  AppKey:  "); print_appkey(debug_print, APPKEY);
        debug_print->printf("  Band:    %s\n", "US915");
    }

    // Attempt OTAA join multiple times
    const int MAX_JOIN_ATTEMPTS = 3;
    int join_attempts = 0;
    int state = RADIOLIB_ERR_NONE;
    bool join_success = false;
    
    while (join_attempts < MAX_JOIN_ATTEMPTS && !join_success) {
        join_attempts++;
        
        if (debug_print) {
            debug_print->printf("[LoRaWANHelper] Join attempt %d of %d...\n", join_attempts, MAX_JOIN_ATTEMPTS);
        }
        
        // We use the same key for both nwkKey and appKey to work with older networks
        state = node->beginOTAA(join_eui, dev_eui, app_key, app_key);
        
        if (state != RADIOLIB_ERR_NONE) {
            if (debug_print) {
                debug_print->printf("[LoRaWANHelper] Join attempt %d failed with error: %d\n", 
                    join_attempts, state);
                
                if (state == RADIOLIB_ERR_INVALID_BANDWIDTH) {
                    debug_print->println("[LoRaWANHelper] The radio hardware does not support the required bandwidth.");
                } else if (state == RADIOLIB_ERR_NETWORK_NOT_JOINED) {
                    debug_print->println("[LoRaWANHelper] The network did not accept the join request.");
                } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
                    debug_print->println("[LoRaWANHelper] Timed out waiting for join accept from gateway.");
                }
            }
            
            // Wait before trying again
            if (join_attempts < MAX_JOIN_ATTEMPTS) {
                if (debug_print) debug_print->println("[LoRaWANHelper] Waiting before retry...");
                delay(5000); // 5 second delay between attempts
            }
            continue;
        }
        
        // Check if we actually got a valid device address
        uint32_t dev_addr = node->getDevAddr();
        if (dev_addr == 0) {
            if (debug_print) {
                debug_print->printf("[LoRaWANHelper] Join attempt %d: Radio reports success but received zero device address!\n", join_attempts);
                debug_print->println("[LoRaWANHelper] This usually means the device did not actually connect to the network.");
            }
            
            if (join_attempts < MAX_JOIN_ATTEMPTS) {
                if (debug_print) debug_print->println("[LoRaWANHelper] Waiting before retry...");
                delay(5000); // 5 second delay between attempts
            }
            continue;
        }
        
        // If we got here, the join was successful
        join_success = true;
        
        if (debug_print) {
            debug_print->println("[LoRaWANHelper] LoRaWAN OTAA join successful!");
            debug_print->printf("[LoRaWANHelper] Device address: %08X\n", dev_addr);
            debug_print->println("[LoRaWANHelper] Default channels enabled:");
            for (int i = 0; i < 8; i++) {
                debug_print->printf("  Channel %d: %.1f MHz\n", i, 902.3 + (i * 0.2));
            }
        }
    }
    
    if (!join_success) {
        if (debug_print) {
            debug_print->printf("[LoRaWANHelper] Failed to join after %d attempts.\n", MAX_JOIN_ATTEMPTS);
            debug_print->println("[LoRaWANHelper] Possible issues:");
            debug_print->println("  1. Device is not registered on the network server");
            debug_print->println("  2. No gateway in range");
            debug_print->println("  3. Incorrect AppEUI, DevEUI, or AppKey");
            debug_print->println("  4. Radio hardware issues (e.g., antenna, bandwidth)");
        }
        return false;
    }
    
    joined_otaa = true;
    return true;
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
    float bw = 125.0;   // kHz, use 125 kHz bandwidth which is widely supported
    
    if (debug_print) debug_print->printf("[LoRaWANHelper] Configuring Class C continuous receive (RX2): %.1f MHz, SF%d, BW%.1f kHz\n", 
        freq, sf, bw);
    
    // Attempt to configure the radio with these parameters
    int state = sx1262_radio->standby();
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set radio to standby: %d\n", state);
        return false;
    }
    
    // Try setting frequency
    state = sx1262_radio->setFrequency(freq);
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set frequency: %d\n", state);
        return false;
    }
    
    // Try setting bandwidth with fallback options if the primary one fails
    // SX1262 supports 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500 kHz
    float bandwidths[] = {125.0, 250.0, 500.0, 62.5, 31.25, 41.7, 20.8, 15.6, 10.4, 7.8};
    bool bw_success = false;
    
    for (int i = 0; i < 10; i++) {
        state = sx1262_radio->setBandwidth(bandwidths[i]);
        if (state == RADIOLIB_ERR_NONE) {
            bw = bandwidths[i];
            bw_success = true;
            if (debug_print) debug_print->printf("[LoRaWANHelper] Set bandwidth to %.1f kHz\n", bw);
            break;
        } else {
            if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set bandwidth to %.1f kHz: %d\n", bandwidths[i], state);
        }
    }
    
    if (!bw_success) {
        if (debug_print) debug_print->println("[LoRaWANHelper] ERROR: Failed to set any bandwidth! Cannot continue.");
        return false;
    }
    
    // Try different spreading factors if SF12 fails
    bool sf_success = false;
    uint8_t spreading_factors[] = {12, 11, 10, 9, 8, 7};
    
    for (int i = 0; i < 6; i++) {
        state = sx1262_radio->setSpreadingFactor(spreading_factors[i]);
        if (state == RADIOLIB_ERR_NONE) {
            sf = spreading_factors[i];
            sf_success = true;
            if (debug_print) debug_print->printf("[LoRaWANHelper] Set spreading factor to SF%d\n", sf);
            break;
        } else {
            if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set SF%d: %d\n", spreading_factors[i], state);
        }
    }
    
    if (!sf_success) {
        if (debug_print) debug_print->println("[LoRaWANHelper] ERROR: Failed to set any spreading factor! Cannot continue.");
        return false;
    }
    
    // Set coding rate to 4/5 (standard for LoRaWAN) with fallback options
    int coding_rates[] = {5, 6, 7, 8};
    bool cr_success = false;
    
    for (int i = 0; i < 4; i++) {
        state = sx1262_radio->setCodingRate(coding_rates[i]);
        if (state == RADIOLIB_ERR_NONE) {
            if (debug_print) debug_print->printf("[LoRaWANHelper] Set coding rate to 4/%d\n", coding_rates[i]);
            cr_success = true;
            break;
        } else {
            if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set coding rate 4/%d: %d\n", coding_rates[i], state);
        }
    }
    
    if (!cr_success) {
        if (debug_print) debug_print->println("[LoRaWANHelper] ERROR: Failed to set any coding rate! Cannot continue.");
        return false;
    }
    
    // Start continuous receive
    state = sx1262_radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to start continuous receive: %d\n", state);
        return false;
    }
    
    if (debug_print) {
        debug_print->printf("[LoRaWANHelper] Class C continuous receive enabled at %.2f MHz, SF%d, BW %.1f kHz!\n", 
            freq, sf, bw);
    }
    custom_class_c_enabled = true;
    
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
        if (debug_print) debug_print->println("[LoRaWANHelper] Not joined, no device address available.");
        return 0;
    }
    return node->getDevAddr();
}

// --- Implementation ---

// Helper to convert EUI strings from secrets.h to uint64_t
static uint64_t eui_string_to_u64(const char* eui_str) {
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
static void appkey_string_to_bytes(const char* appkey_str, uint8_t* key_bytes) {
    char temp[3];
    temp[2] = '\0';
    for (int i = 0; i < 16; ++i) {
        temp[0] = appkey_str[i * 2];
        temp[1] = appkey_str[i * 2 + 1];
        key_bytes[i] = (uint8_t)strtol(temp, nullptr, 16);
    }
} 