#pragma once

// Heltec board configuration constants (from ESP32_Mcu.h comments)
#define HELTEC_BOARD 30  // WIFI_LORA_32_V3
#define SLOW_CLK_TPYE 0  // External 32K crystal (0 = no external crystal)
#define LoRaWAN_DEBUG_LEVEL 0  // Debug level for LoRaWAN

// RADIO_NSS pin definition (required by Heltec library)
#define RADIO_NSS 8  // NSS pin for LoRa radio on Heltec WiFi LoRa 32 V3 