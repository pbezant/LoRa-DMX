// Project-specific definitions for MCCI LMIC
#pragma once

// LMIC printf target
#define LMIC_PRINTF_TO Serial

// Configure SX1262 radio
#define CFG_sx1262_radio 1

// Define US915 band by default (can be overridden in platformio.ini)
#define CFG_us915 1

// Disable features we don't need to save memory and reduce complexity
#define DISABLE_PING
#define DISABLE_BEACONS

// Enable continuous RX for Class C
#define LMIC_ENABLE_continuous_rx 1
#define LMIC_setClassC LMIC_setupClassC

// Set higher clock error to be more tolerant
#define LMIC_CLOCK_ERROR_PERCENTAGE 5

// Enable these options for better handling of SX1262 on ESP32
#define LMIC_ENABLE_arbitrary_clock_error 1
#define LMIC_SPI_FREQ 1000000  // 1MHz SPI clock - slower but more stable

// Enable Lora device-time requests
#define LMIC_ENABLE_DeviceTimeReq 1

// Enable debug logging
#define LMIC_DEBUG_LEVEL 1  // Moderate debug level
#define LMIC_FAILURE_TO Serial

// Use long messages format
#define LMIC_ENABLE_long_messages 1 