#include "HeltecLoRaWAN.h"
#include <heltec.h>

// Static callback trampoline
static HeltecLoRaWAN* _instance = nullptr;

// Forward declaration for Heltec downlink callback
void onHeltecDownlink(uint8_t* payload, uint16_t size, uint8_t port);

HeltecLoRaWAN::HeltecLoRaWAN() : joined(false), currentClass('A') {
    _instance = this;
}

bool HeltecLoRaWAN::begin(int csPin, int dio1Pin, int resetPin, int busyPin) {
    // Initialize Heltec board (OLED off, LoRa on, Serial on)
    Heltec.begin(true /*DisplayEnable*/, true /*LoRaEnable*/, true /*SerialEnable*/, true /*PABOOST*/, csPin, resetPin, dio1Pin);
    // TODO: Set SPI pins if needed (Heltec library usually auto-detects)
    return true;
}

void HeltecLoRaWAN::setDownlinkCallback(DownlinkCallback cb) {
    downlinkCb = cb;
    // Register the static callback with the Heltec LoRaWAN stack
    LoRaWAN.onReceive(onHeltecDownlink);
}

bool HeltecLoRaWAN::sendString(const String& payload, uint8_t port, bool confirmed) {
    // Send a string as a LoRaWAN payload
    int ret = LoRaWAN.send((uint8_t*)payload.c_str(), payload.length(), port, confirmed ? 1 : 0);
    return (ret == 0);
}

bool HeltecLoRaWAN::joinNetwork() {
    // Start OTAA join
    LoRaWAN.join();
    // Wait for join (blocking for now, could be async)
    // TODO: Replace with event-driven join for production
    unsigned long start = millis();
    while (!LoRaWAN.isJoined() && millis() - start < 15000) {
        delay(100);
    }
    joined = LoRaWAN.isJoined();
    return joined;
}

bool HeltecLoRaWAN::setDeviceClass(char deviceClass) {
    // Set device class ('A', 'B', 'C')
    int ret = LoRaWAN.setDeviceClass(deviceClass);
    if (ret == 0) {
        currentClass = deviceClass;
        return true;
    }
    return false;
}

char HeltecLoRaWAN::getDeviceClass() {
    return currentClass;
}

void HeltecLoRaWAN::handleEvents() {
    // Heltec LoRaWAN stack is event-driven, nothing needed here for now
}

bool HeltecLoRaWAN::setCredentialsHex(uint64_t joinEUI, uint64_t devEUI, const char* appKey, const char* nwkKey) {
    // Set OTAA credentials
    uint8_t appEuiArray[8];
    uint8_t devEuiArray[8];
    for (int i = 0; i < 8; i++) {
        appEuiArray[7 - i] = (joinEUI >> (i * 8)) & 0xFF;
        devEuiArray[7 - i] = (devEUI >> (i * 8)) & 0xFF;
    }
    LoRaWAN.setDevEui(devEuiArray);
    LoRaWAN.setAppEui(appEuiArray);
    LoRaWAN.setAppKey((uint8_t*)appKey);
    // nwkKey is not used for OTAA
    return true;
}

// Static callback trampoline
void onHeltecDownlink(uint8_t* payload, uint16_t size, uint8_t port) {
    if (_instance && _instance->downlinkCb) {
        _instance->downlinkCb(payload, size, port);
    }
} 