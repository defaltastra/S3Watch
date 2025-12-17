#ifndef WATCHFACE_H
#define WATCHFACE_H

#include "lvgl.h"
#include "esp_err.h"

// Watchface style enumeration
typedef enum {
    WATCHFACE_STYLE_CLASSIC = 0,    // Original: Large colorful numbers
    WATCHFACE_STYLE_MINIMAL,        // Minimalist: Single line, white
    WATCHFACE_STYLE_DIGITAL,        // Digital clock style: HH:MM:SS
    WATCHFACE_STYLE_GRUVBOX,        // Gruvbox warm retro colors
    WATCHFACE_STYLE_MONOCHROME,     // Black & white
    WATCHFACE_STYLE_NEON,           // Bright neon colors
    WATCHFACE_STYLE_PASTEL,         // Soft pastel colors
    WATCHFACE_STYLE_ANALOG_CLASSIC, // Large traditional analog clock
    WATCHFACE_STYLE_ANALOG_MODERN,  // Medium colorful analog clock
    WATCHFACE_STYLE_ANALOG_MINIMAL, // Small minimalist analog clock
    WATCHFACE_STYLE_ANALOG_SKELETON,// Transparent skeleton analog clock
    WATCHFACE_STYLE_COUNT
} watchface_style_t;

/**
 * @brief Create the watchface screen
 * @param parent Parent object (NULL for standalone screen)
 */
void watchface_create(lv_obj_t* parent);

/**
 * @brief Get the watchface screen object
 * @return Pointer to watchface screen
 */
lv_obj_t* watchface_screen_get(void);

/**
 * @brief Update power state display
 * @param vbus_in USB power connected
 * @param charging Battery is charging
 * @param battery_percent Battery percentage (0-100)
 */
void watchface_set_power_state(bool vbus_in, bool charging, int battery_percent);

/**
 * @brief Update BLE connection state display
 * @param connected BLE is connected
 */
void watchface_set_ble_connected(bool connected);

/**
 * @brief Set watchface style
 * @param style Style to apply
 */
void watchface_set_style(watchface_style_t style);

/**
 * @brief Get current watchface style
 * @return Current style
 */
watchface_style_t watchface_get_style(void);

/**
 * @brief Load saved background wallpaper
 * @return ESP_OK on success
 */
esp_err_t watchface_load_saved_background(void);

/**
 * @brief Set background from file (fast method)
 * @param filepath Path to image file
 * @param w Width (for RAW images, 0 for auto)
 * @param h Height (for RAW images, 0 for auto)
 * @return ESP_OK on success
 */
esp_err_t watchface_set_background_from_file_fast(const char* filepath, uint16_t w, uint16_t h);

#endif // WATCHFACE_H