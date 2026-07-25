#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED
} lcd_wifi_status_t;

typedef enum {
    PC_STATUS_OFF,
    PC_STATUS_ON,
    PC_STATUS_BOOTING
} lcd_pc_status_t;

/**
 * @brief Initialize the Waveshare ESP32-C6-GEEK LCD display (ST7789 240x135) and backlight.
 */
esp_err_t lcd_display_init(void);

/**
 * @brief Update Wi-Fi status and displayed IP address.
 */
void lcd_display_update_wifi(lcd_wifi_status_t status, const char *ip_str);

/**
 * @brief Update PC power status (ON / OFF / BOOTING).
 */
void lcd_display_update_pc(lcd_pc_status_t status);

/**
 * @brief Set custom message line on the LCD display.
 */
void lcd_display_set_message(const char *msg);

/**
 * @brief Enable or disable the LCD backlight.
 */
void lcd_display_set_backlight(bool enable);

#ifdef __cplusplus
}
#endif

#endif // LCD_DISPLAY_H
