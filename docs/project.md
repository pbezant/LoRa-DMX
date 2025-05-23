# Core Architecture, Tech Stack, API Patterns, and Database Schema

This document outlines the core architectural decisions, the primary technologies used, established API patterns, and an overview of the database schema.

## Core Architecture Decisions

*   **Microcontroller Platform:** Heltec LoRa 32 V3 with ESP32S3 and SX126x radio
*   **LoRaWAN Communication:** Official Heltec LoRaWAN library for optimal hardware compatibility
*   **State Machine Design:** Following Heltec's recommended state machine approach for device operation
*   **DMX Control:** Custom UART-based DMX512 implementation for ESP32S3 compatibility
*   **Command Processing:** JSON-based command structure for flexible fixture control

## Technology Stack

### Hardware
*   **Microcontroller:** Heltec WiFi LoRa 32 V3
    - ESP32S3 dual-core processor
    - SX126x LoRa radio module
    - Built-in OLED display
*   **Radio:** SX126x with native Heltec driver support
*   **DMX Interface:** MAX485 or similar RS-485 transceiver

### Software
*   **Framework:** Arduino framework on ESP32
*   **Build System:** PlatformIO
*   **Key Libraries:**
    - Heltec ESP32 Dev-Boards (official LoRaWAN support)
    - ArduinoJson for command parsing
    - FreeRTOS for task management
*   **Custom Components:**
    - DMX controller implementation
    - JSON command processor
    - Message queue handler

## API Patterns

### LoRaWAN Communication
*   **Activation:** OTAA (Over-The-Air Activation)
*   **Class:** Class C for continuous reception
*   **Region:** US915 with configurable channel masks
*   **Security:** Standard LoRaWAN AES-128 encryption

### Command Structure
*   **Format:** JSON payloads
*   **Transport:** LoRaWAN downlink messages
*   **Processing:** ArduinoJson parser with error handling
*   **Queueing:** Priority-based message queue

## Database Schema
Not applicable - embedded system without persistent storage.

## Configuration Management
*   **Build Configuration:** PlatformIO
*   **LoRaWAN Parameters:** Defined in secrets.h
*   **Hardware Pins:** Defined in board-specific headers
*   **Debug Levels:** Configurable via build flags 