#ifndef SECRETS_H
#define SECRETS_H

#include <stdint.h>
#include <Arduino.h>  // For String class

// LoRaWAN Credentials
const uint64_t joinEUI = 0x0000000000000001; // 0000000000000001
const uint64_t devEUI = 0x70B3D57ED80041B2;  // 70B3D57ED80041B2

// Keys in byte array format
uint8_t appKey[] = {0x45, 0xD3, 0x7B, 0xF3, 0x77, 0x61, 0xA6, 0x1F, 0x9F, 0x07, 0x1F, 0xE1, 0x6D, 0x4F, 0x57, 0x77}; // 45D37BF37761A61F9F071FE16D4F5777
uint8_t nwkKey[] = {0x45, 0xD3, 0x7B, 0xF3, 0x77, 0x61, 0xA6, 0x1F, 0x9F, 0x07, 0x1F, 0xE1, 0x6D, 0x4F, 0x57, 0x77}; // Same as appKey for OTAA

// Alternative format using hex strings (uncomment if you want to use this method instead)
const String appKeyHex = "45D37BF37761A61F9F071FE16D4F5777";
const String nwkKeyHex = "45D37BF37761A61F9F071FE16D4F5777";

#endif // SECRETS_H 