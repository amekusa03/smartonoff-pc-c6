#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the shutdown client.
 */
esp_err_t shutdown_client_init(void);

/**
 * @brief Send the shutdown trigger to the target PC.
 */
esp_err_t shutdown_client_execute(void);

#ifdef __cplusplus
}
#endif
