# LoRaManager

A library for managing LoRaWAN communication using RadioLib for ESP32 boards.

## Features

* Easy to use interface for LoRaWAN communication
* Supports OTAA (Over-The-Air Activation)
* Automatic handling of network join and reconnection
* Support for both confirmed and unconfirmed data transmission
* Error handling with automatic retry mechanism

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

### Basic Example

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
  
  // Wait for 5 minutes before sending again
  delay(300000);
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
- `bool joinNetwork()` - Join the LoRaWAN network
- `bool sendData(uint8_t* data, size_t len, uint8_t port = 1, bool confirmed = false)` - Send data to the LoRaWAN network
- `bool sendString(const String& data, uint8_t port = 1)` - Send a string to the LoRaWAN network
- `float getLastRssi()` - Get the last RSSI value
- `float getLastSnr()` - Get the last SNR value
- `bool isNetworkJoined()` - Check if the device is joined to the network
- `void handleEvents()` - Handle events (optional, can be called in the loop)
- `int getLastErrorCode()` - Get the last error from LoRaWAN operations

## License

This library is licensed under the MIT License. 