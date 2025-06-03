#include "HybridLoRaWAN.h"

// Pointer to the current instance for static IRQ handler
static HybridLoRaWAN* currentInstance = nullptr;

// Constructor
HybridLoRaWAN::HybridLoRaWAN(int nssPin, int resetPin, int busyPin, int irqPin, int txenPin, int rxenPin) :
    nssPin(nssPin), resetPin(resetPin), busyPin(busyPin), irqPin(irqPin), txenPin(txenPin), rxenPin(rxenPin),
    radio(nullptr), node(nullptr), band(nullptr),
    initialized(false), joined(false), packetReceived(false),
    receiveCallback(nullptr), joinCallback(nullptr),
    workingBandwidth(0.0f)
{
    // Save instance for static IRQ handler
    currentInstance = this;
    
    // Initialize credential arrays
    memset(devEuiBytes, 0, sizeof(devEuiBytes));
    memset(appEuiBytes, 0, sizeof(appEuiBytes));
    memset(appKeyBytes, 0, sizeof(appKeyBytes));
}

// Destructor
HybridLoRaWAN::~HybridLoRaWAN() {
    // Clean up resources
    if (node != nullptr) {
        delete node;
        node = nullptr;
    }
    
    if (band != nullptr) {
        delete band;
        band = nullptr;
    }
    
    if (radio != nullptr) {
        delete radio;
        radio = nullptr;
    }
    
    // Clear the current instance
    if (currentInstance == this) {
        currentInstance = nullptr;
    }
}

// Initialize the LoRaWAN stack
bool HybridLoRaWAN::begin(bool debugPrint) {
    // Initialize the SX1262 radio using nopnop2002's driver
    radio = new Ra01S(nssPin, resetPin, busyPin, irqPin, txenPin, rxenPin);
    
    if (debugPrint) {
        radio->DebugPrint(true);
    }
    
    // Initialize radio with US915 frequency
    if (radio->begin(915.0, 14) != 0) {
        Serial.println("Radio initialization failed!");
        return false;
    }
    
    // Test bandwidths to find a working one
    float supportedBandwidths[10];
    int numSupported = testBandwidths(supportedBandwidths, 10);
    
    if (numSupported == 0) {
        Serial.println("No compatible bandwidths found!");
        return false;
    }
    
    // Use the first supported bandwidth
    workingBandwidth = supportedBandwidths[0];
    Serial.printf("Using bandwidth: %.2f kHz\n", workingBandwidth);
    
    // Configure radio for LoRa
    radio->LoRaConfig(
        9,              // Spreading Factor (SF9)
        workingBandwidth, // Bandwidth (from testing)
        7,              // Coding Rate (4/7)
        8,              // Preamble Length
        0,              // Header mode (explicit)
        true,           // CRC enabled
        false           // Standard IQ mode
    );
    
    // Initialize the LoRaWAN band for US915
    band = new RadioLib::LoRaWANBand_US915();
    
    // Create LoRaWANNode using our radio
    node = new RadioLib::LoRaWANNode(radio, band);
    
    // Set up DIO1 interrupt
    pinMode(irqPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(irqPin), staticIrqHandler, RISING);
    
    // Set initial state
    initialized = true;
    
    return true;
}

// Set OTAA credentials
void HybridLoRaWAN::setOTAACredentials(const char* devEUI, const char* appEUI, const char* appKey) {
    // Convert credentials from hex strings to byte arrays
    hexStringToBytes(devEUI, devEuiBytes, 8);
    hexStringToBytes(appEUI, appEuiBytes, 8);
    hexStringToBytes(appKey, appKeyBytes, 16);
}

// Join the LoRaWAN network using OTAA
bool HybridLoRaWAN::joinOTAA(join_callback_t callback) {
    if (!initialized) {
        Serial.println("LoRaWAN stack not initialized!");
        return false;
    }
    
    // Store callback
    joinCallback = callback;
    
    // Configure for ADR
    node->setADR(true);
    
    // Set device class to C for continuous reception
    node->setDeviceClass(RADIOLIB_LORAWAN_CLASS_C);
    
    // Convert endianness for EUIs
    uint64_t devEuiInt = bytesToEui(devEuiBytes);
    uint64_t appEuiInt = bytesToEui(appEuiBytes);
    
    // Begin OTAA activation
    int state = node->beginOTAA(appEuiInt, devEuiInt, appKeyBytes, appKeyBytes);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("LoRaWAN initialization failed, code %d\n", state);
        if (joinCallback) {
            joinCallback(false);
        }
        return false;
    }
    
    // Perform OTAA join
    state = node->joinOTAA();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("LoRaWAN join failed, code %d\n", state);
        if (joinCallback) {
            joinCallback(false);
        }
        return false;
    }
    
    // Verify join with device address
    uint32_t deviceAddress = node->getDevAddr();
    if (deviceAddress == 0) {
        Serial.println("Join succeeded but device address is zero!");
        if (joinCallback) {
            joinCallback(false);
        }
        return false;
    }
    
    // Join successful
    joined = true;
    Serial.printf("Join successful! Device address: 0x%08X\n", deviceAddress);
    
    // Enable Class C reception
    if (!enableClassC()) {
        Serial.println("Failed to enable Class C reception!");
        if (joinCallback) {
            joinCallback(false);
        }
        return false;
    }
    
    // Notify callback
    if (joinCallback) {
        joinCallback(true);
    }
    
    return true;
}

// Set the receive callback
void HybridLoRaWAN::setReceiveCallback(receive_callback_t callback) {
    receiveCallback = callback;
}

// Send data to the LoRaWAN network
bool HybridLoRaWAN::send(uint8_t* data, size_t len, uint8_t port, bool confirmed) {
    if (!initialized || !joined) {
        Serial.println("Not initialized or joined!");
        return false;
    }
    
    // Send data
    int state;
    if (confirmed) {
        state = node->sendReceive(data, len, port);
    } else {
        state = node->sendReceive(data, len, port);
    }
    
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("Failed to send data, code %d\n", state);
        return false;
    }
    
    // Re-enable Class C reception
    enableClassC();
    
    return true;
}

// Loop function to process events
void HybridLoRaWAN::loop() {
    if (!initialized) {
        return;
    }
    
    // Check if a packet was received via interrupt
    if (packetReceived) {
        packetReceived = false;
        processReceivedPacket();
    }
}

// Enable Class C operation
bool HybridLoRaWAN::enableClassC() {
    if (!initialized || !joined) {
        Serial.println("Not initialized or joined!");
        return false;
    }
    
    // Configure radio for continuous reception on RX2 parameters
    return configureRadioForRx2();
}

// Check if the device has joined the network
bool HybridLoRaWAN::isJoined() const {
    return joined;
}

// Get the device address
uint32_t HybridLoRaWAN::getDeviceAddress() const {
    if (!initialized || !joined) {
        return 0;
    }
    
    return node->getDevAddr();
}

// Test different bandwidths to find compatible ones
int HybridLoRaWAN::testBandwidths(float* supportedBandwidths, int maxBandwidths) {
    // Test bandwidths (kHz)
    const float bandwidthsToTest[] = {
        7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0, 500.0
    };
    const int numBandwidths = sizeof(bandwidthsToTest) / sizeof(float);
    
    int numSupported = 0;
    
    for (int i = 0; i < numBandwidths && numSupported < maxBandwidths; i++) {
        Serial.printf("Testing bandwidth %.2f kHz... ", bandwidthsToTest[i]);
        int result = radio->setBandWidth(bandwidthsToTest[i]);
        
        if (result == 0) {
            Serial.println("SUCCESS");
            supportedBandwidths[numSupported++] = bandwidthsToTest[i];
        } else {
            Serial.printf("FAILED (Error: %d)\n", result);
        }
    }
    
    return numSupported;
}

// Static IRQ handler
void HybridLoRaWAN::staticIrqHandler() {
    if (currentInstance != nullptr) {
        currentInstance->irqHandler();
    }
}

// IRQ handler
void HybridLoRaWAN::irqHandler() {
    packetReceived = true;
}

// Convert hex string to byte array
void HybridLoRaWAN::hexStringToBytes(const char* hexString, uint8_t* byteArray, size_t length) {
    for (size_t i = 0; i < length; i++) {
        sscanf(hexString + 2 * i, "%2hhx", &byteArray[i]);
    }
}

// Convert byte array to EUI
uint64_t HybridLoRaWAN::bytesToEui(const uint8_t* bytes) {
    uint64_t eui = 0;
    for (int i = 0; i < 8; i++) {
        eui = (eui << 8) | bytes[i];
    }
    return eui;
}

// Configure radio for RX2 parameters
bool HybridLoRaWAN::configureRadioForRx2() {
    // Get RX2 parameters from band
    RadioLib::LoRaWANChannel_t rx2Channel;
    band->getRX2Channel(&rx2Channel);
    
    // Configure radio for RX2 parameters
    int state = radio->setFrequency(rx2Channel.frequency);
    if (state != 0) {
        Serial.printf("Failed to set RX2 frequency: %d\n", state);
        return false;
    }
    
    state = radio->setSpreadingFactor(rx2Channel.spreadingFactor);
    if (state != 0) {
        Serial.printf("Failed to set RX2 spreading factor: %d\n", state);
        return false;
    }
    
    state = radio->setBandWidth(workingBandwidth);
    if (state != 0) {
        Serial.printf("Failed to set RX2 bandwidth: %d\n", state);
        return false;
    }
    
    // Set sync word for public networks
    state = radio->setSyncWord(0x34);
    if (state != 0) {
        Serial.printf("Failed to set sync word: %d\n", state);
        return false;
    }
    
    // Start continuous reception
    state = radio->receive(0);
    if (state != 0) {
        Serial.printf("Failed to start continuous reception: %d\n", state);
        return false;
    }
    
    Serial.println("Class C continuous reception enabled");
    return true;
}

// Process received packet
bool HybridLoRaWAN::processReceivedPacket() {
    // Read the received data
    uint8_t data[256];
    size_t len = sizeof(data);
    int state = radio->readData(data, len);
    
    if (state == 0 && len > 0) {
        Serial.println("Downlink received!");
        
        // Try to parse as LoRaWAN packet
        state = node->processDownlink(data, len);
        if (state == RADIOLIB_ERR_NONE) {
            // Extract actual payload
            uint8_t payload[256];
            size_t payloadLen = sizeof(payload);
            int fport = node->getDownlinkFPort(payload, &payloadLen);
            
            Serial.printf("Downlink on FPort %d, length %d bytes\n", fport, payloadLen);
            
            // Call user callback if set
            if (receiveCallback != nullptr && payloadLen > 0) {
                // Get RSSI and SNR
                int8_t rssi = -100; // Default value
                float snr = 0;      // Default value
                
                // Get packet status if possible
                int8_t rssiPacket, snrPacket;
                radio->GetPacketStatus(&rssiPacket, &snrPacket);
                
                // Call the callback
                receiveCallback(payload, payloadLen, rssiPacket, (float)snrPacket);
            }
        } else {
            Serial.printf("Failed to process downlink, code %d\n", state);
            
            // Re-enable continuous reception
            configureRadioForRx2();
            return false;
        }
    } else {
        Serial.printf("Failed to read downlink data, code %d\n", state);
        
        // Re-enable continuous reception
        configureRadioForRx2();
        return false;
    }
    
    // Re-enable continuous reception
    configureRadioForRx2();
    return true;
} 