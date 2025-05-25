#pragma once

#include <stdint.h>
#include <stddef.h>
// #include <stdbool.h> // bool is available in C++

#define LORAWAN_DOWNLINK_MAX_SIZE 64

// Forward declare RadioLib types if used in C-compatible function signatures
// For C++ context, actual includes are in the .cpp
// However, if these types are directly in the C-linkage functions,
// we need a way for C compilers to see *something* or use void*.
// Given this is a .h for a .cpp, we can expect C++ context primarily.
// If C-only consumption was a strict goal, more `void*` and casting would be needed.

#ifdef __cplusplus
// Forward declaration for C++
namespace RadioLib {
    class SX1262;
}
class Print; // Forward declare Print for Arduino
#else
// For C, treat as opaque pointers if absolutely necessary, though this header is for C++ primarily
typedef void SX1262; 
typedef void Print;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Callback function type for downlink messages (payload, length, fport)
// This definition remains the same as it's fundamental.
typedef void (*lorawan_downlink_callback_t)(const uint8_t* payload, size_t len, uint8_t fport);


#ifdef __cplusplus
} // End extern "C" for C++ to define the C++ specific init
#endif

// Initialize LoRaWAN (US915, OTAA, Class C)
// This function now takes a pointer to the SX1262 radio object and a Print object for debugging.
// It's a C++ function due to RadioLib::SX1262* and Print*.
#ifdef __cplusplus
bool lorawan_helper_init(RadioLib::SX1262* radio_ptr, Print* debug_print, uint32_t app_interval, lorawan_downlink_callback_t downlink_callback);
#endif


#ifdef __cplusplus
extern "C" {
#endif

// Attempt to join the LoRaWAN network (OTAA)
bool lorawan_helper_join(Print* debug_print); // Added debug_print

// Set the callback function for handling downlink data.
// This might be slightly redundant if passed in init, but offers flexibility.
void lorawan_helper_set_downlink_callback(lorawan_downlink_callback_t cb);


// Send an uplink message.
// Changed to take const char* for data to match cpp, returns int (RadioLib status)
int lorawan_helper_send_uplink(const char* data, size_t len, bool confirmed, Print* debug_print);


// Main loop processing for LoRaWAN tasks (especially for custom Class C).
void lorawan_helper_loop(Print* debug_print); // Added debug_print

// Check if the device has successfully joined the network.
bool lorawan_helper_is_joined(); // Renamed from joined_otaa to reflect general state

// Retrieve the device address (DevAddr) after joining.
uint32_t lorawan_helper_get_dev_addr(Print* debug_print); // Added debug_print


// --- True Class C Specifics ---
// These are intended for the custom Class C implementation.

// Flag set by the radio's DIO1 ISR when a packet is received (or RX_DONE event).
// Needs to be accessible by the main loop.
extern volatile bool lorawan_packet_received_flag;

// Tracks if our custom Class C continuous receive mode is active.
// extern bool lorawan_is_class_c_active; // This was in .h but not consistently used in .cpp, custom_class_c_enabled is used internally. Let's remove.

// Manually enable/re-enable continuous receive for Class C on RX2 parameters.
// Returns true on success.
bool lorawan_helper_enable_class_c_receive(Print* debug_print); // Added debug_print

// Process any pending downlink data that has been buffered.
// This is called by lorawan_helper_loop after a packet is indicated by the ISR.
void lorawan_helper_process_pending_downlink(Print* debug_print); // Added debug_print

// --- End True Class C Specifics ---

#ifdef __cplusplus
} // End extern "C"
#endif 