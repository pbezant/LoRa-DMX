// LoRa-DMX Class C Controller (procedural, helpers)
// US915, OTAA, Class C, JSON downlink, full DMX logic, periodic uplink

#include <Arduino.h>
#include <heltec_unofficial.h> // MODIFIED: Use the header specified in library.properties
#include "secrets.h" // For LoRaWAN credentials (in include/)
#include "LoRaWANHelper.h" // Now in lib/
#include "DMXHelper.h"       // Now in lib/
#include "LEDHelper.h"         // Now in lib/
#include <ArduinoJson.h>

#define LED_PIN 25

// --- Configuration ---
const unsigned long UPLINK_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes
unsigned long last_uplink_ms = 0;

// Helper function to convert a hex character to its integer value
static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1; // Invalid hex char
}

// Helper function to convert a hex string to a byte array
static void hex_string_to_bytes(const char* hex_string, uint8_t* byte_array, size_t byte_array_len) {
    size_t hex_len = strlen(hex_string);
    size_t max_bytes = hex_len / 2;
    if (byte_array_len > max_bytes) byte_array_len = max_bytes; // Prevent overflow

    for (size_t i = 0; i < byte_array_len; ++i) {
        int high_nibble = hex_char_to_int(hex_string[i * 2]);
        int low_nibble = hex_char_to_int(hex_string[i * 2 + 1]);
        if (high_nibble == -1 || low_nibble == -1) {
            // Handle error: invalid hex string
            // For simplicity, just zero out the rest and return
            memset(byte_array + i, 0, byte_array_len - i);
            return;
        }
        byte_array[i] = (high_nibble << 4) | low_nibble;
    }
}

// --- Pattern enum (must match DMXHelper) ---
enum PatternType { NONE, COLORFADE, RAINBOW, STROBE, CHASE, ALTERNATE };

// --- LED blink helper ---
void blinkLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_PIN, LOW);
        delay(delayMs);
    }
}

// --- Downlink Callback ---
void handle_downlink(const uint8_t* payload, size_t len, uint8_t fport) {
    led_indicate_downlink(); // Blink for downlink received
    Serial.print(F("[Main] Downlink received on FPort "));
    Serial.print(fport);
    Serial.print(F(", Length: "));
    Serial.println(len);
    Serial.print(F("Payload: "));
    for (size_t i = 0; i < len; i++) {
        Serial.printf("%02X ", payload[i]);
    }
    Serial.println();

    dmx_helper_process_json_command((const char*)payload, len);
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println(F("Starting LoRa-DMX Controller V3 (Class C True - Attempt 3)..."));

    led_helper_init();
    led_indicate_startup();

    dmx_helper_init(); 

    // Prepare LoRaWAN credentials from hex strings
    uint8_t dev_eui_bytes[8];
    uint8_t app_eui_bytes[8];
    uint8_t app_key_bytes[16];

    hex_string_to_bytes(DEVEUI, dev_eui_bytes, sizeof(dev_eui_bytes));
    hex_string_to_bytes(APPEUI, app_eui_bytes, sizeof(app_eui_bytes));
    hex_string_to_bytes(APPKEY, app_key_bytes, sizeof(app_key_bytes));

    // Pass the global 'radio' object (SX1262 type from heltec.h), Serial for debug, desired app interval, and the downlink callback.
    // The EUI/Key byte arrays are no longer passed here; lorawan_helper_join will use strings from secrets.h.
    // Explicitly cast &radio to RadioLib::SX1262* to resolve type ambiguity.
    lorawan_helper_init( (RadioLib::SX1262*)&radio, &Serial, UPLINK_INTERVAL_MS, handle_downlink);

    if (lorawan_helper_join(&Serial)) { // Pass Serial for debugging
        led_indicate_join_success();
        Serial.println(F("LoRaWAN Join successful. True Class C receive should be active."));
    } else {
        led_indicate_join_fail();
        Serial.println(F("LoRaWAN Join failed."));
    }
    last_uplink_ms = millis();
}

void loop() {
    lorawan_helper_loop(&Serial); // Pass Serial for debugging
    dmx_helper_loop();
    delay(5); // Small delay
}

