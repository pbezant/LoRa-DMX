#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <Arduino.h>
#include <RadioLib.h>

// Define band type constants
#define BAND_TYPE_US915 1
#define BAND_TYPE_EU868 2
#define BAND_TYPE_OTHER 0

// Define device class types
#define DEVICE_CLASS_A 'A'
#define DEVICE_CLASS_B 'B'
#define DEVICE_CLASS_C 'C'

// Define beacon states
#define BEACON_STATE_IDLE 0
#define BEACON_STATE_ACQUISITION 1
#define BEACON_STATE_LOCKED 2
#define BEACON_STATE_LOST 3

// Define a callback function type for downlink data
// In RadioLib 7.x, port is now fPort in the LoRaWANEvent_t structure
typedef void (*DownlinkCallback)(uint8_t* payload, size_t size, uint8_t port);

// Define a callback function type for beacon reception
typedef void (*BeaconCallback)(uint8_t* payload, size_t size, float rssi, float snr);

/**
 * @brief A class to manage LoRaWAN communication using RadioLib
 * 
 * This class provides a simplified interface for LoRaWAN communication,
 * handling connection establishment, data transmission and reception.
 * Default configuration uses the US915 frequency band with subband 2 (channels 8-15),
 * but this can be configured in the constructor.
 * Supports Class A, B, and C device operation modes.
 */
class LoRaManager {
public:
    /**
     * @brief Construct a new LoRaManager object
     * 
     * @param freqBand Frequency band to use (EU868 or US915)
     * @param subBand Subband to use (1-8, only for US915)
     */
    LoRaManager(LoRaWANBand_t freqBand = US915, uint8_t subBand = 2);
    
    /**
     * @brief Destroy the LoRaManager object
     */
    ~LoRaManager();
    
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
     * @param joinEUI Join EUI (AppEUI in v1.0.x)
     * @param devEUI Device EUI
     * @param appKey Application Key
     * @param nwkKey Network Key (same as AppKey in v1.0.x)
     */
    void setCredentials(uint64_t joinEUI, uint64_t devEUI, uint8_t* appKey, uint8_t* nwkKey);
    
    /**
     * @brief Set the LoRaWAN credentials using hex strings for keys
     * 
     * @param joinEUI Join EUI (AppEUI in v1.0.x)
     * @param devEUI Device EUI
     * @param appKeyHex Application Key as hex string
     * @param nwkKeyHex Network Key as hex string (same as AppKey in v1.0.x)
     * @return true if the credentials were set successfully
     * @return false if there was an error in the hex strings
     */
    bool setCredentialsHex(uint64_t joinEUI, uint64_t devEUI, const String& appKeyHex, const String& nwkKeyHex);
    
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
     * @param data Pointer to the data to send
     * @param len Length of the data
     * @param port Port to use
     * @param confirmed Whether the transmission should be confirmed
     * @return true if transmission was successful
     * @return false if transmission failed
     */
    bool sendData(uint8_t* data, size_t len, uint8_t port = 1, bool confirmed = false);
    
    /**
     * @brief Send a string to the LoRaWAN network
     * 
     * @param data String to send
     * @param port Port to use
     * @param confirmed Whether the transmission should be confirmed
     * @return true if transmission was successful
     * @return false if transmission failed
     */
    bool sendString(const String& data, uint8_t port = 1, bool confirmed = false);
    
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
     * 
     * In RadioLib 7.x, this doesn't poll for incoming data,
     * but handles internal state management.
     */
    void handleEvents();
    
    /**
     * @brief Get the last error from LoRaWAN operations
     * 
     * @return int Error code
     */
    int getLastErrorCode();
    
    /**
     * @brief Get the current band type
     * 
     * @return uint8_t The current band type (BAND_TYPE_US915, BAND_TYPE_EU868, or BAND_TYPE_OTHER)
     */
    uint8_t getBandType() const;
    
    /**
     * @brief Set the callback function for downlink data
     * 
     * @param callback Pointer to the callback function
     */
    void setDownlinkCallback(DownlinkCallback callback);
    
    /**
     * @brief Get the RX1 delay
     * 
     * @return RX1 delay in seconds
     */
    int getRx1Delay() const;
    
    /**
     * @brief Get the RX1 window timeout
     * 
     * @return RX1 timeout in milliseconds
     */
    int getRx1Timeout() const;
    
    /**
     * @brief Get the RX2 window timeout
     * 
     * @return RX2 timeout in milliseconds
     */
    int getRx2Timeout() const;

    /**
     * @brief Set the device class (A, B, or C)
     * 
     * @param deviceClass The device class to set (DEVICE_CLASS_A, DEVICE_CLASS_B, or DEVICE_CLASS_C)
     * @return true if the class change was successful
     * @return false if the class change failed
     */
    bool setDeviceClass(char deviceClass);
    
    /**
     * @brief Get the current device class
     * 
     * @return char The current device class (DEVICE_CLASS_A, DEVICE_CLASS_B, or DEVICE_CLASS_C)
     */
    char getDeviceClass() const;
    
    /**
     * @brief Start beacon acquisition (Class B)
     * 
     * @return true if beacon acquisition started successfully
     * @return false if beacon acquisition failed to start
     */
    bool startBeaconAcquisition();
    
    /**
     * @brief Stop beacon acquisition (Class B)
     */
    void stopBeaconAcquisition();
    
    /**
     * @brief Set the ping slot periodicity (Class B)
     * 
     * @param periodicity The ping slot periodicity (0-7)
     * @return true if the periodicity was set successfully
     * @return false if setting the periodicity failed
     */
    bool setPingSlotPeriodicity(uint8_t periodicity);
    
    /**
     * @brief Get the current ping slot periodicity (Class B)
     * 
     * @return uint8_t The current ping slot periodicity
     */
    uint8_t getPingSlotPeriodicity() const;
    
    /**
     * @brief Get the current beacon state (Class B)
     * 
     * @return uint8_t The current beacon state (BEACON_STATE_*)
     */
    uint8_t getBeaconState() const;
    
    /**
     * @brief Set the callback function for beacon reception (Class B)
     * 
     * @param callback Pointer to the callback function
     */
    void setBeaconCallback(BeaconCallback callback);
    
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
    
    // Band type
    uint8_t bandType;

    // Class B/C specific variables
    char deviceClass;
    uint8_t beaconState;
    uint8_t pingSlotPeriodicity;
    BeaconCallback beaconCallback;
    unsigned long lastBeaconTimestamp;
    bool continuousReception;
    
    // Class B/C timers and state
    unsigned long nextPingSlotTime;
    unsigned long beaconPeriod;
    unsigned long lastBeaconRxTime;
    
    /**
     * @brief Configure subband channel mask based on the current subband
     * 
     * @param targetSubBand The subband to configure (1-8)
     * @return int Result code from setup
     */
    int configureSubbandChannels(uint8_t targetSubBand);
    
    /**
     * @brief Convert hex string to byte array
     * 
     * @param hexString Hex string to convert (e.g. "F30A2F42EAEA8DE5D796A22DBBC86908")
     * @param result Byte array to store the result (must be pre-allocated)
     * @param resultLen Length of the result array
     * @return true if conversion was successful
     * @return false if conversion failed
     */
    bool hexStringToByteArray(const String& hexString, uint8_t* result, size_t resultLen);
    
    /**
     * @brief Handle beacon reception (Class B)
     * 
     * @param payload Beacon payload
     * @param size Size of the payload
     * @param rssi RSSI of the beacon
     * @param snr SNR of the beacon
     */
    void handleBeaconReception(uint8_t* payload, size_t size, float rssi, float snr);
    
    /**
     * @brief Calculate next ping slot time (Class B)
     */
    void calculateNextPingSlot();
    
    /**
     * @brief Start continuous reception (Class C)
     * 
     * @return true if continuous reception was started successfully
     * @return false if starting continuous reception failed
     */
    bool startContinuousReception();
    
    /**
     * @brief Stop continuous reception (Class C)
     */
    void stopContinuousReception();
    
    // Singleton instance (to help with callbacks)
    static LoRaManager* instance;
};

#endif // LORA_MANAGER_H 