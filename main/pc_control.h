#pragma once

#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PC_STATE_OFF,
    PC_STATE_ON,
    PC_STATE_TRANSITIONING,
} pc_power_state_t;

/**
 * @brief Initialize PC control subsystem (BLE HID Keyboard, etc.)
 */
void pc_control_init(void);

/**
 * @brief Get PC power state from USB communication status
 */
pc_power_state_t pc_get_power_state(void);

/**
 * @brief Execute power command from Matter
 * @param want_on true: ON command (sends AnyKey via Keyboard emulation), false: OFF command (no-op by spec)
 */
esp_err_t pc_execute_command(bool want_on);

/**
 * @brief Force shutdown (No-op when dedicated wiring is removed)
 */
esp_err_t pc_execute_force_shutdown(void);

#ifdef __cplusplus
}
#endif
