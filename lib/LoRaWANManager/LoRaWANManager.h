#pragma once
#include <Arduino.h>
#include <functional>
#include <lmic.h>
#include <hal/hal.h>

// Device class types
#define LORAWAN_CLASS_A 0x0A
#define LORAWAN_CLASS_C 0x0C

// Forward declarations for friend functions
void onLmicEventCallback(void *userData, ev_t ev);
void os_getArtEui(u1_t* buf);
void os_getDevEui(u1_t* buf);
void os_getDevKey(u1_t* buf);

class LoRaWANManager {
public:
    // Define the callback type for receiving downlink messages
    typedef std::function<void(uint8_t* payload, size_t size, uint8_t port)> DownlinkCallback;
    
    // Define the callback type for event notification
    typedef std::function<void(uint8_t event)> EventCallback;

    // Define event types
    enum Event {
        EV_INIT_FAILED = 0,
        EV_INIT_SUCCESS = 1,
        EV_JOIN_STARTED = 2,
        EV_JOIN_SUCCESS = 3,
        EV_JOIN_FAILED = 4,
        EV_TX_STARTED = 5,
        EV_TX_COMPLETE = 6,
        EV_TX_FAILED = 7,
        EV_RX_RECEIVED = 8
    };

    /**
     * Get the singleton instance of the LoRaWANManager
     */
    static LoRaWANManager& getInstance();

    /**
     * Destructor - handles cleanup
     */
    ~LoRaWANManager();

    /**
     * Initialize the LoRaWAN manager with pin configuration and LoRaWAN keys
     * 
     * @param cs SPI chip select pin
     * @param dio1 LoRa radio DIO1 pin (used for DIO0 in LMIC)
     * @param rst LoRa radio reset pin
     * @param busy LoRa radio busy pin (SX126x only)
     * @param joinEUI JoinEUI (AppEUI) for OTAA
     * @param devEUI DeviceEUI for OTAA
     * @param appKeyHex Application Key as hex string
     * @param deviceClass LoRaWAN device class (A or C)
     * @param region LoRaWAN region (US915, EU868, etc.)
     * @param subBand Sub-band for regions like US915 (1-8)
     * @return true if initialization successful
     */
    bool begin(int8_t cs, int8_t dio1, int8_t rst, int8_t busy,
               const char* devEUIHex, const char* joinEUIHex,
               const char* appKeyHex,
               uint8_t deviceClass = LORAWAN_CLASS_C,
               const char* region = "US915",
               uint8_t subBand = 2);

    /**
     * Set the callback function for receiving downlink messages
     * 
     * @param cb Callback function
     */
    void setDownlinkCallback(DownlinkCallback cb);

    /**
     * Set the callback function for events
     * 
     * @param cb Callback function
     */
    void setEventCallback(EventCallback cb);

    /**
     * Join the LoRaWAN network using OTAA
     * 
     * @return true if join started successfully
     */
    bool joinNetwork();

    /**
     * Send data (raw bytes) over LoRaWAN
     * 
     * @param data Pointer to data buffer
     * @param len Length of data
     * @param port LoRaWAN port number
     * @param confirmed Whether to use confirmed uplink
     * @return true if send operation started successfully
     */
    bool sendData(const uint8_t* data, size_t len, uint8_t port = 1, bool confirmed = false);
    
    /**
     * Send a string over LoRaWAN
     * 
     * @param data String to send
     * @param port LoRaWAN port number
     * @param confirmed Whether to use confirmed uplink
     * @return true if send operation started successfully
     */
    bool sendString(const String& data, uint8_t port = 1, bool confirmed = false);

    /**
     * Handle LoRaWAN events - call this in the main loop
     */
    void handleEvents();

    /**
     * Get the last RSSI value
     * 
     * @return RSSI value in dBm
     */
    float getLastRssi() const;
    
    /**
     * Get the last SNR value
     * 
     * @return SNR value in dB
     */
    float getLastSnr() const;

    /**
     * Get current device class
     * 
     * @return Device class (LORAWAN_CLASS_A or LORAWAN_CLASS_C)
     */
    uint8_t getDeviceClass() const;

    /**
     * Check if device is joined to the network
     * 
     * @return true if joined
     */
    bool isJoined() const;

    /**
     * Check if a transmission is pending
     * 
     * @return true if transmission is pending
     */
    bool isTxPending() const;

    /**
     * Called by LMIC event handler - must be public
     * Not intended for direct use
     */
    void onLmicEvent(ev_t ev);
    
    // Make these functions friends so they can access _instance and private members
    friend void onLmicEventCallback(void *userData, ev_t ev);
    friend void os_getArtEui(u1_t* buf);
    friend void os_getDevEui(u1_t* buf);
    friend void os_getDevKey(u1_t* buf);

private:
    // Private constructor for singleton pattern
    LoRaWANManager();
    
    // Prevent copying
    LoRaWANManager(const LoRaWANManager&) = delete;
    LoRaWANManager& operator=(const LoRaWANManager&) = delete;

    // Internal state
    uint8_t _devEUI[8];       // Device EUI in byte array format
    uint8_t _joinEUI[8];      // Join EUI in byte array format
    uint8_t _appKey[16];      // App Key in byte array format
    uint8_t _deviceClass;
    bool _joined;
    float _lastRssi;
    float _lastSnr;
    DownlinkCallback _downlinkCb;
    EventCallback _eventCb;
    String _region;
    uint8_t _subBand;
    int8_t _cs;
    int8_t _dio1;
    int8_t _rst;
    int8_t _busy;

    // LMIC pin configuration
    lmic_pinmap _lmic_pins;

    // Internal flags
    bool _initDone;
    bool _txPending;
    bool _joinPending;
    bool _classSetupDone;
    unsigned long _lastEventTime;

    // Internal helpers
    bool _initLMIC();
    bool _setupChannels();
    bool _setupClass();
    void _resetRadio();
    bool _hexStringToByteArray(const char* hexString, uint8_t* output, size_t length);
    
    // Process received downlink
    void _processDownlink(uint8_t* payload, size_t size, uint8_t port);
    
    // Notify event
    void _notifyEvent(uint8_t event);

    // Static instance for singleton pattern
    static LoRaWANManager* _instance;
}; 