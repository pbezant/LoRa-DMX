#include "LEDHelper.h"

void led_helper_init() {
    pinMode(HELPER_LED_PIN, OUTPUT);
    digitalWrite(HELPER_LED_PIN, LOW); // Start with LED off
}

void led_helper_blink(int count, int duration_ms) {
    for (int i = 0; i < count; ++i) {
        digitalWrite(HELPER_LED_PIN, HIGH);
        delay(duration_ms);
        digitalWrite(HELPER_LED_PIN, LOW);
        if (i < count - 1) {
            delay(duration_ms); // Delay between blinks, but not after the last one
        }
    }
}

void led_indicate_startup() {
    led_helper_blink(2, 250); // Two blinks for startup
}

void led_indicate_join_success() {
    // Quick triple blink for join success
    for (int i = 0; i < 3; ++i) {
        digitalWrite(HELPER_LED_PIN, HIGH);
        delay(100);
        digitalWrite(HELPER_LED_PIN, LOW);
        delay(100);
    }
}

void led_indicate_join_fail() {
    // Rapid 5 blinks for join fail
    for (int i = 0; i < 5; ++i) {
        digitalWrite(HELPER_LED_PIN, HIGH);
        delay(50);
        digitalWrite(HELPER_LED_PIN, LOW);
        delay(50);
    }
}

void led_indicate_uplink() {
    led_helper_blink(1, 100); // Single short blink
}

void led_indicate_downlink() {
    led_helper_blink(2, 100); // Double short blink
}

void led_indicate_error() {
    // SOS: 3 short, 3 long, 3 short
    for (int j=0; j<2; j++) { // Repeat SOS twice for more visibility
        for (int i = 0; i < 3; ++i) { // S
            digitalWrite(HELPER_LED_PIN, HIGH);
            delay(150);
            digitalWrite(HELPER_LED_PIN, LOW);
            delay(100);
        }
        delay(200);
        for (int i = 0; i < 3; ++i) { // O
            digitalWrite(HELPER_LED_PIN, HIGH);
            delay(400);
            digitalWrite(HELPER_LED_PIN, LOW);
            delay(100);
        }
        delay(200);
        for (int i = 0; i < 3; ++i) { // S
            digitalWrite(HELPER_LED_PIN, HIGH);
            delay(150);
            digitalWrite(HELPER_LED_PIN, LOW);
            delay(100);
        }
        delay(500); // Pause between SOS repeats
    }
} 