#include "LoRaManager.h"
#include <RadioLib.h>

// Define error codes that are not already defined in RadioLib
// or use the ones from RadioLib directly
// We'll keep these for backward compatibility with existing code
#ifndef RADIOLIB_ERR_INVALID_STATE
#define RADIOLIB_ERR_INVALID_STATE             (-1)
#endif

#ifndef RADIOLIB_ERR_INVALID_INPUT
#define RADIOLIB_ERR_INVALID_INPUT             (-3)
#endif

// Use RadioLib's error codes instead of redefining them
// RADIOLIB_ERR_NETWORK_NOT_JOINED is already defined in RadioLib
// RADIOLIB_ERR_INVALID_FREQUENCY is already defined in RadioLib
// RADIOLIB_LORAWAN_NEW_SESSION is already defined in RadioLib

#ifndef RADIOLIB_ERR_NO_CHANNEL_AVAILABLE
#define RADIOLIB_ERR_NO_CHANNEL_AVAILABLE      (-1106)
#endif

#ifndef RADIOLIB_LORAWAN_NO_DOWNLINK
#define RADIOLIB_LORAWAN_NO_DOWNLINK           (-5)
#endif

// Define error codes for Class B and C operations
#ifndef RADIOLIB_ERR_BEACON_NOT_RECEIVED
#define RADIOLIB_ERR_BEACON_NOT_RECEIVED      (-2000)
#endif

#ifndef RADIOLIB_ERR_BEACON_ACQUISITION_FAILED
#define RADIOLIB_ERR_BEACON_ACQUISITION_FAILED (-2001)
#endif

#ifndef RADIOLIB_ERR_CLASS_NOT_SUPPORTED
#define RADIOLIB_ERR_CLASS_NOT_SUPPORTED      (-2002)
#endif

// Initialize static instance pointer
LoRaManager* LoRaManager::instance = nullptr;

// Constructor with configurable frequency band and subband
LoRaManager::LoRaManager(LoRaWANBand_t freqBand, uint8_t subBand) : 
  radio(nullptr),
  node(nullptr),
  joinEUI(0),
  devEUI(0),
  freqBand(freqBand),
  subBand(subBand),
  isJoined(false),
  lastRssi(0),
  lastSnr(0),
  receivedBytes(0),
  lastErrorCode(RADIOLIB_ERR_NONE),
  consecutiveTransmitErrors(0),
  downlinkCallback(nullptr),
  deviceClass(DEVICE_CLASS_A),
  beaconState(BEACON_STATE_IDLE),
  pingSlotPeriodicity(0),
  beaconCallback(nullptr),
  lastBeaconTimestamp(0),
  continuousReception(false),
  nextPingSlotTime(0),
  beaconPeriod(128000), // 128 seconds (standard LoRaWAN beacon period)
  lastBeaconRxTime(0) {
  
  // Set this instance as the active one
  instance = this;
  
  // Initialize arrays
  memset(appKey, 0, sizeof(appKey));
  memset(nwkKey, 0, sizeof(nwkKey));
  memset(receivedData, 0, sizeof(receivedData));
  
  // Log selected frequency band using bandNum instead of name
  Serial.print(F("[LoRaManager] Selected frequency band: "));
  Serial.println(freqBand.bandNum);
  
  Serial.print(F("[LoRaManager] Selected subband: "));
  Serial.println(subBand);
}

// Destructor
LoRaManager::~LoRaManager() {
  // Clean up allocated resources
  if (node != nullptr) {
    delete node;
    node = nullptr;
  }
  
  if (radio != nullptr) {
    delete radio;
    radio = nullptr;
  }
  
  // Clear the instance pointer
  if (instance == this) {
    instance = nullptr;
  }
}

// Get band type based on band number
uint8_t LoRaManager::getBandType() const {
  // Check band number instead of name
  if (freqBand.bandNum == 0) {
    return BAND_TYPE_OTHER;
  }
  
  // Use band number to determine type
  // US915 is typically band number 2
  if (freqBand.bandNum == 2) {
    return BAND_TYPE_US915;
  } 
  // EU868 is typically band number 1
  else if (freqBand.bandNum == 1) {
    return BAND_TYPE_EU868;
  }
  
  return BAND_TYPE_OTHER;
}

// Initialize the LoRa module
bool LoRaManager::begin(int8_t pinCS, int8_t pinDIO1, int8_t pinReset, int8_t pinBusy) {
  // Initialize the SX1262 module
  Serial.println(F("[LoRaManager] Initializing radio module..."));
  
  // Check if a previous instance exists
  if (radio != nullptr) {
    delete radio;
    radio = nullptr;
  }
  
  if (node != nullptr) {
    delete node;
    node = nullptr;
  }
  
  // Create a new Module instance - required for RadioLib 7.x
  Module* module = new Module(pinCS, pinDIO1, pinReset, pinBusy);
  
  // Create a new instance of the radio using the module
  radio = new SX1262(module);
  if (radio == nullptr) {
    Serial.println(F("[LoRaManager] Failed to create radio instance"));
    delete module; // Clean up the module if we can't create the radio
    return false;
  }
  
  // Initialize the radio
  int state = radio->begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[LoRaManager] Failed to initialize radio: "));
    Serial.println(state);
    lastErrorCode = state;
    return false;
  }
  
  Serial.println(F("[LoRaManager] Radio initialized successfully"));
  
  // Create a node instance with the specified band
  node = new LoRaWANNode(radio, &freqBand, subBand);
  if (node == nullptr) {
    Serial.println(F("[LoRaManager] Failed to create LoRaWAN node instance"));
    return false;
  }
  
  // Configure the subbands based on the selected frequency plan
  int result = configureSubbandChannels(subBand);
  if (result != RADIOLIB_ERR_NONE) {
    Serial.print(F("[LoRaManager] Warning: Failed to configure subband channels: "));
    Serial.println(result);
    // This is a warning, not a fatal error, so we'll continue
  }
  
  Serial.println(F("[LoRaManager] LoRaWAN node initialized successfully"));
  
  return true;
}

// Configure subband channel mask based on the current subband
int LoRaManager::configureSubbandChannels(uint8_t targetSubBand) {
  if (!node) {
    Serial.println(F("[LoRaWAN] Node not initialized"));
    return RADIOLIB_ERR_INVALID_STATE;
  }
  
  // Only applicable for US915
  uint8_t bandType = getBandType();
  if (bandType != BAND_TYPE_US915) {
    Serial.println(F("[LoRaWAN] Subband configuration only applies to US915"));
    return RADIOLIB_ERR_NONE;
  }
  
  // Validate subband (1-8)
  if (targetSubBand < 1 || targetSubBand > 8) {
    Serial.println(F("[LoRaWAN] Invalid subband, must be 1-8"));
    return RADIOLIB_ERR_INVALID_INPUT;
  }
  
  Serial.print(F("[LoRaWAN] Subband configuration is handled automatically during initialization"));
  Serial.print(F(" for subband "));
  Serial.println(targetSubBand);
  
  // In RadioLib 7.1.2, the subband is configured during node initialization
  // and channel selection is handled internally, so we don't need to do anything here
  return RADIOLIB_ERR_NONE;
}

// Convert hex string to byte array
bool LoRaManager::hexStringToByteArray(const String& hexString, uint8_t* result, size_t resultLen) {
  // Check if the string length is valid (2 chars per byte)
  if (hexString.length() != resultLen * 2) {
    Serial.println(F("[LoRaManager] Invalid hex string length"));
    return false;
  }
  
  // Convert each pair of hex chars to a byte
  for (size_t i = 0; i < resultLen; i++) {
    // Get the two hex characters
    String byteStr = hexString.substring(i * 2, i * 2 + 2);
    
    // Convert to integer
    char* endPtr;
    uint8_t byteVal = strtol(byteStr.c_str(), &endPtr, 16);
    
    // Check if conversion was successful
    if (*endPtr != '\0') {
      Serial.println(F("[LoRaManager] Invalid hex character in string"));
      return false;
    }
    
    // Store the byte value
    result[i] = byteVal;
  }
  
  return true;
}

// Set the LoRaWAN credentials
void LoRaManager::setCredentials(uint64_t joinEUI, uint64_t devEUI, uint8_t* appKey, uint8_t* nwkKey) {
  this->joinEUI = joinEUI;
  this->devEUI = devEUI;
  
  // Copy the keys
  memcpy(this->appKey, appKey, 16);
  memcpy(this->nwkKey, nwkKey, 16);
}

// Set the LoRaWAN credentials using hex strings for keys
bool LoRaManager::setCredentialsHex(uint64_t joinEUI, uint64_t devEUI, const String& appKeyHex, const String& nwkKeyHex) {
  this->joinEUI = joinEUI;
  this->devEUI = devEUI;
  
  // Convert hex strings to byte arrays
  bool appKeyResult = hexStringToByteArray(appKeyHex, this->appKey, 16);
  bool nwkKeyResult = hexStringToByteArray(nwkKeyHex, this->nwkKey, 16);
  
  // Return true only if both conversions were successful
  return appKeyResult && nwkKeyResult;
}

// Set the callback function for downlink data
void LoRaManager::setDownlinkCallback(DownlinkCallback callback) {
  this->downlinkCallback = callback;
  Serial.println(F("[LoRaManager] Downlink callback registered"));
}

// Join the LoRaWAN network
bool LoRaManager::joinNetwork() {
  if (node == nullptr) {
    Serial.println(F("[LoRaWAN] Node not initialized!"));
    lastErrorCode = RADIOLIB_ERR_INVALID_STATE;
    return false;
  }
  
  // Maximum number of join attempts
  const uint8_t maxAttempts = 5;
  uint8_t attemptCount = 0;
  
  // Initial backoff delay in milliseconds
  uint16_t backoffDelay = 1000;
  
  // Try multiple times with exponential backoff
  while (attemptCount < maxAttempts) {
    // Increment attempt counter
    attemptCount++;
    
    // Attempt to join the network
    Serial.print(F("[LoRaWAN] Attempting over-the-air activation (attempt "));
    Serial.print(attemptCount);
    Serial.print(F(" of "));
    Serial.print(maxAttempts);
    Serial.print(F(") ... "));
    
    // Set the proper credentials before activation
    node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    
    // Select a subband based on the attempt number
    uint8_t currentSubBand = attemptCount == 1 ? subBand : (1 + (attemptCount % 8)); // Start with configured subband, then try others
    
    // Configure channels for the selected subband (US915 only)
    uint8_t bandType = getBandType();
    if (bandType == BAND_TYPE_US915) {
      int maskResult = configureSubbandChannels(currentSubBand);
      
      // If we couldn't set the channel mask, try the next attempt
      if (maskResult != RADIOLIB_ERR_NONE) {
        Serial.println(F("[LoRaWAN] Continuing with default channel configuration"));
      }
    }

    // Try to join the network
    int state = node->activateOTAA();
    lastErrorCode = state;
    
    // Check for successful join or new session status
    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      // Successfully joined
      isJoined = true;
      
      // Configure the data rate for reliability
      Serial.println(F("[LoRaWAN] Setting data rate to DR1 for reliability"));
      node->setDatarate(1);
      
      // Reset frame counters to ensure a clean session
      node->resetFCntDown();
      
      // Send an initial small packet to confirm the join and establish the session fully
      uint8_t testData[] = {0x01};
      int sendState = node->sendReceive(testData, sizeof(testData), 1);
      
      if (sendState == RADIOLIB_ERR_NONE || sendState > 0) {
        // Successfully sent the initial packet and potentially received a downlink
        Serial.println(F("success! (new session started)"));
        return true;
      } else {
        // Session started but initial message failed
        Serial.println(F("session started but initial message failed, code "));
        Serial.println(sendState);
        // We'll still consider this a success since the join worked
        return true;
      }
    } else {
      // Join attempt failed
      Serial.print(F("failed, code "));
      Serial.println(state);
      
      if (state == RADIOLIB_ERR_NETWORK_NOT_JOINED) {
        // Node rejected by network - try a different subband or wait longer
        Serial.println(F("[LoRaWAN] Rejected by network. Will try again with different parameters."));
      } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
        // Signal problem - check antenna, power, or location
        Serial.println(F("[LoRaWAN] Transmission timeout. Check antenna, signal strength, or move to better location."));
      } else {
        // Other error, print for debugging
        Serial.print(F("[LoRaWAN] Error code: "));
        Serial.println(state);
      }
      
      // Wait a bit before the next attempt (with exponential backoff)
      delay(backoffDelay);
      backoffDelay *= 2;  // Exponential backoff
      
      // Cap the backoff delay at 30 seconds
      if (backoffDelay > 30000) {
        backoffDelay = 30000;
      }
    }
  }
  
  // If we got here, all attempts failed
  isJoined = false;
  lastErrorCode = RADIOLIB_ERR_NETWORK_NOT_JOINED;
  Serial.println(F("[LoRaWAN] Failed to join after maximum attempts."));
  return false;
}

// Send data to the LoRaWAN network
bool LoRaManager::sendData(uint8_t* data, size_t len, uint8_t port, bool confirmed) {
  if (!isJoined) {
    Serial.println(F("[LoRaWAN] Not joined to the network"));
    lastErrorCode = RADIOLIB_ERR_NETWORK_NOT_JOINED;
    return false;
  }
  
  if (!node) {
    Serial.println(F("[LoRaWAN] Node not initialized"));
    lastErrorCode = RADIOLIB_ERR_INVALID_STATE;
    return false;
  }
  
  // Set maximum transmit attempts
  int maxAttempts = confirmed ? 3 : 1;
  int attemptCount = 0;
  bool shouldRetry = false;
  
  // Reset the consecutive transmit error counter if we succeed
  bool success = false;
  
  while (attemptCount < maxAttempts) {
    // Increment attempt counter
    attemptCount++;
    
    // Send the data
    Serial.print(F("[LoRaWAN] Sending data (attempt "));
    Serial.print(attemptCount);
    Serial.print(F(" of "));
    Serial.print(maxAttempts);
    Serial.print(F(") ... "));
    
    // Prepare buffer for downlink
    uint8_t downlinkData[256];
    size_t downlinkLen = sizeof(downlinkData);
    
    // Create event structures for uplink and downlink details
    LoRaWANEvent_t uplinkEvent;
    LoRaWANEvent_t downlinkEvent;
    
    // Send data and wait for downlink using the updated API
    int state = node->sendReceive(data, len, port, downlinkData, &downlinkLen, confirmed, &uplinkEvent, &downlinkEvent);
    lastErrorCode = state;
    
    // Check for successful transmission
    if (state >= RADIOLIB_ERR_NONE) {
      if (state > 0) {  // Downlink received in window 1 or 2
        Serial.print(F("success! Received downlink in RX"));
        Serial.println(state);
        
        // Process the downlink data
        if (downlinkLen > 0) {
          Serial.print(F("[LoRaWAN] Received "));
          Serial.print(downlinkLen);
          Serial.println(F(" bytes"));
          
          for (size_t i = 0; i < downlinkLen; i++) {
            Serial.print(downlinkData[i], HEX);
            Serial.print(' ');
          }
          Serial.println();
          
          // Call the callback if registered
          if (downlinkCallback != nullptr) {
            downlinkCallback(downlinkData, downlinkLen, downlinkEvent.fPort);
          }
          
          // Copy the data to our buffer
          memcpy(receivedData, downlinkData, downlinkLen);
          receivedBytes = downlinkLen;
        }
      } else if (state == RADIOLIB_ERR_NONE) {
        // No downlink received but uplink was successful
        Serial.println(F("success! No downlink received."));
      }
      
      // Get RSSI and SNR
      lastRssi = radio->getRSSI();
      lastSnr = radio->getSNR();
      
      // Print the RSSI and SNR of the uplink
      Serial.print(F("[LoRaWAN] RSSI: "));
      Serial.print(lastRssi);
      Serial.print(F(" dBm, SNR: "));
      Serial.print(lastSnr);
      Serial.println(F(" dB"));
      
      // Transmission was successful, reset the error counter
      consecutiveTransmitErrors = 0;
      success = true;
      
      // Break out of the retry loop since we succeeded
      break;
    } else {
      // Transmission failed
      Serial.print(F("failed! Error code: "));
      Serial.println(state);
      
      // Increment consecutive transmit error counter
      consecutiveTransmitErrors++;
      
      // In RadioLib 7.x, we can't use RADIOLIB_ERR_INVALID_ADR_ACK_LIMIT
      // Instead, just retry confirmed messages
      shouldRetry = confirmed;
      
      if (shouldRetry && attemptCount < maxAttempts) {
        Serial.print(F("[LoRaWAN] Will retry transmission in 3 seconds (attempt "));
        Serial.print(attemptCount + 1);
        Serial.print(F(" of "));
        Serial.print(maxAttempts);
        Serial.println(F(")"));
        
        // Wait before next attempt
        delay(3000);
      } else {
        // If we've encountered errors multiple times in a row, try rejoining on next transmission
        if (consecutiveTransmitErrors >= 3) {
          Serial.println(F("[LoRaWAN] Multiple transmission errors, will attempt to rejoin on next transmission."));
          isJoined = false; // Force a rejoin on next transmission
        }
        
        return false;
      }
    }
  }
  
  // If we've reached this point and success is still false, all attempts failed
  if (!success) {
    Serial.println(F("[LoRaWAN] All transmission attempts failed."));
    return false;
  }
  
  return true;
}

// Helper method to send a string
bool LoRaManager::sendString(const String& data, uint8_t port, bool confirmed) {
  return sendData((uint8_t*)data.c_str(), data.length(), port, confirmed);
}

// Get the last RSSI value
float LoRaManager::getLastRssi() {
  return lastRssi;
}

// Get the last SNR value
float LoRaManager::getLastSnr() {
  return lastSnr;
}

// Check if the device is joined to the network
bool LoRaManager::isNetworkJoined() {
  return isJoined;
}

// Get information about RX1 delay (time between uplink end and RX1 window opening)
int LoRaManager::getRx1Delay() const {
  // Default is 1 second for most networks
  return 5; // This is usually configured by the network on join
}

// Get information about RX1 window timeout
int LoRaManager::getRx1Timeout() const {
  // Return the configured timeout, typically around 50ms
  return 50; // This is the default for Class A devices
}

// Get information about RX2 window timeout
int LoRaManager::getRx2Timeout() const {
  // Return the configured timeout, typically around 190ms
  return 190; // This is the default for Class A devices
}

// Handle events (should be called in the loop)
void LoRaManager::handleEvents() {
  if (!node) {
    return;
  }
  
  // For RadioLib 7.x, Class C implementation is different
  // Class C reception now happens after normal transmission windows
  if (deviceClass == DEVICE_CLASS_C && isJoined) {
    // For Class C, just log the continuous reception mode periodically
    static unsigned long lastClassCLog = 0;
    if (millis() - lastClassCLog > 60000) { // Log every minute
      lastClassCLog = millis();
      Serial.println(F("[LoRaManager] Class C: Continuous reception mode active"));
    }
  }
  
  // Check for network status
  if (!isJoined && node != nullptr) {
    // Check if we've become joined since last check
    if (node->isActivated()) {
      isJoined = true;
      Serial.println(F("[LoRaManager] Join successful!"));
    }
  }
  
  // Note: In RadioLib 7.x, events are primarily handled through the sendReceive method
  // and are reported via the event structures passed to that method.
  // The old polling approach with check() and readEvent() is no longer supported.
}

// Set the device class (A, B, or C)
bool LoRaManager::setDeviceClass(char deviceClass) {
  if (deviceClass != DEVICE_CLASS_A && deviceClass != DEVICE_CLASS_B && deviceClass != DEVICE_CLASS_C) {
    Serial.println(F("[LoRaWAN] Invalid device class"));
    lastErrorCode = RADIOLIB_ERR_INVALID_INPUT;
    return false;
  }
  
  // If we're already in this class, just return success
  if (this->deviceClass == deviceClass) {
    return true;
  }
  
  // Handle class transitions
  char previousClass = this->deviceClass;
  this->deviceClass = deviceClass;
  
  Serial.print(F("[LoRaWAN] Switching from Class "));
  Serial.print(previousClass);
  Serial.print(F(" to Class "));
  Serial.println(deviceClass);
  
  // Handle specific transitions
  if (deviceClass == DEVICE_CLASS_A) {
    // When switching to Class A, stop any Class B or C specific operations
    if (previousClass == DEVICE_CLASS_B) {
      stopBeaconAcquisition();
    } else if (previousClass == DEVICE_CLASS_C) {
      stopContinuousReception();
    }
    
    Serial.println(F("[LoRaWAN] Switched to Class A mode"));
    return true;
  } else if (deviceClass == DEVICE_CLASS_B) {
    // When switching to Class B, start beacon acquisition
    if (previousClass == DEVICE_CLASS_C) {
      stopContinuousReception();
    }
    
    return startBeaconAcquisition();
  } else if (deviceClass == DEVICE_CLASS_C) {
    // When switching to Class C, start continuous reception
    if (previousClass == DEVICE_CLASS_B) {
      stopBeaconAcquisition();
    }
    
    return startContinuousReception();
  }
  
  // We should never reach here
  return false;
}

// Get the current device class
char LoRaManager::getDeviceClass() const {
  return deviceClass;
}

// Start beacon acquisition (Class B)
bool LoRaManager::startBeaconAcquisition() {
  if (node == nullptr || radio == nullptr) {
    Serial.println(F("[LoRaWAN] Node or radio not initialized"));
    lastErrorCode = RADIOLIB_ERR_INVALID_STATE;
    return false;
  }
  
  if (!isJoined) {
    Serial.println(F("[LoRaWAN] Must be joined to the network before starting beacon acquisition"));
    lastErrorCode = RADIOLIB_ERR_NETWORK_NOT_JOINED;
    return false;
  }
  
  Serial.println(F("[LoRaWAN] Starting beacon acquisition for Class B operation"));
  
  // In a real implementation, we would:
  // 1. Configure the radio for beacon reception frequency
  // 2. Calculate the next beacon time based on GPS time or network time
  // 3. Schedule beacon reception
  
  // For this example, we'll just set the state and handle the details in handleEvents
  beaconState = BEACON_STATE_ACQUISITION;
  lastBeaconRxTime = millis();
  
  // Inform the network server that we're switching to Class B
  // by sending a specific uplink message (not implemented in this example)
  
  return true;
}

// Stop beacon acquisition (Class B)
void LoRaManager::stopBeaconAcquisition() {
  if (beaconState != BEACON_STATE_IDLE) {
    Serial.println(F("[LoRaWAN] Stopping beacon acquisition/tracking"));
    beaconState = BEACON_STATE_IDLE;
    
    // Inform the network server that we're no longer in Class B mode
    // by sending a specific uplink message (not implemented in this example)
  }
}

// Set the ping slot periodicity (Class B)
bool LoRaManager::setPingSlotPeriodicity(uint8_t periodicity) {
  if (periodicity > 7) {
    Serial.println(F("[LoRaWAN] Invalid ping slot periodicity (must be 0-7)"));
    lastErrorCode = RADIOLIB_ERR_INVALID_INPUT;
    return false;
  }
  
  pingSlotPeriodicity = periodicity;
  Serial.print(F("[LoRaWAN] Ping slot periodicity set to "));
  Serial.println(pingSlotPeriodicity);
  
  // If we're already in Class B with a locked beacon, recalculate ping slots
  if (deviceClass == DEVICE_CLASS_B && beaconState == BEACON_STATE_LOCKED) {
    calculateNextPingSlot();
  }
  
  // Inform the network server about the new ping slot periodicity
  // by sending a specific uplink message (not implemented in this example)
  
  return true;
}

// Get the current ping slot periodicity (Class B)
uint8_t LoRaManager::getPingSlotPeriodicity() const {
  return pingSlotPeriodicity;
}

// Get the current beacon state (Class B)
uint8_t LoRaManager::getBeaconState() const {
  return beaconState;
}

// Set the callback function for beacon reception (Class B)
void LoRaManager::setBeaconCallback(BeaconCallback callback) {
  beaconCallback = callback;
  Serial.println(F("[LoRaWAN] Beacon callback registered"));
}

// Handle beacon reception (Class B)
void LoRaManager::handleBeaconReception(uint8_t* payload, size_t size, float rssi, float snr) {
  // Store the timestamp of reception
  lastBeaconTimestamp = millis();
  
  // Update signal quality metrics
  lastRssi = rssi;
  lastSnr = snr;
  
  Serial.print(F("[LoRaWAN] Beacon received - RSSI: "));
  Serial.print(rssi);
  Serial.print(F(" dBm, SNR: "));
  Serial.print(snr);
  Serial.println(F(" dB"));
  
  // Process beacon information (in a real implementation, we would extract time information)
  
  // Call the beacon callback if registered
  if (beaconCallback != nullptr) {
    beaconCallback(payload, size, rssi, snr);
  }
}

// Calculate next ping slot time (Class B)
void LoRaManager::calculateNextPingSlot() {
  // The real implementation would calculate ping slots based on:
  // 1. Device address
  // 2. Beacon time
  // 3. Ping slot periodicity
  
  // For this example, we'll use a simplified approach with random ping slot timing
  // In a real implementation, this would use precise calculations based on the LoRaWAN specification
  
  // Calculate time to next ping slot based on periodicity
  // pingSlotPeriodicity defines how often ping slots occur (0 = most frequent, 7 = least frequent)
  uint32_t pingPeriod = 1 << (5 + pingSlotPeriodicity); // in seconds
  
  // Convert to milliseconds and add some random offset for demonstration
  uint32_t pingOffset = random(500); // Random offset up to 500ms for demonstration
  
  // Set the next ping slot time
  nextPingSlotTime = millis() + (pingPeriod * 1000) + pingOffset;
  
  Serial.print(F("[LoRaWAN] Next ping slot in "));
  Serial.print(pingPeriod);
  Serial.println(F(" seconds (plus offset)"));
}

// Start continuous reception (Class C)
bool LoRaManager::startContinuousReception() {
  if (node == nullptr || radio == nullptr) {
    Serial.println(F("[LoRaWAN] Node or radio not initialized"));
    lastErrorCode = RADIOLIB_ERR_INVALID_STATE;
    return false;
  }
  
  if (!isJoined) {
    Serial.println(F("[LoRaWAN] Must be joined to the network before starting continuous reception"));
    lastErrorCode = RADIOLIB_ERR_NETWORK_NOT_JOINED;
    return false;
  }
  
  Serial.println(F("[LoRaWAN] Starting continuous reception for Class C operation"));
  
  // In RadioLib 7.x, Class C mode is handled differently
  // The node will keep RX2 window open continuously after any uplink
  // So we just need to track that we're in Class C mode locally
  
  // Mark continuous reception as active
  continuousReception = true;
  
  // Send a small packet to inform the network server we're in Class C mode
  // This will also open the continuous reception window after the uplink
  uint8_t classChangeData[] = {0xC0}; // Class C notification packet
  size_t classChangeLen = sizeof(classChangeData);
  uint8_t downlinkData[8]; // Small buffer for any response
  size_t downlinkLen = sizeof(downlinkData);
  
  // Send with port 0 (MAC commands) or another suitable port
  int state = node->sendReceive(classChangeData, classChangeLen, 3, downlinkData, &downlinkLen, true);
  
  if (state >= RADIOLIB_ERR_NONE) {
    Serial.println(F("[LoRaWAN] Class C activation successful"));
    return true;
  } else {
    Serial.print(F("[LoRaWAN] Class C activation failed with error: "));
    Serial.println(state);
    lastErrorCode = state;
    continuousReception = false;
    return false;
  }
}

// Stop continuous reception (Class C)
void LoRaManager::stopContinuousReception() {
  if (continuousReception) {
    Serial.println(F("[LoRaWAN] Stopping continuous reception"));
    
    // In RadioLib 7.x, to disable Class C, we just need to send an uplink
    // that informs the network we're switching back to Class A
    // For now, we just update our local state
    
    continuousReception = false;
    
    // Send a small packet to inform the network server we're in Class A mode
    uint8_t classChangeData[] = {0xA0}; // Class A notification packet
    size_t classChangeLen = sizeof(classChangeData);
    uint8_t downlinkData[8]; // Small buffer for any response
    size_t downlinkLen = sizeof(downlinkData);
    
    // Send with port 3 (arbitrary choice)
    int state = node->sendReceive(classChangeData, classChangeLen, 3, downlinkData, &downlinkLen, true);
    
    if (state < RADIOLIB_ERR_NONE) {
      Serial.print(F("[LoRaWAN] Warning: Class A notification failed with error: "));
      Serial.println(state);
      lastErrorCode = state;
    } else {
      Serial.println(F("[LoRaWAN] Successfully switched back to Class A mode"));
    }
  }
}

// Get the last error from LoRaWAN operations
int LoRaManager::getLastErrorCode() {
  return lastErrorCode;
} 