#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display timeout constants
#define SETTINGS_DISPLAY_TIMEOUT_10S  10000
#define SETTINGS_DISPLAY_TIMEOUT_20S  20000
#define SETTINGS_DISPLAY_TIMEOUT_30S  30000
#define SETTINGS_DISPLAY_TIMEOUT_1MIN 60000

// ===== INITIALIZATION =====
/**
 * @brief Initialize settings system
 */
void settings_init(void);

// ===== DISPLAY SETTINGS =====
/**
 * @brief Set display brightness level
 * @param level Brightness level (0-100)
 */
void settings_set_brightness(uint8_t level);

/**
 * @brief Get display brightness level
 * @return Brightness level (0-100)
 */
uint8_t settings_get_brightness(void);

/**
 * @brief Set display timeout
 * @param timeout Timeout in milliseconds
 */
void settings_set_display_timeout(uint32_t timeout);

/**
 * @brief Get display timeout
 * @return Timeout in milliseconds
 */
uint32_t settings_get_display_timeout(void);

// ===== WATCHFACE SETTINGS =====
/**
 * @brief Get current watchface style
 * @return Style index (0-6)
 */
uint8_t settings_get_watchface_style(void);

/**
 * @brief Set watchface style
 * @param style Style index (0-6)
 */
void settings_set_watchface_style(uint8_t style);

/**
 * @brief Save wallpaper path to NVS
 * @param filepath Path to wallpaper file
 * @return ESP_OK on success
 */
esp_err_t settings_set_wallpaper(const char* filepath);

/**
 * @brief Get saved wallpaper path from NVS
 * @param filepath Buffer to store path (should be at least 256 bytes)
 * @param max_len Maximum buffer size
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no wallpaper saved
 */
esp_err_t settings_get_wallpaper(char* filepath, size_t max_len);

/**
 * @brief Get wallpaper dimensions if it's a RAW file
 * @param width Output for width
 * @param height Output for height
 * @return ESP_OK if dimensions are saved
 */
esp_err_t settings_get_wallpaper_dimensions(uint16_t* width, uint16_t* height);

/**
 * @brief Save wallpaper dimensions for RAW files
 * @param width Image width
 * @param height Image height
 * @return ESP_OK on success
 */
esp_err_t settings_set_wallpaper_dimensions(uint16_t width, uint16_t height);

// ===== TIME & DATE SETTINGS =====
/**
 * @brief Set time format preference
 * @param is_24h true for 24-hour format, false for 12-hour format
 */
void settings_set_time_format_24h(bool is_24h);

/**
 * @brief Get time format preference
 * @return true for 24-hour format, false for 12-hour format
 */
bool settings_get_time_format_24h(void);

/**
 * @brief Save current time to NVS
 * @param time Time structure to save
 * @return ESP_OK on success
 */
esp_err_t settings_save_time(const struct tm* time);

/**
 * @brief Load saved time from NVS
 * @param time Output time structure
 * @return ESP_OK on success
 */
esp_err_t settings_load_time(struct tm* time);

// ===== AUDIO SETTINGS =====
/**
 * @brief Enable or disable sound
 * @param enabled true to enable sound
 */
void settings_set_sound(bool enabled);

/**
 * @brief Get sound enabled state
 * @return true if sound is enabled
 */
bool settings_get_sound(void);

/**
 * @brief Set notification volume
 * @param vol_percent Volume percentage (0-100)
 */
void settings_set_notify_volume(uint8_t vol_percent);

/**
 * @brief Get notification volume
 * @return Volume percentage (0-100)
 */
uint8_t settings_get_notify_volume(void);

// ===== BLUETOOTH SETTINGS =====
/**
 * @brief Enable or disable Bluetooth
 * @param enabled true to enable Bluetooth
 */
void settings_set_bluetooth_enabled(bool enabled);

/**
 * @brief Get Bluetooth enabled state
 * @return true if Bluetooth is enabled
 */
bool settings_get_bluetooth_enabled(void);

// ===== HEALTH SETTINGS =====
/**
 * @brief Set daily step goal
 * @param steps Number of steps
 */
void settings_set_step_goal(uint32_t steps);

/**
 * @brief Get daily step goal
 * @return Number of steps
 */
uint32_t settings_get_step_goal(void);

// ===== PERSISTENCE =====
/**
 * @brief Save all settings to SPIFFS JSON
 * @return true on success
 */
bool settings_save(void);

/**
 * @brief Load all settings from SPIFFS JSON
 * @return true on success
 */
bool settings_load(void);

/**
 * @brief Reset all settings to factory defaults
 * @return true on success
 */
bool settings_reset_defaults(void);

/**
 * @brief Format SPIFFS storage partition
 * @return true on success
 */
bool settings_format_spiffs(void);

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_H