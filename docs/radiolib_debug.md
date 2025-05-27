# LoRaWAN Troubleshooting Log

## Initial Errors

The project was experiencing multiple errors:

1. **Multiple definition errors** during linking:
   - Multiple definitions of `heltec_led`, `display`, `radio`, etc.
   - Example: `multiple definition of 'heltec_led(int)'`

2. **LoRaWAN join failure** with error code -28 (RADIOLIB_ERR_INVALID_BANDWIDTH)

## Steps Taken

### 1. Fixed Multiple Definition Errors

The root cause was that `heltec_unofficial.h` was being included in both `src/main.cpp` and `lib/LoRaWANHelper/LoRaWANHelper.cpp`. This header defined global objects like `radio` and `display`, causing the linker to see multiple definitions.

**Solution:**
- Removed `#include <heltec_unofficial.h>` from `LoRaWANHelper.cpp`
- Added explicit `#define DIO1 GPIO_NUM_14` to `LoRaWANHelper.cpp` to maintain access to the DIO1 pin constant
- Added `#include <Arduino.h>` to ensure ESP32 pin definitions were available

This resolved the multiple definition errors, but we still had the LoRaWAN join issue.

### 2. Added Enhanced Debugging

To diagnose the join failure with error -28 (INVALID_BANDWIDTH):

- Added extensive debug output in `lorawan_helper_join()`
- Added error translation for RadioLib error codes
- Printed out the LoRaWAN credentials to verify they were being parsed correctly
- Added band configuration checking

The debug output confirmed:
- LoRaWAN credentials (APPEUI, DEVEUI, APPKEY) were being read correctly
- The band configuration (US915) was set
- The error consistently occurred at `node->beginOTAA()` with error -28

### 3. Attempted Radio Parameter Modification

We tried several approaches to address the bandwidth error:

1. **First Attempt:** Directly modifying the radio parameters before calling beginOTAA
   - This led to error -20 (INVALID_MODE/INVALID_OP_MODE), indicating the radio was in the wrong state to accept changes

2. **Second Attempt:** Creating a custom US915 band with modified bandwidth settings
   - This failed because the LoRaWANBand_t structure in RadioLib 7.1.2 doesn't have the field names we assumed
   - Compilation errors for fields like `rxBandwidth`, `uplinkFreq`, and `rx2Freq`

3. **Final Approach:** Simplified code using standard US915 band
   - Removed all attempts to modify band parameters directly
   - Used the standard US915 band definition from RadioLib
   - Fixed build errors related to structure field references
   - Corrected error code references (RADIOLIB_ERR_INVALID_OP_MODE → RADIOLIB_ERR_INVALID_MODE)

This final approach resulted in a successful build, eliminating all compiler errors.

### 4. Current Status

The firmware now builds successfully and uploads to the device. However, **we're still seeing the same error -28 (INVALID_BANDWIDTH)** when attempting to join. This suggests the issue is not with our code structure, but with a fundamental incompatibility between:

1. RadioLib's US915 band configuration
2. The Heltec hardware capabilities
3. Possibly the particular SX1262 chip in this board

### 5. Attempted to create direct SX1262 test

We tried to create a basic LoRa test to verify if the SX1262 radio could work in simple LoRa mode (without LoRaWAN). However, we faced multiple definition errors due to the `heltec_unofficial.h` header being included in multiple files.

### 6. Analyzed RadioLib US915 Band Configuration

We looked at the RadioLib source code to understand how the US915 band is defined. The key findings are:

1. **US915 Data Rates**:
   - DR0: LoRa SF10/BW125 (uplink channels 0-63)
   - DR1: LoRa SF9/BW125 (uplink channels 0-63)
   - DR2: LoRa SF8/BW125 (uplink channels 0-63)
   - DR3: LoRa SF7/BW125 (uplink channels 0-63)
   - DR4: LoRa SF8/BW500 (uplink channels 64-71)
   - DR8: LoRa SF12/BW500 (downlink channels)
   - DR9: LoRa SF11/BW500 (downlink channels)
   - DR10: LoRa SF10/BW500 (downlink channels)
   - DR11: LoRa SF9/BW500 (downlink channels)
   - DR12: LoRa SF8/BW500 (downlink channels)
   - DR13: LoRa SF7/BW500 (downlink channels)

2. **Potential Issue**:
   - The US915 band uses 500 kHz bandwidth for some data rates
   - The SX1262 supports 500 kHz bandwidth in theory
   - However, our testing shows error -28 (INVALID_BANDWIDTH) is occurring
   - This suggests the specific hardware implementation may not support the 500 kHz bandwidth even though the chip technically should

3. **Specific RX2 Configuration**:
   - RX2 frequency: 923.3 MHz
   - RX2 data rate: DR8 (SF12/BW500)
   - This wide bandwidth (500 kHz) may be causing the issue if the hardware doesn't support it

## Advanced Debugging Needed

Based on the persistent INVALID_BANDWIDTH error, we need to dig deeper:

1. **Create standalone test project:**
   - To avoid compilation conflicts, create a completely separate PlatformIO project
   - Test only the SX1262 radio in basic LoRa mode with different bandwidths
   - Determine which bandwidths are actually supported by the hardware

2. **RadioLib Internals Investigation:**
   - Examine how the US915 band is defined internally in RadioLib 
   - Understand what happens in `beginOTAA()` that triggers the INVALID_BANDWIDTH error
   - Look at the exact bandwidth values being set vs. what the SX1262 supports

3. **Hardware Investigation:**
   - Check the SX1262 datasheet for supported bandwidth values
   - Compare the INVALID_BANDWIDTH error with the hardware capabilities
   - Look for known limitations with the Heltec V3 implementation

## Next Steps (Recommended Action Plan)

1. **Create a simple standalone PlatformIO project:**
   ```
   mkdir -p ~/LoRa-DMX-Test
   cd ~/LoRa-DMX-Test
   pio init --board heltec_wifi_lora_32_V3
   ```

2. **Create a basic test sketch that:**
   - Iterates through all possible bandwidths (7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500 kHz)
   - Attempts to initialize the SX1262 with each bandwidth
   - Reports which bandwidths work and which fail

3. **If 500 kHz bandwidth is confirmed not working:**
   - Try to modify the US915 band definition to use a supported bandwidth
   - For example, create a custom band that uses 250 kHz instead of 500 kHz
   - This may require deeper changes to the RadioLib library

4. **Consider alternative approaches:**
   - Try a different LoRaWAN library if available
   - Try a different radio configuration (frequency, bandwidth, spreading factor)
   - Check if there's a Heltec-specific LoRaWAN implementation that might work better
   - Consider downgrading RadioLib to an earlier version

## Raw Debug Output

```
19:37:17.262 > [LoRaWANHelper] Using standard US915 band...
19:37:17.266 > [LoRaWANHelper] LoRaWANNode created.
19:37:17.269 > [LoRaWANHelper] Starting OTAA join...
19:37:17.273 > [LoRaWANHelper] OTAA parameters:
19:37:17.276 >   Join EUI: 0xED733220D2A9F133
19:37:17.278 >   Dev EUI: 0x90CFF868EF8BD4CC
19:37:17.281 >   App Key: 0xF7EDCFE4617E66701665A13A2B76DD52
19:37:17.285 > [LoRaWANHelper] Checking radio configuration...
19:37:17.290 >   Band is configured.
19:37:17.291 > [LoRaWANHelper] Using standard US915 band settings...
19:37:17.296 > [LoRaWANHelper] Calling beginOTAA...
19:37:17.299 > [LoRaWANHelper] Failed to configure OTAA, error: -28
19:37:17.304 >   Refer to RadioLib error codes for details
19:37:17.801 > LoRaWAN Join failed.
```

## SX1262 Supported Bandwidths (From Datasheet)

According to the SX1262 datasheet, these are the supported bandwidths in LoRa mode:

- 7.8 kHz
- 10.4 kHz
- 15.6 kHz
- 20.8 kHz
- 31.25 kHz
- 41.7 kHz
- 62.5 kHz
- 125 kHz
- 250 kHz
- 500 kHz

The US915 band uses 500 kHz bandwidth for some data rates (including the default RX2 settings), which might be causing the INVALID_BANDWIDTH error if this specific implementation of the SX1262 doesn't support it. 