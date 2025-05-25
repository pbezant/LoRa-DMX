#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize DMX (calls DmxController::begin)
void dmx_helper_init();

// Set DMX channels for a fixture (address is 1-based, channels is array of values)
void dmx_helper_set_fixture_channels(int address, const uint8_t* channels, size_t num_channels);

// Update DMX output (send current buffer)
void dmx_helper_update();

// Clear all DMX channels
void dmx_helper_clear();

// Start/stop patterns (pattern type, speed, cycles)
void dmx_helper_start_pattern(int pattern_type, int speed, int cycles);
void dmx_helper_stop_pattern();
void dmx_helper_run_pattern();

// Process a JSON command for DMX control
void dmx_helper_process_json_command(const char* json_payload, size_t length);

// Call in main loop for DMX pattern processing or other periodic DMX tasks
void dmx_helper_loop();

#ifdef __cplusplus
}
#endif 