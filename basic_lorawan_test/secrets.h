#pragma once

// LoRaWAN credentials

// Replace these with your actual device credentials from your LoRaWAN network server
// (e.g., The Things Network, Helium, etc.)

// Device EUI (8 bytes, hex format without spaces)
#define DEVEUI "90cff868ef8bd4cc"

// Application EUI (8 bytes, hex format without spaces)
#define APPEUI "ED733220D2A9F133"

// Application Key (16 bytes, hex format without spaces)
#define APPKEY "f7edcfe4617e66701665a13a2b76dd52"

// Network Key (16 bytes, hex format without spaces)
// For LoRaWAN 1.1, this should be a different key than APPKEY
// For LoRaWAN 1.0.x, this can be the same as APPKEY
#define NWKKEY "f7edcfe4617e66701665a13a2b76dd52"

// Note: For security reasons, it's recommended to store these credentials
// in a separate file that is not committed to version control.
// You should replace these placeholder values with your actual credentials. 