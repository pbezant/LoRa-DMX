#ifndef HELTEC_LORAWAN_H
#define HELTEC_LORAWAN_H

#include <Arduino.h>
#include <functional>

class HeltecLoRaWAN {
public:
    using DownlinkCallback = std::function<void(uint8_t* payload, size_t size, uint8_t port)>;

    HeltecLoRaWAN();
    bool begin(int csPin, int dio1Pin, int resetPin, int busyPin);
    void setDownlinkCallback(DownlinkCallback cb);
    bool sendString(const String& payload, uint8_t port, bool confirmed);
    bool joinNetwork();
    bool setDeviceClass(char deviceClass);
    char getDeviceClass();
    void handleEvents();
    bool setCredentialsHex(uint64_t joinEUI, uint64_t devEUI, const char* appKey, const char* nwkKey = nullptr);

private:
    DownlinkCallback downlinkCb;
    bool joined;
    char currentClass;
    // Add any other private members needed for Heltec LoRaWAN state
};

#endif // HELTEC_LORAWAN_H 