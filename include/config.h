#pragma once

// Define a value for unused pins 
// Use a different name to avoid conflict with MCCI LMIC
// Use 0xFF (255) which matches LMIC's internal definition
#define CONFIG_UNUSED_PIN 0xFF

// Pin configuration for Heltec LoRa 32 V3 (ESP32S3)
#define CONFIG_BOARD_HELTEC_V3

// SX1262 LoRa radio pins (Heltec V3)
#ifdef CONFIG_BOARD_HELTEC_V3
    // SPI interface
    #define LORA_CS    8
    #define LORA_SCK   9
    #define LORA_MOSI  10
    #define LORA_MISO  11
    #define LORA_RESET 7
    #define LORA_DIO1  14
    #define LORA_BUSY  12

    // LMIC-specific description of these pins
    #define LMIC_PIN_NSS       LORA_CS
    #define LMIC_PIN_RST       LORA_RESET
    #define LMIC_PIN_DIO0      LORA_DIO1    // DIO1 is mapped to DIO0 in LMIC
    #define LMIC_PIN_DIO1      CONFIG_UNUSED_PIN
    #define LMIC_PIN_DIO2      CONFIG_UNUSED_PIN
    #define LMIC_PIN_BUSY      LORA_BUSY

    // DMX pins
    #define DMX_TX      19   // Serial TX to MAX485 DI
    #define DMX_RX      20   // Serial RX from MAX485 RO (if needed)
    #define DMX_DIR     5    // Direction control (connected to DE & RE)
#endif

// Uncomment and adapt for other boards when needed
/*
// Pin configuration for RAK Wireless (ESP32)
#ifdef CONFIG_BOARD_RAK_WIRELESS
    // SPI interface
    #define LORA_CS    ...
    #define LORA_SCK   ...
    #define LORA_MOSI  ...
    #define LORA_MISO  ...
    #define LORA_RESET ...
    #define LORA_DIO1  ...
    #define LORA_BUSY  ...

    // LMIC-specific description of these pins
    #define LMIC_PIN_NSS       LORA_CS
    #define LMIC_PIN_RST       LORA_RESET
    #define LMIC_PIN_DIO0      LORA_DIO1
    #define LMIC_PIN_DIO1      CONFIG_UNUSED_PIN
    #define LMIC_PIN_DIO2      CONFIG_UNUSED_PIN
    #define LMIC_PIN_BUSY      LORA_BUSY

    // DMX pins
    #define DMX_TX      ...
    #define DMX_RX      ...
    #define DMX_DIR     ...
#endif
*/

// LoRaWAN Configuration
#define DEFAULT_LORAWAN_REGION "US915"
#define DEFAULT_LORAWAN_SUBBAND 2
#define DEFAULT_LORAWAN_CLASS LORAWAN_CLASS_C  // LORAWAN_CLASS_A or LORAWAN_CLASS_C

// DMX Configuration
#define DMX_NUM_CHANNELS 512
#define DMX_DEFAULT_BAUDRATE 250000

// Diagnostic LED
#define LED_PIN      35

// Add more board-specific pins here as needed 