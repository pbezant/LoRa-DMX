#ifndef LED_HELPER_H
#define LED_HELPER_H

#include <Arduino.h>

// Standard LED pin for Heltec LoRa 32 V3 (often GPIO 35, but check specific board variant if issues)
// Using LED_BUILTIN is safer if defined correctly by the board package.
// For Heltec ESP32 LoRa V2/V3, LED_BUILTIN is often mapped to GPIO 25.
// Let's try with a common GPIO for Heltec, or fallback to LED_BUILTIN.
#if defined(HELTEC_WIFI_LORA_32_V3) || defined(HELTEC_WIRELESS_STICK_LITE)
  #define HELPER_LED_PIN 35 // GPIO35 for V3 and Wireless Stick Lite
#elif defined(HELTEC_WIFI_LORA_32) // V2
  #define HELPER_LED_PIN 25 // GPIO25 for V2
#else
  #define HELPER_LED_PIN LED_BUILTIN // Fallback
#endif

void led_helper_init();
void led_helper_blink(int count, int duration_ms);

void led_indicate_startup();        // e.g., 1 long blink
void led_indicate_join_success();   // e.g., 3 short blinks
void led_indicate_join_fail();      // e.g., 5 rapid blinks
void led_indicate_uplink();         // e.g., 1 short blink
void led_indicate_downlink();       // e.g., 2 short blinks
void led_indicate_error();          // e.g., SOS pattern or continuous fast blink

#endif // LED_HELPER_H 