#ifndef DMX_CONTROLLER_H
#define DMX_CONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// LXESP32DMX configuration - prevent default object creation
#define DO_NO_CREATE_DEFAULT_LXESP32DMX_CLASS_OBJECT 1
#include <LXESP32DMX.h>

// DMX constants for compatibility
#define DMX_PACKET_SIZE 513

class DmxController {
private:
    LX32DMX* dmx;
    int txPin;
    int rxPin;
    int directionPin;
    bool initialized;
    
    // DMX data buffer (512 channels + start code)
    uint8_t dmxData[513];
    
    // Task and mutex for thread safety
    TaskHandle_t dmxTaskHandle;
    SemaphoreHandle_t dmxMutex;
    
    // DMX task function (static to work with FreeRTOS)
    static void dmxTask(void* parameter);
    
public:
    DmxController();
    ~DmxController();
    
    // Initialize DMX with pin configuration
    bool begin(int tx, int rx, int dir);
    
    // Set channel value (1-512)
    void setChannel(uint16_t channel, uint8_t value);
    
    // Get channel value (1-512)
    uint8_t getChannel(uint16_t channel);
    
    // Set multiple channels from fixture data
    bool setFixtureChannels(uint16_t startChannel, const JsonObject& fixture);
    
    // Update DMX output
    void update();
    
    // Check if DMX is initialized and running
    bool isRunning();
    
    // Stop DMX transmission
    void stop();
    
    // Get DMX data pointer for direct access
    uint8_t* getDmxData() { return dmxData; }
    
    // Legacy compatibility methods
    void sendData() { update(); }
    void saveSettings() { /* No-op for now - LXESP32DMX doesn't have persistence */ }
    void loadSettings() { /* No-op for now */ }
    
    // Fixture management methods
    int getNumFixtures() { return 4; } // Default number for compatibility
    void initializeFixtures(int numFixtures, int channelsPerFixture) { /* No-op */ }
    void setFixtureConfig(int id, const char* name, int startCh, int dimmerCh, int redCh, int greenCh, int blueCh) { /* No-op */ }
    void setFixtureColor(int fixtureId, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    
    // Pattern methods for backward compatibility
    void runRainbowChase(int cycles, int speed, bool staggered) { /* No-op */ }
    void runStrobeTest(int color, int count, int onTime, int offTime, bool alternate) { /* No-op */ }
    void updateRainbowStep(int step, bool staggered) { /* No-op */ }
    
    // Persistence methods
    bool saveCustomData(const char* key, uint8_t* data, size_t size) { return false; }
    bool loadCustomData(const char* key, uint8_t* data, size_t size) { return false; }
    
    // Static utility methods
    static void blinkLED(int pin, int count, int delayMs);
};

#endif // DMX_CONTROLLER_H 