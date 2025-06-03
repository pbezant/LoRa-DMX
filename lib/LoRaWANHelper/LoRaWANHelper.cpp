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
#include "../SX1262Radio/SX1262Radio.h"
#include "../SX1262Radio/LoRaWANAdapter.h"

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

// Our new hardware and protocol layers
static SX1262Radio* radio_hw = nullptr;
static LoRaWANAdapter* lorawan_adapter = nullptr;

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

// Callback to handle downlink data from LoRaWANAdapter
static void lorawan_rx_callback(uint8_t* buffer, uint8_t length, int16_t rssi, float snr);

void IRAM_ATTR lorawan_custom_class_c_isr() {
    lorawan_packet_received_flag = true;
}

// Initialize the LoRaWAN helper
bool lorawan_helper_init(Print* debug_print, uint32_t app_interval, lorawan_downlink_callback_t downlink_callback) {
    if (debug_print) debug_print->println("[LoRaWANHelper] Initializing with nopnop2002's SX1262 driver + RadioLib...");
    
    // Store user callback and application interval
    user_downlink_callback = downlink_callback;
    application_interval_ms = app_interval;
    
    // Initialize mutex for thread-safe downlink handling
    lorawan_downlink_mutex = xSemaphoreCreateMutex();
    
    // Create our hardware layer
    radio_hw = new SX1262Radio();
    if (!radio_hw) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to create SX1262Radio instance!");
        return false;
    }

    // Initialize hardware
    if (!radio_hw->begin()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to initialize SX1262Radio!");
        return false;
    }
    
    if (debug_print) {
        debug_print->println("[LoRaWANHelper] SX1262Radio initialized successfully!");
        debug_print->print("[LoRaWANHelper] Supported bandwidths: ");
        for (int i = 0; i < radio_hw->getNumSupportedBandwidths(); i++) {
            debug_print->print(radio_hw->getSupportedBandwidth(i));
            debug_print->print(" kHz");
            if (i < radio_hw->getNumSupportedBandwidths() - 1) {
                debug_print->print(", ");
            }
        }
        debug_print->println();
    }
    
    // Create our protocol layer
    lorawan_adapter = new LoRaWANAdapter();
    if (!lorawan_adapter) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to create LoRaWANAdapter instance!");
        return false;
    }

    // Initialize protocol layer with our hardware
    if (!lorawan_adapter->begin(radio_hw)) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to initialize LoRaWANAdapter!");
        return false;
    }

    // Set the downlink callback
    lorawan_adapter->setRxCallback(lorawan_rx_callback);
    
    if (debug_print) debug_print->println("[LoRaWANHelper] LoRaWANAdapter initialized successfully!");
    
    return true;
}

// Set the callback function for handling downlink data
void lorawan_helper_set_downlink_callback(lorawan_downlink_callback_t cb) {
    user_downlink_callback = cb;
}

// Callback to handle downlink data from LoRaWANAdapter
static void lorawan_rx_callback(uint8_t* buffer, uint8_t length, int16_t rssi, float snr) {
    // Take the mutex
    if (xSemaphoreTake(lorawan_downlink_mutex, (TickType_t)10) == pdTRUE) {
        // Copy the data to our buffer
        if (length <= LORAWAN_DOWNLINK_MAX_SIZE) {
            memcpy(lorawan_downlink_data, buffer, length);
            lorawan_downlink_data_len = length;
            lorawan_packet_received_flag = true;
        }
        
        // Release the mutex
        xSemaphoreGive(lorawan_downlink_mutex);
    }
}

// Join the LoRaWAN network using OTAA
bool lorawan_helper_join(Print* debug_print) {
    if (!radio_hw || !lorawan_adapter) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot join: Hardware/Protocol layers not initialized.");
        return false;
    }
    
    if (debug_print) {
        debug_print->println("[LoRaWANHelper] Starting OTAA join with the following credentials:");
        debug_print->print("[LoRaWANHelper] DevEUI: ");
        print_eui(debug_print, DEVEUI);
        debug_print->print("[LoRaWANHelper] JoinEUI: ");
        print_eui(debug_print, APPEUI);
        debug_print->print("[LoRaWANHelper] AppKey: ");
        print_appkey(debug_print, APPKEY);
    }
    
    // Convert credentials from strings to the required formats
    uint64_t devEUI = eui_string_to_u64(DEVEUI);
    uint64_t joinEUI = eui_string_to_u64(APPEUI);
    uint8_t nwkKey[16];
    uint8_t appKey[16];
    appkey_string_to_bytes(NWKKEY, nwkKey);
    appkey_string_to_bytes(APPKEY, appKey);
    
    // Attempt to join
    joined_otaa = lorawan_adapter->joinOTAA(devEUI, joinEUI, nwkKey, appKey);
    
    if (joined_otaa) {
        if (debug_print) {
            debug_print->println("[LoRaWANHelper] OTAA join successful!");
            debug_print->printf("[LoRaWANHelper] Device Address: 0x%08X\n", lorawan_adapter->getDevAddr());
        }
        
        // Enable Class C after joining
        if (lorawan_adapter->enableClassC()) {
            custom_class_c_enabled = true;
            if (debug_print) debug_print->println("[LoRaWANHelper] Class C enabled successfully!");
        } else {
            if (debug_print) debug_print->println("[LoRaWANHelper] Failed to enable Class C!");
            custom_class_c_enabled = false;
        }
    } else {
        if (debug_print) debug_print->println("[LoRaWANHelper] OTAA join failed!");
}

    return joined_otaa;
}

// Send an uplink message
int lorawan_helper_send_uplink(const char* data, size_t len, bool confirmed, Print* debug_print) {
    if (!radio_hw || !lorawan_adapter || !lorawan_adapter->isJoined()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot send uplink: Not joined or not initialized.");
        return -1;
    }

    if (debug_print) debug_print->printf("[LoRaWANHelper] Sending uplink: %s (len: %d), confirmed: %d\n", data, len, confirmed);
    
    // Send the data
    bool success = lorawan_adapter->send((uint8_t*)data, len, 1, confirmed);

    if (success) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Uplink send successful.");
        return 0; // Success (corresponds to RADIOLIB_ERR_NONE)
    } else {
        if (debug_print) debug_print->println("[LoRaWANHelper] Uplink send failed.");
        return -2; // Generic failure
        }
    }

// Process any pending downlink data
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

// Check and re-enable Class C if needed
bool lorawan_helper_check_class_c(Print* debug_print) {
    if (!radio_hw || !lorawan_adapter || !lorawan_adapter->isJoined()) {
        return false;
    }
    
    return lorawan_adapter->isClassCEnabled();
}

// Enable Class C receive mode
bool lorawan_helper_enable_class_c_receive(Print* debug_print) {
    if (!radio_hw || !lorawan_adapter || !lorawan_adapter->isJoined()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot enable Class C: Not joined or not initialized.");
        return false;
    }
    
    if (debug_print) debug_print->println("[LoRaWANHelper] Enabling Class C receive mode...");
    
    bool success = lorawan_adapter->enableClassC();
    
    if (success) {
        custom_class_c_enabled = true;
        if (debug_print) debug_print->println("[LoRaWANHelper] Class C enabled successfully!");
    } else {
        custom_class_c_enabled = false;
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to enable Class C!");
    }
    
    return success;
    }
    
// Main loop processing
void lorawan_helper_loop(Print* debug_print) {
    if (!radio_hw || !lorawan_adapter) {
        return;
    }
    
    // Let the adapter handle events
    lorawan_adapter->loop();
    
    // Check if we have pending downlink data
    if (lorawan_packet_received_flag) {
        lorawan_packet_received_flag = false;
        
        if (debug_print) debug_print->println("[LoRaWANHelper] Packet received flag detected!");
        
        // Process the downlink
        lorawan_helper_process_pending_downlink(debug_print);
    }
}

// Check if the device has joined the network
bool lorawan_helper_is_joined() {
    if (!lorawan_adapter) {
        return false;
    }
    
    return lorawan_adapter->isJoined();
}

// Get the device address
uint32_t lorawan_helper_get_dev_addr(Print* debug_print) {
    if (!lorawan_adapter || !lorawan_adapter->isJoined()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot get DevAddr: Not joined or not initialized.");
        return 0;
    }
    
    return lorawan_adapter->getDevAddr();
}

// Convert a hex string EUI to uint64_t
static uint64_t eui_string_to_u64(const char* eui_str) {
    uint64_t eui = 0;
    for (int i = 0; i < 16; i += 2) {
        char hex[3] = {eui_str[i], eui_str[i+1], 0};
        uint8_t byte = strtoul(hex, NULL, 16);
        eui = (eui << 8) | byte;
    }
    return eui;
}

// Convert a hex string AppKey to a byte array
static void appkey_string_to_bytes(const char* appkey_str, uint8_t* key_bytes) {
    for (int i = 0; i < 16; i++) {
        char hex[3] = {appkey_str[i*2], appkey_str[i*2+1], 0};
        key_bytes[i] = strtoul(hex, NULL, 16);
    }
} 