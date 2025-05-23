#pragma once

// LoRaWAN credentials

// Replace these with your actual device credentials from your LoRaWAN network server
// (e.g., The Things Network, Helium, etc.)

// Device EUI (8 bytes, hex format without spaces)
#define DEVEUI "522b03a2a79b1d87"

// Application EUI (8 bytes, hex format without spaces)
#define APPEUI "27ae8d0509c0aa88"

// Application Key (16 bytes, hex format without spaces)
#define APPKEY "2d6fb7d54929b790c52e66758678fb2e"

// Network Key (16 bytes, hex format without spaces)
// For LoRaWAN 1.1, this should be a different key than APPKEY
// For LoRaWAN 1.0.x, this can be the same as APPKEY
#define NWKKEY "2d6fb7d54929b790c52e66758678fb2e"

// Note: For security reasons, it's recommended to store these credentials
// in a separate file that is not committed to version control.
// You should replace these placeholder values with your actual credentials. 