// Core Arduino/ESP includes first if any specific ones were needed beyond what Arduino.h pulls in.
// For now, assuming standard Arduino environment handles it.

// External Libraries - RadioLib needs to be very high for type definitions.
#include <RadioLib.h>
#include <heltec_unofficial.h> // Provides global 'radio' and board specifics
#include <LoRaWAN_ESP32.h>    // May provide LoRaWAN specifics, ensure it plays nice with RadioLib direct use
#include <ArduinoJson.h>      // For potential future use

// Project-specific headers
#include "../../include/secrets.h" // For LORAWAN_DEVICE_EUI, etc. - CRITICAL for lorawan_helper_join
#include "LoRaWANHelper.h"       // Our own header, includes LORAWAN_DOWNLINK_MAX_SIZE

// --- Global Variables & Static Declarations ---
// These can now use types fully defined by RadioLib.h and constants from secrets.h

// RadioLib-specific globals
RadioLib::LoRaWANNode* node = nullptr;
RadioLib::SX1262* sx1262_radio = nullptr; // Pointer to the global radio from heltec_unofficial.h
RadioLib::LoRaWANBand* current_band = nullptr;

// Internal state for LoRaWANHelper
static bool joined_otaa = false; // Tracks OTAA join status specifically
                               // Consider renaming or consolidating with a general 'is_joined' if appropriate

// Downlink data handling
static char lorawan_downlink_data[LORAWAN_DOWNLINK_MAX_SIZE];
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

bool lorawan_helper_init(RadioLib::SX1262* radio_ptr, Print* debug_print, uint32_t app_interval, lorawan_downlink_callback_t downlink_callback) {
    if (!radio_ptr) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Radio pointer is null!");
        return false;
    }
    sx1262_radio = radio_ptr; 

    // Select the correct band (US915)
    // The old constant RADIOLIB_LORAWAN_BAND_US915_HYBRID is gone.
    // We now use predefined band structures. For US915, it's &US915.
    // This requires RadioLib.h to have brought in the band definitions.
    current_band = &US915; // This assumes US915 is a globally available LoRaWANBand_t struct from RadioLib

    // Create the LoRaWAN node instance
    // The constructor now takes the radio pointer and a pointer to the band structure.
    // Note: sx1262_radio is already an SX1262*, which is a PhysicalLayer*.
    node = new RadioLib::LoRaWANNode(sx1262_radio, current_band);
    if (!node) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to create LoRaWANNode instance.");
        return false;
    }

    // If you need to set a sub-band (e.g., for TTN US915 which often uses FSB 2),
    // you might do it here. For example (check RadioLib docs for exact method if needed):
    // node->setSubBand(2); 

    if (debug_print) {
        // node->setDiag(debug_print); // Removed as per prior file state, was not there.
        debug_print->println("[LoRaWANHelper] LoRaWANNode created."); // Keep this for feedback
    }
    
    ::lorawan_user_downlink_callback = downlink_callback; // Match existing variable name from file context

    lorawan_downlink_mutex = xSemaphoreCreateMutex();
    if (lorawan_downlink_mutex == nullptr) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Failed to create downlink mutex!");
        return false;
    }
    // Remove old node->begin() call, it's not used in the new API for LoRaWAN setup.

    return true;
}

void lorawan_helper_set_downlink_callback(void (*cb)(const uint8_t*, size_t, uint8_t)) {
    lorawan_user_downlink_callback = cb;
}

static void internal_downlink_cb(uint8_t port, const uint8_t* payload, size_t len) {
    if (lorawan_user_downlink_callback) lorawan_user_downlink_callback(payload, len, port);
}

int lorawan_helper_send_uplink(const char* data, size_t len, bool confirmed, Print* debug_print) {
    if (!node || !node->isJoined()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot send uplink: Not joined or node not initialized.");
        return -1;
    }

    if (debug_print) debug_print->printf("[LoRaWANHelper] Sending uplink: %s (len: %d), confirmed: %d\n", data, len, confirmed);
    
    // Use node->send() from RadioLib v7.x
    // Parameters: uint8_t* data, size_t len, bool confirmed, uint8_t fPort (optional, defaults to 1)
    // We'll use the default fPort for now.
    int state = node->send((uint8_t*)data, len, confirmed);

    if (state == RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Uplink send successful (or queued by RadioLib).");
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
        bool reinit_ok = lorawan_helper_enable_class_c_receive(debug_print); // Re-establish Class C receive.
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
                lorawan_user_downlink_callback(lorawan_downlink_data, lorawan_downlink_data_len);
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
                
                RadioLib::LoRaWANPktType_t pktType = node->parseDownlink(downlink_buffer, received_len);

                if (pktType != RadioLib::LoRaWANPktType_t::UNKNOWN && pktType != RadioLib::LoRaWANPktType_t::JOIN_ACCEPT) {
                    if (debug_print) debug_print->printf("[LoRaWANHelper] Downlink parsed, type: %d\n", static_cast<int>(pktType));

                    uint8_t appPayload[LORAWAN_DOWNLINK_MAX_SIZE];
                    size_t appPayloadLength = 0;
                    int getPayloadState = node->getDownlinkData(appPayload, &appPayloadLength, LORAWAN_DOWNLINK_MAX_SIZE);

                    if (getPayloadState == RADIOLIB_ERR_NONE && appPayloadLength > 0) {
                        if (xSemaphoreTake(lorawan_downlink_mutex, (TickType_t)10) == pdTRUE) {
                            memcpy(lorawan_downlink_data, appPayload, appPayloadLength);
                            lorawan_downlink_data_len = appPayloadLength;
                            xSemaphoreGive(lorawan_downlink_mutex);
                            if (debug_print) debug_print->printf("[LoRaWANHelper] Copied app payload (len %d) to user buffer.\n", appPayloadLength);
                        } else {
                             if (debug_print) debug_print->println("[LoRaWANHelper] Failed to take downlink mutex to store payload.");
                        }
                    } else if (appPayloadLength == 0) {
                        if (debug_print) debug_print->println("[LoRaWANHelper] Downlink contained no application payload (e.g., MAC commands only).");
                    }
                    else {
                        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to get app payload from parsed downlink, error: %d\n", getPayloadState);
                    }

                } else if (pktType == RadioLib::LoRaWANPktType_t::JOIN_ACCEPT) {
                    if (debug_print) debug_print->println("[LoRaWANHelper] Parsed a Join Accept. This should have been handled by beginOTAA.");
                }
                
                else {
                     if (debug_print) debug_print->println("[LoRaWANHelper] Failed to parse downlink or unknown packet type.");
                }

            } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
                if (debug_print) debug_print->println("[LoRaWANHelper] Packet received with CRC error.");
            } else {
                if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to read packet from radio, error: %d\n", state);
            }
            lorawan_helper_enable_class_c_receive(debug_print);

        }
        lorawan_helper_process_pending_downlink(debug_print);

    } else {
        // node->loop(); // Intentionally removed for now based on prior logic for custom Class C
    }
}

bool lorawan_helper_join(Print* debug_print) {
    if (!node) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Node not initialized.");
        return false;
    }
    // Ensure the global radio object is used by node if not set already (should be by init)
    // node->setDevice(sx1262_radio); // This kind of call is not typical in new API; radio is set at construction

    if (debug_print) debug_print->println("[LoRaWANHelper] Attempting OTAA join...");

    // Convert EUIs and AppKey from string format in secrets.h
    // DevEUI and JoinEUI (formerly AppEUI) are uint64_t
    // AppKey (used as NwkKey and AppKey for LoRaWAN 1.0.x) is a 16-byte array.

    uint64_t devEUI_u64 = eui_string_to_u64(LORAWAN_DEVICE_EUI);
    uint64_t joinEUI_u64 = eui_string_to_u64(LORAWAN_JOIN_EUI); // Previously AppEUI
    uint8_t appKey_bytes[16];
    appkey_string_to_bytes(LORAWAN_APP_KEY, appKey_bytes);

    // The old setDevEUI, setJoinEUI, setAppKey methods are gone.
    // Credentials are now passed directly to beginOTAA.
    // For LoRaWAN 1.0.x, AppKey is typically used as NwkKey and AppKey.
    // The order in beginOTAA is typically: joinEUI, devEUI, nwkKey, appKey.
    int state = node->beginOTAA(joinEUI_u64, devEUI_u64, appKey_bytes, appKey_bytes);

    if (state == RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->println("[LoRaWANHelper] OTAA Join successful!");
        joined_otaa = true; // Update internal join state
        
        // Optionally, attempt to set device to Class C in the LoRaWAN stack
        // This might be useful for the stack's internal state management, even with custom RX.
        // int class_c_state = node->setClass(RadioLib::LoRaWANClass::CLASS_C);
        // if (class_c_state == RADIOLIB_ERR_NONE) {
        //     if (debug_print) debug_print->println("[LoRaWANHelper] Successfully set LoRaWANNode to Class C mode.");
        // } else {
        //     if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set LoRaWANNode to Class C mode, error: %d\n", class_c_state);
        // }

        return true;
    } else {
        if (debug_print) {
            debug_print->printf("[LoRaWANHelper] OTAA Join failed, error: %d\n", state);
            // Consider adding node->getStatusString() or similar for more verbose errors if available in RadioLib v7
        }
        joined_otaa = false;
        return false;
    }
}

// Function to enable custom Class C continuous receive
bool lorawan_helper_enable_class_c_receive(Print* debug_print) {
    if (!node || !sx1262_radio || !node->isJoined()) {
        if (debug_print) debug_print->println("[LoRaWANHelper] Cannot enable Class C: Not joined or node/radio not initialized.");
        return false;
    }

    // Get RX2 parameters
    float freq = node->getRX2Freq();
    uint8_t dr = node->getRX2DR(); // Data Rate
    // For SX126x, data rate directly maps to SF and BW. We need to convert.
    // This part is tricky and needs correct mapping from LoRaWAN DR to SF/BW for SX126x.
    // RadioLib might have helper functions or the LoRaWANNode itself handles this when setting up RX.
    // For now, let's assume node->start kontinuas RX on RX2 might be enough, or we need to manually configure sx1262_radio.

    uint8_t sf = 0; // Spreading Factor
    float bw = 0.0; // Bandwidth

    // Example: Manually determine SF/BW from DR for US915 RX2 (typically DR8: SF12/500kHz)
    // This is highly band-specific and needs to be accurate.
    // The LoRaWAN regional parameters define these.
    // We should ideally get these translated by RadioLib if possible.
    if (current_band == &US915) { // Check if we are on US915
        switch (dr) {
            case 8: sf = 12; bw = 500.0; break; // US915 RX2 default
            // Add other DR to SF/BW mappings if necessary
            default:
                if (debug_print) debug_print->printf("[LoRaWANHelper] RX2 DR %d not implemented for SF/BW mapping.\n", dr);
                return false;
        }
    } else {
         if (debug_print) debug_print->println("[LoRaWANHelper] Band not supported for custom Class C SF/BW mapping.");
        return false;
    }
    
    if (debug_print) debug_print->printf("[LoRaWANHelper] Configuring for Class C continuous RX on Freq: %.2f MHz, SF: %d, BW: %.1f kHz\n", freq, sf, bw);

    // Put radio into continuous receive mode on RX2 parameters
    // The exact sequence might vary with RadioLib v7.x
    // sx1262_radio->setDio1Action(lorawan_custom_class_c_isr); // Set ISR for RX_DONE
    // int state = sx1262_radio->startReceiveContinuously(freq, bw, sf, RADIOLIB_SX126X_CR_4_5, RADIOLIB_SX126X_SYNC_WORD_LORAWAN, 1000); // Example, check parameters
    
    // Simpler approach: Ask LoRaWANNode to handle Class C receive window opening
    // This might be preferred if RadioLib provides a robust way.
    // For now, we'll try to manually configure the radio for continuous RX.

    // Configuration for SX1262 continuous receive:
    // 1. Set frequency
    // 2. Set bandwidth
    // 3. Set spreading factor
    // 4. Set coding rate (usually 4/5 for LoRaWAN)
    // 5. Set sync word (LoRaWAN public or private)
    // 6. Set preamble length (LoRaWAN default)
    // 7. Clear IRQ status
    // 8. Set DIO mapping for RX_DONE
    // 9. Start receive with a very long timeout (or 0 for continuous if supported)

    int state = sx1262_radio->setFrequency(freq);
    if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set RX frequency: %d\n", state);
        return false;
    }
    state = sx1262_radio->setBandwidth(bw);
     if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set RX bandwidth: %d\n", state);
        return false;
    }
    state = sx1262_radio->setSpreadingFactor(sf);
     if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set RX spreading factor: %d\n", state);
        return false;
    }
    state = sx1262_radio->setCodingRate(5); // CR 4/5
     if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set RX coding rate: %d\n", state);
        return false;
    }
    state = sx1262_radio->setSyncWord(RADIOLIB_LORAWAN_SYNC_WORD); // LoRaWAN public sync word
     if (state != RADIOLIB_ERR_NONE) {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to set RX sync word: %d\n", state);
        return false;
    }
    // sx1262_radio->setPreambleLength(8); // Default LoRaWAN preamble length

    // Clear any pending interrupt flags on DIO1
    sx1262_radio->clearIrqStatus();
    // Set the DIO1 interrupt to trigger on RX_DONE
    sx1262_radio->setDio1Action(lorawan_custom_class_c_isr);

    // Start continuous reception (timeout 0)
    state = sx1262_radio->startReceive(0, RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_TIMEOUT | RADIOLIB_SX126X_IRQ_CRC_ERR);
    // The flags above might need adjustment. We primarily care about RX_DONE.

    if (state == RADIOLIB_ERR_NONE) {
        custom_class_c_enabled = true;
        if (debug_print) debug_print->println("[LoRaWANHelper] Custom Class C continuous receive enabled.");
        return true;
    } else {
        if (debug_print) debug_print->printf("[LoRaWANHelper] Failed to start continuous receive, error: %d\n", state);
        return false;
    }
}

// Function to get the DevAddr (for debugging or info)
uint32_t lorawan_helper_get_dev_addr(Print* debug_print) {
    if (node && node->isJoined()) {
        return node->getDevAddr();
    }
    if (debug_print) debug_print->println("[LoRaWANHelper] Cannot get DevAddr, node not joined or not initialized.");
    return 0;
} 