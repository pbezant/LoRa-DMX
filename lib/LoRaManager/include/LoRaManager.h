#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <Arduino.h>
#include <RadioLib.h>

// Define band type constants
#define BAND_TYPE_US915 1
#define BAND_TYPE_EU868 2
#define BAND_TYPE_OTHER 0

// Define a callback function type for downlink data
typedef void (*DownlinkCallback)(uint8_t* payload, size_t size, uint8_t port);

/**
 * @brief A class to manage LoRaWAN communication using RadioLib
 * 
 * This class provides a simplified interface for LoRaWAN communication,
 * handling connection establishment, data transmission and reception.
 * Default configuration uses the US915 frequency band with subband 2 (channels 8-15),
 * but this can be configured in the constructor.
 */
class LoRaManager {
public:
    /**
     * @brief Constructor with configurable frequency band (defaults to US915) and subband (defaults to 2)
     * 
     * @param freqBand The LoRaWAN frequency band to use (defaults to US915)
     * @param subBand The subband to use (defaults to 2 for US915)
     */
    LoRaManager(LoRaWANBand_t freqBand = US915, uint8_t subBand = 2);
    
    /**
     * @brief Destructor
     */
    ~LoRaManager();
    
    /**
     * @brief Singleton instance
     */
    static LoRaManager* instance;
    
    /**
     * @brief Initialize the LoRa module
     * 
     * @param pinCS CS pin
     * @param pinDIO1 DIO1 pin
     * @param pinReset Reset pin
     * @param pinBusy Busy pin
     * @return true if initialization was successful
     * @return false if initialization failed
     */
    bool begin(int8_t pinCS, int8_t pinDIO1, int8_t pinReset, int8_t pinBusy);
    
    /**
     * @brief Set the LoRaWAN credentials
     * 
     * @param joinEUI Join EUI
     * @param devEUI Device EUI
     * @param appKey Application Key
     * @param nwkKey Network Key
     */
    void setCredentials(uint64_t joinEUI, uint64_t devEUI, uint8_t* appKey, uint8_t* nwkKey);
    
    /**
     * @brief Join the LoRaWAN network
     * 
     * @return true if join was successful
     * @return false if join failed
     */
    bool joinNetwork();
    
    /**
     * @brief Send data to the LoRaWAN network
     * 
     * @param data Data to send
     * @param len Length of data
     * @param port Port to use
     * @param confirmed Whether to use confirmed transmission
     * @return true if transmission was successful
     * @return false if transmission failed
     */
    bool sendData(uint8_t* data, size_t len, uint8_t port = 1, bool confirmed = false);
    
    /**
     * @brief Send a string to the LoRaWAN network
     * 
     * @param data String to send
     * @param port Port to use
     * @return true if transmission was successful
     * @return false if transmission failed
     */
    bool sendString(const String& data, uint8_t port = 1);
    
    /**
     * @brief Get the last RSSI value
     * 
     * @return float RSSI value
     */
    float getLastRssi();
    
    /**
     * @brief Get the last SNR value
     * 
     * @return float SNR value
     */
    float getLastSnr();
    
    /**
     * @brief Check if the device is joined to the network
     * 
     * @return true if joined
     * @return false if not joined
     */
    bool isNetworkJoined();
    
    /**
     * @brief Handle events (should be called in the loop)
     */
    void handleEvents();
    
    /**
     * @brief Get the last error from LoRaWAN operations
     * 
     * @return int Error code
     */
    int getLastErrorCode();
    
    /**
     * @brief Get the frequency band type (US915, EU868, or OTHER)
     * 
     * @return uint8_t Band type constant (BAND_TYPE_US915, BAND_TYPE_EU868, or BAND_TYPE_OTHER)
     */
    uint8_t getBandType();
    
    /**
     * @brief Set the callback function for downlink data
     * 
     * @param callback Pointer to the callback function
     */
    void setDownlinkCallback(DownlinkCallback callback);
    
private:
    // Radio module and LoRaWAN node
    SX1262* radio;
    LoRaWANNode* node;
    
    // LoRaWAN credentials
    uint64_t joinEUI;
    uint64_t devEUI;
    uint8_t appKey[16];
    uint8_t nwkKey[16];
    
    // Frequency band and subband configuration
    LoRaWANBand_t freqBand;
    uint8_t subBand;
    
    // Status variables
    bool isJoined;
    float lastRssi;
    float lastSnr;
    uint8_t consecutiveTransmitErrors;
    
    // Receive buffer
    uint8_t receivedData[256];
    size_t receivedBytes;
    
    // Error handling
    int lastErrorCode;
    
    // Downlink callback
    DownlinkCallback downlinkCallback;
    
    /**
     * @brief Configure subband channel mask based on the current subband
     * 
     * @param targetSubBand The subband to configure (1-8)
     * @return int Result code from setupChannelsDyn
     */
    int configureSubbandChannels(uint8_t targetSubBand);
};

#endif // LORA_MANAGER_H 