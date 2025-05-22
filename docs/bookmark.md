# LoRa-DMX Project: Refactoring & Troubleshooting Summary

## Completed Changes & Current State

1.  **LoRaWAN Implementation Overhaul:**
    *   **Abandoned MCCI LMIC:** After extensive troubleshooting (build errors, runtime crashes related to SX1262, Class C, and internal library issues like `radio.c`), the MCCI LMIC approach was abandoned for the Heltec WiFi LoRa 32 V3.
    *   **Adopted Heltec LoRaWAN Library:** Switched to using `heltecautomation/Heltec ESP32 Dev-Boards` library.
    *   **Created `HeltecLoRaWANWrapper`:** Developed a custom wrapper (`lib/HeltecLoRaWANWrapper/Heltec_LoRaWAN_Wrapper.h`) to simplify using the Heltec library for Class C LoRaWAN operations, including downlink and connection status callbacks.
    *   **Integrated Wrapper into `main.cpp`:** Refactored `src/main.cpp` to use the new wrapper, manage connection state, and handle uplinks/downlinks.
    *   **Credential Management:** LoRaWAN credentials (DevEUI, AppEUI, AppKey) are sourced from `include/secrets.h` by the wrapper.
    *   **PlatformIO Configuration for Heltec Library:** Successfully identified and applied necessary build flags (`-DHELTEC_BOARD=30`, `-DSLOW_CLK_TPYE=1`, `-DWIFI_LORA_32_V3`) in `platformio.ini` to ensure the Heltec library initializes correctly for the board and radio.
    *   Removed all MCCI LMIC related files and configurations (e.g., `lmic_project_config.h`, old `LoRaWANManager`).

2.  **DMX Integration (Ongoing Troubleshooting):**
    *   The project uses `someweisguy/esp_dmx` library for DMX control, wrapped by `DmxController`.
    *   Currently facing compilation errors within `esp_dmx` (e.g., `ESP_INTR_FLAG_IRAM undefined`, `timer_group_t undefined`).
    *   **Root Cause Identified:** These errors are due to an ESP-IDF version mismatch. The current `esp_dmx @ ^4.1.0` expects ESP-IDF v5.x APIs, but the project's Arduino core (`framework-arduinoespressif32 @ 3.20017...`) is based on ESP-IDF v4.4.7.

## Pending Issues & Next Steps

1.  **Resolve DMX Library Compilation Errors:**
    *   **Current Strategy:** Attempting to use an older, compatible version of `someweisguy/esp_dmx`. The `platformio.ini` was just updated to try `someweisguy/esp_dmx @ 2.0.2` (previously tried `^4.1.0` which caused errors, and `~2.0.2` which PlatformIO couldn't find).
    *   **Next Action:** Re-compile the project with `esp_dmx @ 2.0.2` to see if this resolves the ESP-IDF API mismatch errors.
    *   **Alternative if `2.0.2` fails or is too old:** Systematically try other versions of `esp_dmx` that predate explicit ESP-IDF v5.x support, or explore upgrading the entire project environment to use Arduino Core v3.x (based on ESP-IDF v5.x), though this is a more significant change.

2.  **Code Cleanup & Verification:**
    *   Review `platformio.ini` for any other potentially problematic or redundant library entries (e.g., the `ESP32 DMX` line was just removed).
    *   Once DMX compilation is successful, ensure DMX functionality (`DmxController` and its usage in `main.cpp`) is still correct and compatible with the chosen `esp_dmx` library version.

3.  **Testing Requirements (after compilation successes):**
    *   Thoroughly test LoRaWAN Class C join, uplink, and downlink with the Heltec wrapper.
    *   Verify DMX output control is functional.
    *   Test combined operation: receiving DMX commands via LoRaWAN and outputting them.

4.  **Future Enhancements (Post-Core Functionality):**
    *   Implement more robust error recovery for LoRa and DMX.
    *   Enhance documentation with full setup and usage examples.
    *   Consider power-saving features if battery operation becomes a requirement.

## Implementation Notes

The project has pivoted significantly in its LoRaWAN strategy. The focus is now on the Heltec native libraries for stability on their hardware. The immediate critical path is resolving the DMX library compatibility to achieve a clean build, then proceeding to functional testing. 