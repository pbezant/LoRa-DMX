/**
 * DmxController.h - A library for controlling DMX fixtures with ESP32
 * 
 * This library provides functionality for controlling RGBW DMX fixtures,
 * including setup, color control, and data transmission.
 * 
 * Compatible with esp_dmx 4.1.0 and newer
 */

#ifndef DMX_CONTROLLER_H
#define DMX_CONTROLLER_H

#include <Arduino.h>
#include <esp_dmx.h>
#include <Preferences.h>  // For persistent storage

// Add the DMX_INTR_FLAGS_DEFAULT definition if it's not already included
#ifndef DMX_INTR_FLAGS_DEFAULT
#define DMX_INTR_FLAGS_DEFAULT (0)
#endif

// DMX configuration
#define DMX_PACKET_SIZE 513  // DMX packet size (512 channels + start code)
#define DMX_TIMEOUT_TICK 100 // Timeout for DMX operations

// Fixture configuration structure
struct FixtureConfig {
  const char* name;
  int startAddr;
  int redChannel;
  int greenChannel;
  int blueChannel;
  int whiteChannel;
};

// Simple color structure for RGBW
struct RgbwColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
};

class DmxController {
public:
    /**
     * Constructor
     * 
     * @param dmxPort The DMX port to use
     * @param txPin The TX pin for DMX output
     * @param rxPin The RX pin for DMX input
     * @param dirPin The direction pin for DMX direction control
     */
    DmxController(
        uint8_t dmxPort = 1,
        uint8_t txPin = 19,
        uint8_t rxPin = 20,
        uint8_t dirPin = 5
    );

    /**
     * Initialize the DMX controller
     */
    bool begin();

    /**
     * Initialize the DMX controller with custom parameters
     * 
     * @param txPin The TX pin for DMX output
     * @param rxPin The RX pin for DMX input
     * @param dirPin The direction pin for DMX direction control
     * @param numChannels The number of DMX channels to use
     * @param baudRate The baud rate for DMX communication
     */
    bool begin(int txPin, int rxPin, int dirPin, int numChannels, int baudRate);

    /**
     * Set a fixture's color with direct RGBW handling
     * 
     * @param fixtureIndex Index of the fixture in the fixtures array
     * @param r Red value (0-255)
     * @param g Green value (0-255)
     * @param b Blue value (0-255)
     * @param w White value (0-255), defaults to 0
     */
    void setFixtureColor(int fixtureIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);

    /**
     * Set a fixture's color with direct RGBW handling at any address
     * 
     * @param startAddr Starting DMX address
     * @param r Red value (0-255)
     * @param g Green value (0-255)
     * @param b Blue value (0-255)
     * @param w White value (0-255), defaults to 0
     */
    void setManualFixtureColor(int startAddr, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);

    /**
     * Initialize the fixtures array with default values
     */
    void initializeFixtures(int numFixtures, int channelsPerFixture);

    /**
     * Scan through possible DMX addresses for fixtures
     */
    void scanForFixtures(int scanStartAddr, int scanEndAddr, int scanStep);

    /**
     * Run a channel test sequence to help identify fixture channels
     */
    void testAllChannels();

    /**
     * Test all fixtures with a series of color patterns
     */
    void testAllFixtures();

    /**
     * Print all fixture values for debug
     */
    void printFixtureValues();

    /**
     * Send the current DMX data to the fixtures
     */
    void sendDmxData();

    /**
     * Periodically update the DMX output by sending current data.
     * This should be called regularly in the main loop or a task.
     */
    void update();

    /**
     * Clear all DMX data (set all channels to 0)
     * Preserves the DMX start code (0 at index 0)
     */
    void clearAllChannels();

    /**
     * Create and store a new fixture configuration
     * 
     * @param index Index in fixtures array
     * @param name Fixture name
     * @param startAddr DMX start address
     * @param rChan Red channel
     * @param gChan Green channel
     * @param bChan Blue channel
     * @param wChan White channel
     */
    void setFixtureConfig(int index, const char* name, int startAddr, 
                         int rChan, int gChan, int bChan, int wChan);

    /**
     * Get the number of configured fixtures
     */
    int getNumFixtures() { return _numFixtures; }

    /**
     * Get the number of channels per fixture
     */
    int getChannelsPerFixture() { return _channelsPerFixture; }

    /**
     * Get the DMX data buffer
     */
    uint8_t* getDmxData() { return _dmxData; }

    /**
     * Get a fixture configuration
     */
    FixtureConfig* getFixture(int index);

    /**
     * Get all fixtures
     */
    FixtureConfig* getAllFixtures() { return _fixtures; }

    /**
     * Run a rainbow chase pattern across all fixtures
     */
    void runRainbowChase(int cycles = 3, int delayMs = 50, bool staggered = true);
    
    /**
     * Run a single step of the rainbow animation
     * This is useful for continuous rainbow mode in the main loop
     */
    void cycleRainbowStep(uint32_t step, bool staggered = true);
    
    /**
     * Update a single step of the rainbow animation
     * Thread-safe version for use with FreeRTOS tasks
     */
    void updateRainbowStep(uint32_t step, bool staggered = true);

    /**
     * Run a strobe test pattern on all fixtures
     * 
     * @param color Color to use for the strobe (0=white, 1=red, 2=green, 3=blue)
     * @param count Number of strobe flashes
     * @param onTimeMs Time for the "on" phase in milliseconds
     * @param offTimeMs Time for the "off" phase in milliseconds
     * @param alternate If true, alternate between fixtures for a back-and-forth effect
     */
    void runStrobeTest(uint8_t color = 0, int count = 20, int onTimeMs = 50, int offTimeMs = 50, bool alternate = false);

    /**
     * Helper function to blink an LED a specific number of times
     * 
     * @param ledPin Pin number for the LED
     * @param times Number of times to blink
     * @param delayMs Delay between blinks in milliseconds
     */
    static void blinkLED(int ledPin, int times, int delayMs);

    /**
     * Save the current DMX settings to persistent storage
     * This saves the current state of all DMX channels
     * 
     * @return True if saved successfully
     */
    bool saveSettings();

    /**
     * Load DMX settings from persistent storage
     * If no settings exist, default to white
     * 
     * @return True if settings were loaded, false if defaults were used
     */
    bool loadSettings();

    /**
     * Set all fixtures to default white color
     */
    void setDefaultWhite();

    // Add custom data storage methods
    bool saveCustomData(const char* key, uint8_t* data, size_t size);
    bool loadCustomData(const char* key, uint8_t* data, size_t size);
    bool clearCustomData(const char* key);

private:
    uint8_t _dmxPort;
    uint8_t _txPin;
    uint8_t _rxPin;
    uint8_t _dirPin;
    uint8_t _dmxData[DMX_PACKET_SIZE];  // Array to hold DMX data
    bool _isInitialized = false;        // Flag indicating if DMX is properly initialized
    Preferences _preferences;           // Preferences instance for storing settings
    
    FixtureConfig* _fixtures;  // Dynamic array of fixture configurations
    int _numFixtures;          // Number of fixtures
    int _channelsPerFixture;   // Number of channels per fixture

    // Internal counter for scanner function
    int _scanCurrentAddr;
    int _scanCurrentColor;
    
    // Helper function to convert HSV to RGB for rainbow effects
    RgbwColor hsvToRgb(uint8_t h, uint8_t s, uint8_t v);

    // Add preferences namespace for custom data
    static const char* CUSTOM_PREFS_NAMESPACE;
    Preferences customPrefs;
};

#endif // DMX_CONTROLLER_H 