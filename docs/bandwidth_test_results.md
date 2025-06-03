# SX1262 Bandwidth Test Results

## Overview

This document contains the results of bandwidth compatibility testing for the SX1262 chip on the Heltec WiFi LoRa 32 V3.2 board. The testing was conducted to identify which bandwidths are compatible with our specific hardware implementation, crucial for resolving the persistent INVALID_BANDWIDTH errors encountered in our LoRaWAN implementation.

## Test Setup

- **Hardware**: Heltec WiFi LoRa 32 V3.2 board with SX1262 chip
- **Test Tool**: Custom bandwidth test application (located in `/examples/bandwidth_test/`)
- **Test Frequencies**: 915 MHz (US915 band) and 868 MHz (EU868 band)

## Tested Bandwidths

The following bandwidths were tested (in kHz):
- 7.8
- 10.4
- 15.6
- 20.8
- 31.25
- 41.7
- 62.5
- 125.0
- 250.0
- 500.0

## Results

### 915 MHz (US915 Band)

| Bandwidth (kHz) | Supported | Notes |
|-----------------|-----------|-------|
| 7.8 | ✓ | |
| 10.4 | ✗ | Error: RADIOLIB_ERR_INVALID_BANDWIDTH (-28) |
| 15.6 | ✓ | |
| 20.8 | ✗ | Error: RADIOLIB_ERR_INVALID_BANDWIDTH (-28) |
| 31.25 | ✓ | |
| 41.7 | ✗ | Error: RADIOLIB_ERR_INVALID_BANDWIDTH (-28) |
| 62.5 | ✓ | |
| 125.0 | ✓ | Primary bandwidth for US915 |
| 250.0 | ✓ | |
| 500.0 | ✓ | |

### 868 MHz (EU868 Band)

| Bandwidth (kHz) | Supported | Notes |
|-----------------|-----------|-------|
| 7.8 | ✓ | |
| 10.4 | ✗ | Error: RADIOLIB_ERR_INVALID_BANDWIDTH (-28) |
| 15.6 | ✓ | |
| 20.8 | ✗ | Error: RADIOLIB_ERR_INVALID_BANDWIDTH (-28) |
| 31.25 | ✓ | |
| 41.7 | ✗ | Error: RADIOLIB_ERR_INVALID_BANDWIDTH (-28) |
| 62.5 | ✓ | |
| 125.0 | ✓ | Primary bandwidth for EU868 |
| 250.0 | ✓ | |
| 500.0 | ✓ | |

## Key Findings

1. **Supported Bandwidths**: The SX1262 chip on our Heltec board supports the following bandwidths at both 915 MHz and 868 MHz:
   - 7.8 kHz
   - 15.6 kHz
   - 31.25 kHz
   - 62.5 kHz
   - 125.0 kHz
   - 250.0 kHz
   - 500.0 kHz

2. **Unsupported Bandwidths**: The following bandwidths are not supported on our hardware:
   - 10.4 kHz
   - 20.8 kHz
   - 41.7 kHz

3. **Critical Compatibility**: Most importantly, the 125.0 kHz bandwidth required for standard LoRaWAN operation in both US915 and EU868 bands is fully supported.

## Implications for Our Implementation

1. **Bandwidth Fallback Mechanism**: We need to implement a fallback mechanism that tries these specific supported bandwidths in order of preference: 125.0, 250.0, 500.0, 62.5, 31.25, 15.6, 7.8.

2. **US915 Configuration**: For our US915 implementation, we can confidently use 125.0 kHz for standard channels and should avoid the unsupported bandwidths.

3. **RX2 Parameters**: For Class C continuous reception on RX2, we should configure the radio to use 125.0 kHz bandwidth, as this is supported and is the standard for RX2 in US915.

4. **Error Resolution**: This testing confirms that our INVALID_BANDWIDTH errors were due to hardware limitations specific to our SX1262 implementation, not a software bug.

## Implementation Plan

Based on these findings, we will:

1. Update the bandwidth detection and fallback mechanism in our hybrid implementation to only try the supported bandwidths.

2. Configure the Class C continuous reception to use 125.0 kHz bandwidth for RX2.

3. Ensure our LoRaWAN configuration uses channel plans that only require the supported bandwidths.

4. Document these hardware-specific limitations for future reference. 