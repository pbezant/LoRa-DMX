#ifndef HYBRID_LORAWAN_H
#define HYBRID_LORAWAN_H

#include <Arduino.h>
#include <RadioLib.h>
#include <Ra01S.h>

// Callback types
typedef void (*receive_callback_t)(uint8_t* data, size_t len, int16_t rssi, float snr);
typedef void (*join_callback_t)(bool success);

class HybridLoRaWAN {
public:
    /**
     * Constructor
     * 
     * @param nssPin NSS/CS pin for SPI
     * @param resetPin Reset pin for SX1262
     * @param busyPin Busy pin for SX1262
     * @param irqPin DIO1/IRQ pin for SX1262
     * @param txenPin TX enable pin (optional, not used on Heltec)
     * @param rxenPin RX enable pin (optional, not used on Heltec)
     */
    HybridLoRaWAN(int nssPin, int resetPin, int busyPin, int irqPin, int txenPin = -1, int rxenPin = -1);
    
    /**
     * Destructor
     */
    ~HybridLoRaWAN();
    
    /**
     * Initialize the LoRaWAN stack
     * 
     * @param debugPrint Enable debug printing
     * @return Success status
     */
    bool begin(bool debugPrint = false);
    
    /**
     * Set the LoRaWAN credentials for OTAA activation
     * 
     * @param devEUI Device EUI (8 bytes)
     * @param appEUI Application EUI / Join EUI (8 bytes)
     * @param appKey Application Key (16 bytes)
     */
    void setOTAACredentials(const char* devEUI, const char* appEUI, const char* appKey);
    
    /**
     * Join the LoRaWAN network using OTAA
     * 
     * @param callback Optional callback to be notified of join status
     * @return Success status
     */
    bool joinOTAA(join_callback_t callback = nullptr);
    
    /**
     * Set the receive callback
     * 
     * @param callback Function to call when data is received
     */
    void setReceiveCallback(receive_callback_t callback);
    
    /**
     * Send data to the LoRaWAN network
     * 
     * @param data Data to send
     * @param len Length of data
     * @param port FPort to use
     * @param confirmed Whether to use confirmed uplinks
     * @return Success status
     */
    bool send(uint8_t* data, size_t len, uint8_t port = 1, bool confirmed = false);
    
    /**
     * Loop function to process events
     * Must be called regularly from the main loop
     */
    void loop();
    
    /**
     * Enable Class C operation
     * This must be called after successful join
     * 
     * @return Success status
     */
    bool enableClassC();
    
    /**
     * Check if the device has joined the network
     * 
     * @return True if joined
     */
    bool isJoined() const;
    
    /**
     * Get the device address
     * 
     * @return Device address or 0 if not joined
     */
    uint32_t getDeviceAddress() const;
    
    /**
     * Test different bandwidths to find compatible ones
     * 
     * @param supportedBandwidths Array to store supported bandwidths
     * @param maxBandwidths Maximum number of bandwidths to store
     * @return Number of supported bandwidths found
     */
    int testBandwidths(float* supportedBandwidths, int maxBandwidths);

private:
    // Hardware components
    Ra01S* radio;
    RadioLib::LoRaWANNode* node;
    RadioLib::LoRaWANBand* band;
    
    // Pin configuration
    int nssPin;
    int resetPin;
    int busyPin;
    int irqPin;
    int txenPin;
    int rxenPin;
    
    // State variables
    bool initialized;
    bool joined;
    volatile bool packetReceived;
    
    // LoRaWAN credentials
    uint8_t devEuiBytes[8];
    uint8_t appEuiBytes[8];
    uint8_t appKeyBytes[16];
    
    // Callbacks
    receive_callback_t receiveCallback;
    join_callback_t joinCallback;
    
    // Working bandwidth for continuous reception
    float workingBandwidth;
    
    // IRQ handler
    static void staticIrqHandler();
    void irqHandler();
    
    // Helper functions
    void hexStringToBytes(const char* hexString, uint8_t* byteArray, size_t length);
    uint64_t bytesToEui(const uint8_t* bytes);
    bool configureRadioForRx2();
    bool processReceivedPacket();
};

#endif // HYBRID_LORAWAN_H 