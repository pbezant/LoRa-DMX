#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>

#define LORAWAN_DOWNLINK_MAX_SIZE 64

// Forward declarations
class SX1262Radio;
class LoRaWANAdapter;
class Print;

#ifdef __cplusplus
extern "C" {
#endif

// Callback function type for downlink messages (payload, length, fport)
typedef void (*lorawan_downlink_callback_t)(const uint8_t* payload, size_t len, uint8_t fport);


#ifdef __cplusplus
} // End extern "C" for C++ to define the C++ specific init
#endif

// Initialize LoRaWAN (US915, OTAA, Class C) with nopnop2002's SX1262 driver + RadioLib
// This function initializes the SX1262Radio and LoRaWANAdapter
#ifdef __cplusplus
bool lorawan_helper_init(Print* debug_print, uint32_t app_interval, lorawan_downlink_callback_t downlink_callback);
#endif


#ifdef __cplusplus
extern "C" {
#endif

// Attempt to join the LoRaWAN network (OTAA)
bool lorawan_helper_join(Print* debug_print);

// Set the callback function for handling downlink data.
void lorawan_helper_set_downlink_callback(lorawan_downlink_callback_t cb);

// Send an uplink message.
int lorawan_helper_send_uplink(const char* data, size_t len, bool confirmed, Print* debug_print);

// Main loop processing for LoRaWAN tasks (especially for custom Class C).
void lorawan_helper_loop(Print* debug_print);

// Check if the device has successfully joined the network.
bool lorawan_helper_is_joined();

// Retrieve the device address (DevAddr) after joining.
uint32_t lorawan_helper_get_dev_addr(Print* debug_print);

// --- True Class C Specifics ---

// Flag set by the radio's DIO1 ISR when a packet is received (or RX_DONE event).
// Needs to be accessible by the main loop.
extern volatile bool lorawan_packet_received_flag;

// Manually enable/re-enable continuous receive for Class C on RX2 parameters.
// Returns true on success.
bool lorawan_helper_enable_class_c_receive(Print* debug_print);

// Process any pending downlink data that has been buffered.
// This is called by lorawan_helper_loop after a packet is indicated by the ISR.
void lorawan_helper_process_pending_downlink(Print* debug_print);

// --- End True Class C Specifics ---

// Function declarations
bool lorawan_helper_check_class_c(Print* debug_print);

#ifdef __cplusplus
} // End extern "C"
#endif 