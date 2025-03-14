/**
 * DmxController.h - A library for controlling DMX fixtures with ESP32
 * 
 * This library provides functionality for controlling RGB DMX fixtures,
 * including setup, color control, and data transmission.
 * 
 * Compatible with esp_dmx 4.1.0 and newer
 */

#ifndef DMX_CONTROLLER_H
#define DMX_CONTROLLER_H

#include <Arduino.h>
#include <esp_dmx.h>

// Add the DMX_INTR_FLAGS_DEFAULT definition if it's not already included
#ifndef DMX_INTR_FLAGS_DEFAULT
#define DMX_INTR_FLAGS_DEFAULT (0)
#endif

// DMX color values
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} Color;

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
    void begin();

    /**
     * Set a fixture's RGB color
     * 
     * @param address The DMX address of the fixture
     * @param color The RGB color to set
     */
    void setFixtureColor(int address, Color color);

    /**
     * Send the current DMX data to the fixtures
     */
    void sendData();

    /**
     * Get a debug-friendly string of current DMX values for a fixture
     * 
     * @param fixtureLabel The label to use for the fixture
     * @param address The DMX address of the fixture
     * @return A formatted string with the fixture's RGB values
     */
    String getDmxValueString(const String& fixtureLabel, int address);
    
    /**
     * Clear all DMX data (set all channels to 0)
     * Preserves the DMX start code (0 at index 0)
     */
    void clearAllChannels();
    
    /**
     * Log all DMX values for debugging
     * 
     * @param fixture1Label Label for the first fixture
     * @param fixture1Addr Address of the first fixture
     * @param fixture2Label Label for the second fixture (optional)
     * @param fixture2Addr Address of the second fixture (optional)
     */
    void logDmxValues(const String& fixture1Label, int fixture1Addr, 
                      const String& fixture2Label = "", int fixture2Addr = 0);
    
    /**
     * Convert HSV color to RGB color
     * 
     * @param h Hue (0.0 - 1.0)
     * @param s Saturation (0.0 - 1.0)
     * @param v Value (0.0 - 1.0)
     * @param r Red output value (0.0 - 1.0)
     * @param g Green output value (0.0 - 1.0)
     * @param b Blue output value (0.0 - 1.0)
     */
    static void HSVtoRGB(float h, float s, float v, float &r, float &g, float &b);
    
    /**
     * Get standard colors array
     * 
     * @return Array of standard RGB colors
     */
    static Color* getStandardColors(int &numColors);
    
    /**
     * Get the DMX data buffer
     * 
     * @return Pointer to the DMX data buffer
     */
    uint8_t* getDmxData() { return _dmxData; }
    
    /**
     * Set up color cycling for two fixtures
     * 
     * @param fixture1Addr Address of the first fixture
     * @param fixture2Addr Address of the second fixture
     * @param useStandardColors True to use standard colors, false for smooth HSV transitions
     * @param colorIndex Current color index (will be incremented by this function)
     * @param offset Color index offset for the second fixture
     */
    void cycleColors(int fixture1Addr, int fixture2Addr, bool useStandardColors, 
                     int &colorIndex, int offset = 3);
    
    /**
     * Update a fixture's color based on HSV values for smooth transitions
     * 
     * @param address The DMX address of the fixture
     * @param hue Hue value (0.0 - 1.0)
     */
    void setFixtureColorHSV(int address, float hue, float saturation = 1.0, float value = 1.0);
    
    /**
     * Update multiple fixtures with the same color
     * 
     * @param addresses Array of fixture addresses
     * @param numFixtures Number of fixtures in the array
     * @param color The RGB color to set for all fixtures
     */
    void setMultipleFixtureColors(const int addresses[], int numFixtures, Color color);

private:
    uint8_t _dmxPort;
    uint8_t _txPin;
    uint8_t _rxPin;
    uint8_t _dirPin;
    uint8_t _dmxData[DMX_PACKET_SIZE];  // Array to hold DMX data
    
    // Helper function to log a message (you'll need to provide your own implementation)
    void logMessage(const String& message);
};

#endif // DMX_CONTROLLER_H 