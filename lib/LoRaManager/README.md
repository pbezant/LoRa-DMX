# LoRaManager

A library for managing LoRaWAN communication using RadioLib for ESP32 boards.

## Features

* Easy to use interface for LoRaWAN communication
* Supports OTAA (Over-The-Air Activation)
* Automatic handling of network join and reconnection
* Support for both confirmed and unconfirmed data transmission
* Error handling with automatic retry mechanism
* Support for hex string credentials instead of byte arrays
* **Support for LoRaWAN device classes A, B, and C**
* **Class B functionality with beacon synchronization and ping slots**
* **Class C functionality with continuous reception**

## Dependencies

* [RadioLib](https://github.com/jgromes/RadioLib) (>= v6.0.0)
* Arduino framework

## Installation

### PlatformIO

1. Add the library to your `platformio.ini` file:

```ini
lib_deps =
    LoRaManager
```

### Arduino IDE

1. Download the library as a ZIP file
2. In the Arduino IDE, go to Sketch > Include Library > Add .ZIP Library...
3. Select the downloaded ZIP file

## Hardware Compatibility

This library is primarily designed for ESP32 boards with SX1262 LoRa modules, such as:

* HELTEC Wireless Stick Lite
* HELTEC WiFi LoRa 32 V3
* Other ESP32 boards with SX1262 LoRa modules

## Usage

### Basic Example (Class A)

```cpp
#include <Arduino.h>
#include <LoRaManager.h>

// Define LoRa pins for your board
#define LORA_CS   18
#define LORA_DIO1 23
#define LORA_RST  14
#define LORA_BUSY 33

// LoRaWAN credentials
uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI = 0x0000000000000000;
uint8_t appKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t nwkKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Create LoRaManager instance
LoRaManager lora;

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("Starting LoRaWAN communication...");
  
  // Initialize the LoRa module
  if (!lora.begin(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY)) {
    Serial.println("Failed to initialize LoRa!");
    while (1);
  }
  
  // Set LoRaWAN credentials
  lora.setCredentials(joinEUI, devEUI, appKey, nwkKey);
  
  // Join the network
  Serial.println("Joining LoRaWAN network...");
  if (lora.joinNetwork()) {
    Serial.println("Successfully joined the network!");
  } else {
    Serial.println("Failed to join the network!");
    // The library will automatically retry when sending data
  }
}

void loop() {
  // Prepare some data to send
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  
  // Send the data on port 1 (unconfirmed)
  if (lora.sendData(data, sizeof(data), 1, false)) {
    Serial.println("Data sent successfully!");
    
    // Get the last RSSI and SNR
    Serial.print("RSSI: ");
    Serial.print(lora.getLastRssi());
    Serial.print(" dBm, SNR: ");
    Serial.print(lora.getLastSnr());
    Serial.println(" dB");
  } else {
    Serial.println("Failed to send data!");
  }
  
  // Handle LoRaWAN events (required for Class B and C)
  lora.handleEvents();
  
  // Wait for 5 minutes before sending again
  delay(300000);
}
```

### Class B Example

```cpp
#include <Arduino.h>
#include <LoRaManager.h>

// Define LoRa pins for your board
#define LORA_CS   18
#define LORA_DIO1 23
#define LORA_RST  14
#define LORA_BUSY 33

// LoRaWAN credentials
uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI = 0x0000000000000000;
uint8_t appKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t nwkKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Create LoRaManager instance
LoRaManager lora;

// Callback for downlink data
void handleDownlink(uint8_t* payload, size_t size, uint8_t port) {
  Serial.print("Received downlink on port ");
  Serial.print(port);
  Serial.print(": ");
  for (size_t i = 0; i < size; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// Callback for beacon reception
void handleBeacon(uint8_t* payload, size_t size, float rssi, float snr) {
  Serial.println("Beacon received!");
  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.print(" dBm, SNR: ");
  Serial.print(snr);
  Serial.println(" dB");
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("Starting LoRaWAN communication (Class B)...");
  
  // Initialize the LoRa module
  if (!lora.begin(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY)) {
    Serial.println("Failed to initialize LoRa!");
    while (1);
  }
  
  // Set LoRaWAN credentials
  lora.setCredentials(joinEUI, devEUI, appKey, nwkKey);
  
  // Set downlink and beacon callbacks
  lora.setDownlinkCallback(handleDownlink);
  lora.setBeaconCallback(handleBeacon);
  
  // Join the network
  Serial.println("Joining LoRaWAN network...");
  if (lora.joinNetwork()) {
    Serial.println("Successfully joined the network!");
    
    // Switch to Class B mode
    if (lora.setDeviceClass(DEVICE_CLASS_B)) {
      Serial.println("Switched to Class B mode, starting beacon acquisition...");
      
      // Set ping slot periodicity (0-7, where 0 is most frequent, 7 is least frequent)
      lora.setPingSlotPeriodicity(0);
    } else {
      Serial.println("Failed to switch to Class B mode!");
    }
  } else {
    Serial.println("Failed to join the network!");
  }
}

void loop() {
  // Handle LoRaWAN events (required for Class B)
  lora.handleEvents();
  
  // Check beacon state
  if (lora.getBeaconState() == BEACON_STATE_LOCKED) {
    // Beacons are being received, device is in Class B mode
    // You can send data as usual
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 300000) { // Send every 5 minutes
      uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
      lora.sendData(data, sizeof(data), 1, false);
      lastSendTime = millis();
    }
  }
  
  delay(100); // Short delay to prevent CPU hogging
}
```

### Class C Example

```cpp
#include <Arduino.h>
#include <LoRaManager.h>

// Define LoRa pins for your board
#define LORA_CS   18
#define LORA_DIO1 23
#define LORA_RST  14
#define LORA_BUSY 33

// LoRaWAN credentials
uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI = 0x0000000000000000;
uint8_t appKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t nwkKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Create LoRaManager instance
LoRaManager lora;

// Callback for downlink data
void handleDownlink(uint8_t* payload, size_t size, uint8_t port) {
  Serial.print("Received downlink on port ");
  Serial.print(port);
  Serial.print(": ");
  for (size_t i = 0; i < size; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Process downlink commands immediately (Class C advantage)
  if (port == 2 && size > 0) {
    // Example: processing a command
    switch (payload[0]) {
      case 0x01:
        Serial.println("Received command: Turn on LED");
        // Turn on LED code here
        break;
      case 0x02:
        Serial.println("Received command: Turn off LED");
        // Turn off LED code here
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("Starting LoRaWAN communication (Class C)...");
  
  // Initialize the LoRa module
  if (!lora.begin(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY)) {
    Serial.println("Failed to initialize LoRa!");
    while (1);
  }
  
  // Set LoRaWAN credentials
  lora.setCredentials(joinEUI, devEUI, appKey, nwkKey);
  
  // Set downlink callback
  lora.setDownlinkCallback(handleDownlink);
  
  // Join the network
  Serial.println("Joining LoRaWAN network...");
  if (lora.joinNetwork()) {
    Serial.println("Successfully joined the network!");
    
    // Switch to Class C mode for continuous reception
    if (lora.setDeviceClass(DEVICE_CLASS_C)) {
      Serial.println("Switched to Class C mode, continuous reception active");
    } else {
      Serial.println("Failed to switch to Class C mode!");
    }
  } else {
    Serial.println("Failed to join the network!");
  }
}

void loop() {
  // Handle LoRaWAN events (required for Class C)
  lora.handleEvents();
  
  // Send telemetry data periodically
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 300000) { // Send every 5 minutes
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    lora.sendData(data, sizeof(data), 1, false);
    lastSendTime = millis();
  }
  
  delay(100); // Short delay to prevent CPU hogging
}
```

## API Reference

### Constructor

```cpp
LoRaManager();
```

### Methods

- `bool begin(int8_t pinCS, int8_t pinDIO1, int8_t pinReset, int8_t pinBusy)` - Initialize the LoRa module
- `void setCredentials(uint64_t joinEUI, uint64_t devEUI, uint8_t* appKey, uint8_t* nwkKey)` - Set the LoRaWAN credentials
- `bool setCredentialsHex(uint64_t joinEUI, uint64_t devEUI, const String& appKeyHex, const String& nwkKeyHex)` - Set the LoRaWAN credentials using hex strings
- `bool joinNetwork()` - Join the LoRaWAN network
- `bool sendData(uint8_t* data, size_t len, uint8_t port = 1, bool confirmed = false)` - Send data to the LoRaWAN network
- `bool sendString(const String& data, uint8_t port = 1)` - Send a string to the LoRaWAN network
- `float getLastRssi()` - Get the last RSSI value
- `float getLastSnr()` - Get the last SNR value
- `bool isNetworkJoined()` - Check if the device is joined to the network
- `void handleEvents()` - Handle events (required for Class B and C)
- `int getLastErrorCode()` - Get the last error from LoRaWAN operations

#### Class B and C Methods
- `bool setDeviceClass(char deviceClass)` - Set the device class (DEVICE_CLASS_A, DEVICE_CLASS_B, or DEVICE_CLASS_C)
- `char getDeviceClass()` - Get the current device class
- `bool startBeaconAcquisition()` - Start beacon acquisition (Class B)
- `void stopBeaconAcquisition()` - Stop beacon acquisition (Class B)
- `bool setPingSlotPeriodicity(uint8_t periodicity)` - Set the ping slot periodicity (Class B, 0-7)
- `uint8_t getPingSlotPeriodicity()` - Get the current ping slot periodicity (Class B)
- `uint8_t getBeaconState()` - Get the current beacon state (Class B)
- `void setBeaconCallback(BeaconCallback callback)` - Set the callback function for beacon reception (Class B)
- `void setDownlinkCallback(DownlinkCallback callback)` - Set the callback function for downlink data

## License

This library is licensed under the MIT License. 