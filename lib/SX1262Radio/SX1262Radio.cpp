/**
 * SX1262Radio.cpp - Integration layer between nopnop2002's SX1262 driver and RadioLib's LoRaWAN stack
 * 
 * This file provides a compatibility layer that allows using nopnop2002's SX1262 driver
 * with RadioLib's LoRaWAN stack for true Class C operation.
 * 
 * Created: May 2024
 */

#include "SX1262Radio.h"

// Global variables for interrupt handling
volatile bool packetReceived = false;
SX1262Radio* activeRadio = nullptr;

// DIO1 interrupt handler
void IRAM_ATTR dio1InterruptHandler() {
    packetReceived = true;
}

// Helper function to convert bandwidth in kHz to the SX126x enum value
uint8_t bandwidthToEnum(float bandwidth) {
    if (bandwidth <= 7.8) return 0;
    if (bandwidth <= 10.4) return 1;
    if (bandwidth <= 15.6) return 2;
    if (bandwidth <= 20.8) return 3;
    if (bandwidth <= 31.25) return 4;
    if (bandwidth <= 41.7) return 5;
    if (bandwidth <= 62.5) return 6;
    if (bandwidth <= 125.0) return 7;
    if (bandwidth <= 250.0) return 8;
    return 9; // 500 kHz
}

// Constructor
SX1262Radio::SX1262Radio() {
    radio = new SX126x(NSS_PIN, RESET_PIN, BUSY_PIN);
    freq = 915.0;
    bw = 125.0;
    sf = 7;
    cr = 1;
    syncWord = 0x12;
    preambleLength = 8;
    crcEnabled = true;
    interruptEnabled = false;
    rxDoneCallback = nullptr;
    
    // Initialize supported bandwidths array
    numSupportedBandwidths = 0;
    for (int i = 0; i < 10; i++) {
        supportedBandwidths[i] = 0.0;
    }
    
    // Register this instance for interrupt handling
    activeRadio = this;
}

// Initialize radio
bool SX1262Radio::begin() {
    if (!radio) {
        return false;
    }
    
    // Initialize radio
    int16_t state = radio->begin(freq * 1000000, 22); // 22 dBm TX power
    if (state != ERR_NONE) {
        Serial.printf("Failed to initialize radio: %d\n", state);
        return false;
    }
    
    // Detect supported bandwidths
    detectSupportedBandwidths();
    
    if (numSupportedBandwidths == 0) {
        Serial.println("No supported bandwidths found!");
        return false;
    }
    
    // Use the first supported bandwidth
    bw = supportedBandwidths[0];
    
    // Configure LoRa parameters
    state = radio->LoRaConfig(sf, 
                          bandwidthToEnum(bw),
                          cr,
                          preambleLength,
                          0, // Variable length packet
                          crcEnabled,
                          false); // invertIrq
    
    if (state != ERR_NONE) {
        Serial.printf("Failed to configure LoRa parameters: %d\n", state);
        return false;
    }
    
    // Configure DIO1 for interrupt
    pinMode(DIO1_PIN, INPUT);
    
    // Set DIO IRQ parameters (but don't attach interrupt yet)
    radio->SetDioIrqParams(SX126X_IRQ_ALL,                    // All interrupts enabled
                           SX126X_IRQ_RX_DONE,                // Map RX_DONE to DIO1
                           SX126X_IRQ_NONE,                   // No interrupts on DIO2
                           SX126X_IRQ_NONE);                  // No interrupts on DIO3
    
    return true;
}

// Test if a bandwidth is supported
bool SX1262Radio::testBandwidth(float bandwidth) {
    if (!radio) {
        return false;
    }
    
    // Try to set frequency (this usually works)
    int16_t state = radio->SetRfFrequency(915000000); // 915 MHz
    if (state != ERR_NONE) {
        return false;
    }
    
    // Try setting the bandwidth
    uint8_t bwEnum = bandwidthToEnum(bandwidth);
    state = radio->SetModulationParams(sf, bwEnum, cr);
    
    return (state == ERR_NONE);
}

// Detect all supported bandwidths
void SX1262Radio::detectSupportedBandwidths() {
    float bandwidthsToTest[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0, 500.0};
    numSupportedBandwidths = 0;
    
    for (int i = 0; i < 10; i++) {
        if (testBandwidth(bandwidthsToTest[i])) {
            supportedBandwidths[numSupportedBandwidths++] = bandwidthsToTest[i];
            Serial.printf("Bandwidth %.2f kHz is supported\n", bandwidthsToTest[i]);
        }
    }
}

// Set frequency
int16_t SX1262Radio::setFrequency(float frequency) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    freq = frequency;
    return radio->SetRfFrequency(uint32_t(frequency * 1000000.0));
}

// Set bandwidth
int16_t SX1262Radio::setBandwidth(float bandwidth) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    // Check if bandwidth is supported
    if (!isBandwidthSupported(bandwidth)) {
        // Try to find the nearest supported bandwidth
        float nearest = 500.0; // Default to highest
        float minDiff = 1000.0;
        
        for (int i = 0; i < numSupportedBandwidths; i++) {
            float diff = abs(supportedBandwidths[i] - bandwidth);
            if (diff < minDiff) {
                minDiff = diff;
                nearest = supportedBandwidths[i];
            }
        }
        
        Serial.printf("Bandwidth %.2f kHz not supported, using %.2f kHz instead\n", 
                    bandwidth, nearest);
        bandwidth = nearest;
    }
    
    bw = bandwidth;
    
    // Update modulation parameters
    return radio->SetModulationParams(sf, bandwidthToEnum(bw), cr);
}

// Set spreading factor
int16_t SX1262Radio::setSpreadingFactor(uint8_t spreadingFactor) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    sf = spreadingFactor;
    
    // Update modulation parameters
    return radio->SetModulationParams(sf, bandwidthToEnum(bw), cr);
}

// Set coding rate
int16_t SX1262Radio::setCodingRate(uint8_t codingRate) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    cr = codingRate;
    
    // Update modulation parameters
    return radio->SetModulationParams(sf, bandwidthToEnum(bw), cr);
}

// Set sync word
int16_t SX1262Radio::setSyncWord(uint8_t sw) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    syncWord = sw;
    
    // Set LoRa sync word
    return radio->WriteRegister(REG_LR_SYNCWORD, &syncWord, 1);
}

// Set preamble length
int16_t SX1262Radio::setPreambleLength(uint16_t length) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    preambleLength = length;
    
    // Set packet parameters
    return radio->SetPacketParams(preambleLength, 0, 0xFF, crcEnabled, false);
}

// Set CRC
int16_t SX1262Radio::setCRC(bool enable) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    crcEnabled = enable;
    
    // Set packet parameters
    return radio->SetPacketParams(preambleLength, 0, 0xFF, crcEnabled, false);
}

// Transmit data
int16_t SX1262Radio::transmit(uint8_t* data, size_t len) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    // Disable interrupt during transmission
    if (interruptEnabled) {
        detachInterrupt(digitalPinToInterrupt(DIO1_PIN));
    }
    
    // Send data synchronously
    bool result = radio->Send(data, len, SX126x_TXMODE_SYNC);
    
    // Re-enable interrupt after transmission
    if (interruptEnabled) {
        attachInterrupt(digitalPinToInterrupt(DIO1_PIN), dio1InterruptHandler, RISING);
    }
    
    return result ? ERR_NONE : ERR_TX_TIMEOUT;
}

// Receive data with timeout
int16_t SX1262Radio::receive(uint8_t* data, size_t len) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    // Wait for data with timeout
    uint8_t receivedLen = radio->Receive(data, len);
    
    return (receivedLen > 0) ? receivedLen : ERR_RX_TIMEOUT;
}

// Start continuous receive mode
int16_t SX1262Radio::startReceive() {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    // Clear any pending flags
    packetReceived = false;
    
    // Start receiver (asynchronous mode)
    radio->RxMode();
    
    return ERR_NONE;
}

// Put radio in standby mode
int16_t SX1262Radio::standby() {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    return radio->StandbyMode();
}

// Put radio in sleep mode
int16_t SX1262Radio::sleep() {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    return radio->SleepMode();
}

// Enable Class C continuous reception
bool SX1262Radio::enableClassC(float frequency, uint8_t spreadingFactor, float bandwidth) {
    if (!radio) {
        return false;
    }
    
    // Store RX2 parameters
    rx2_frequency = frequency;
    rx2_sf = spreadingFactor;
    rx2_bw = bandwidth;
    
    // Configure radio for RX2 parameters
    int16_t state = setFrequency(rx2_frequency);
    state |= setSpreadingFactor(rx2_sf);
    state |= setBandwidth(rx2_bw);
    
    if (state != ERR_NONE) {
        Serial.printf("Failed to configure RX2 parameters: %d\n", state);
        return false;
    }
    
    // Start continuous reception
    state = startReceive();
    if (state != ERR_NONE) {
        Serial.printf("Failed to start reception: %d\n", state);
        return false;
    }
    
    return true;
}

// Disable Class C mode
bool SX1262Radio::disableClassC() {
    if (!radio) {
        return false;
    }
    
    // Put radio in standby mode
    int16_t state = standby();
    if (state != ERR_NONE) {
        return false;
    }
    
    return true;
}

// Set DIO1 action (callback for received packets)
void SX1262Radio::setDio1Action(rx_done_callback_t callback) {
    rxDoneCallback = callback;
    
    if (callback) {
        // Enable interrupt
        attachInterrupt(digitalPinToInterrupt(DIO1_PIN), dio1InterruptHandler, RISING);
        interruptEnabled = true;
    } else {
        // Disable interrupt
        if (interruptEnabled) {
            detachInterrupt(digitalPinToInterrupt(DIO1_PIN));
            interruptEnabled = false;
        }
    }
}

// Check if a packet has been received
bool SX1262Radio::isReceived() {
    // Check if we received a packet
    bool received = packetReceived;
    
    // Clear flag
    if (received) {
        packetReceived = false;
    }
    
    return received;
}

// Read received data
int16_t SX1262Radio::readData(uint8_t* data, size_t len) {
    if (!radio) {
        return ERR_UNKNOWN;
    }
    
    // Put radio in standby mode
    int16_t state = standby();
    if (state != ERR_NONE) {
        return ERR_UNKNOWN;
    }
    
    // Read packet data
    PacketStatus_t pktStatus;
    uint8_t receivedLen = 0;
    int16_t result = radio->ReadBuffer(data, &receivedLen, len, &pktStatus);
    
    // Restart reception
    startReceive();
    
    return (result == ERR_NONE) ? receivedLen : ERR_UNKNOWN;
}

// Get RSSI of last received packet
float SX1262Radio::getRSSI() {
    if (!radio) {
        return -200.0; // Invalid RSSI
    }
    
    PacketStatus_t pktStatus;
    radio->GetPacketStatus(&pktStatus);
    
    return static_cast<float>(pktStatus.LoRa.RssiPkt);
}

// Get SNR of last received packet
float SX1262Radio::getSNR() {
    if (!radio) {
        return -200.0; // Invalid SNR
    }
    
    PacketStatus_t pktStatus;
    radio->GetPacketStatus(&pktStatus);
    
    return static_cast<float>(pktStatus.LoRa.SnrPkt);
}

// Check if channel is free (RSSI below threshold)
bool SX1262Radio::isChannelFree(float rssiThreshold) {
    if (!radio) {
        return false;
    }
    
    int8_t rssi = radio->GetRssiInst();
    return (rssi < rssiThreshold);
}

// Get number of supported bandwidths
int SX1262Radio::getNumSupportedBandwidths() {
    return numSupportedBandwidths;
}

// Get supported bandwidth at index
float SX1262Radio::getSupportedBandwidth(int index) {
    if (index >= 0 && index < numSupportedBandwidths) {
        return supportedBandwidths[index];
    }
    return 0.0;
}

// Check if bandwidth is supported
bool SX1262Radio::isBandwidthSupported(float bandwidth) {
    for (int i = 0; i < numSupportedBandwidths; i++) {
        if (abs(supportedBandwidths[i] - bandwidth) < 0.1) {
            return true;
        }
    }
    return false;
} 