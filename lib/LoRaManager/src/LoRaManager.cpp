#include "LoRaManager.h"

// Define LoRaWAN region
#define LORAWAN_REGION_US915 1

// Initialize static instance pointer
LoRaManager* LoRaManager::instance = nullptr;

// Constructor
LoRaManager::LoRaManager() : 
  radio(nullptr),
  node(nullptr),
  lorawanBand(nullptr),
  joinEUI(0),
  devEUI(0),
  isJoined(false),
  lastRssi(0),
  lastSnr(0),
  receivedBytes(0),
  lastErrorCode(RADIOLIB_ERR_NONE),
  consecutiveTransmitErrors(0) {
  
  // Set this instance as the active one
  instance = this;
  
  // Initialize arrays
  memset(appKey, 0, sizeof(appKey));
  memset(nwkKey, 0, sizeof(nwkKey));
  memset(receivedData, 0, sizeof(receivedData));
}

// Destructor
LoRaManager::~LoRaManager() {
  // Clean up allocated resources
  if (node != nullptr) {
    delete node;
  }
  
  if (radio != nullptr) {
    delete radio;
  }
  
  // Clear the instance pointer
  if (instance == this) {
    instance = nullptr;
  }
}

// Initialize the LoRa module
bool LoRaManager::begin(int8_t pinCS, int8_t pinDIO1, int8_t pinReset, int8_t pinBusy) {
  // Create a new Module instance
  Module* module = new Module(pinCS, pinDIO1, pinReset, pinBusy);
  
  // Create a new SX1262 instance
  radio = new SX1262(module);
  
  // Initialize the radio
  Serial.print(F("[SX1262] Initializing ... "));
  int state = radio->begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    return false;
  }

  // Create a new LoRaWANNode instance with US915 band
  // Use the US915 band for now
  #if defined(LORAWAN_REGION_US915)
  lorawanBand = &US915;
  #elif defined(LORAWAN_REGION_EU868)
  lorawanBand = &EU868;
  #endif
  
  // Create a new LoRaWANNode instance
  node = new LoRaWANNode(radio, lorawanBand);
  
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

// Join the LoRaWAN network
bool LoRaManager::joinNetwork() {
  if (node == nullptr) {
    Serial.println(F("[LoRaWAN] Node not initialized!"));
    lastErrorCode = -2; // RADIOLIB_ERR_INVALID_STATE
    return false;
  }
  
  // Attempt to join the network
  Serial.print(F("[LoRaWAN] Attempting over-the-air activation ... "));
  
  // Begin OTAA procedure - using the correct API
  node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  
  // Try to join the network
  int state = node->activateOTAA();
  lastErrorCode = state;
  
  // Check for successful join or new session status
  // RADIOLIB_LORAWAN_NEW_SESSION (-1118) is a status indicating a new session was created - not an error
  if (state == RADIOLIB_ERR_NONE || state == -1118) {
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.println(F("success! (new session started)"));
    }
    isJoined = true;
    return true;
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    
    isJoined = false;
    return false;
  }
}

// Send data to the LoRaWAN network
bool LoRaManager::sendData(uint8_t* data, size_t len, uint8_t port, bool confirmed) {
  if (!isJoined || node == nullptr) {
    Serial.println(F("[LoRaWAN] Not joined to network!"));
    
    // Add automatic rejoin attempt when trying to send while not joined
    Serial.println(F("[LoRaWAN] Attempting to rejoin the network..."));
    if (joinNetwork()) {
      Serial.println(F("[LoRaWAN] Successfully rejoined, will now try to send data"));
    } else {
      Serial.println(F("[LoRaWAN] Rejoin failed, cannot send data"));
      return false;
    }
  }
  
  // Send the data
  Serial.print(F("[LoRaWAN] Sending data ... "));
  
  // Prepare buffer for downlink
  uint8_t downlinkData[256];
  size_t downlinkLen = 0;
  
  // Send data and wait for downlink
  int state = node->sendReceive(data, len, port, downlinkData, &downlinkLen, confirmed);
  lastErrorCode = state;
  
  if (state > 0) {
    // Downlink received in window state (1 = RX1, 2 = RX2)
    Serial.print(F("success! Received downlink in RX"));
    Serial.println(state);
    
    // Process the downlink data
    if (downlinkLen > 0) {
      Serial.print(F("[LoRaWAN] Received "));
      Serial.print(downlinkLen);
      Serial.println(F(" bytes:"));
      
      for (size_t i = 0; i < downlinkLen; i++) {
        Serial.print(downlinkData[i], HEX);
        Serial.print(' ');
      }
      Serial.println();
      
      // Copy the data to our buffer
      memcpy(receivedData, downlinkData, downlinkLen);
      receivedBytes = downlinkLen;
    }
    
    // Get RSSI and SNR
    lastRssi = radio->getRSSI();
    lastSnr = radio->getSNR();
    
    consecutiveTransmitErrors = 0; // Reset error counter on success
    return true;
  } else if (state == RADIOLIB_ERR_NONE) {
    // No downlink received but uplink was successful
    Serial.println(F("success! No downlink received."));
    consecutiveTransmitErrors = 0; // Reset error counter on success
    return true;
  } else {
    // Error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);
    
    consecutiveTransmitErrors++; // Track consecutive errors
    
    // If we've encountered errors multiple times in a row, try rejoining
    if (consecutiveTransmitErrors >= 3) {
      Serial.println(F("[LoRaWAN] Multiple transmission errors, attempting to rejoin..."));
      isJoined = false; // Force a rejoin by marking as not joined
      if (joinNetwork()) {
        Serial.println(F("[LoRaWAN] Successfully rejoined network"));
        consecutiveTransmitErrors = 0;
      } else {
        Serial.println(F("[LoRaWAN] Failed to rejoin network"));
      }
    }
    
    return false;
  }
}

// Helper method to send a string
bool LoRaManager::sendString(const String& data, uint8_t port) {
  return sendData((uint8_t*)data.c_str(), data.length(), port);
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

// Handle events (should be called in the loop)
void LoRaManager::handleEvents() {
  if (!isJoined || node == nullptr) {
    return;
  }
  
  // This method is kept for compatibility but is not needed with the current implementation
  // as the sendReceive method already handles downlink reception
}

// Get the last error from LoRaWAN operations
int LoRaManager::getLastErrorCode() {
  return lastErrorCode;
} 