#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_DISPLAY_TIMEOUT_10S 10000
#define SETTINGS_DISPLAY_TIMEOUT_20S 20000
#define SETTINGS_DISPLAY_TIMEOUT_30S 30000
#define SETTINGS_DISPLAY_TIMEOUT_1MIN 60000
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

// Your existing functions...
bool settings_get_time_format_24h(void);
void settings_set_time_format_24h(bool is_24h);

// Wallpaper functions
esp_err_t settings_set_wallpaper(const char* filepath);
esp_err_t settings_get_wallpaper(char* filepath, size_t max_len);
esp_err_t settings_get_wallpaper_dimensions(uint16_t* width, uint16_t* height);
esp_err_t settings_set_wallpaper_dimensions(uint16_t width, uint16_t height);
esp_err_t settings_save_time(const struct tm* time);
esp_err_t settings_load_time(struct tm* time);
#ifdef __cplusplus
}
#endif

#endif // SETTINGS_H
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
  */
 esp_err_t settings_set_wallpaper_dimensions(uint16_t width, uint16_t height);
 /**
 * @brief Load and apply saved wallpaper from NVS
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no wallpaper saved
 */
esp_err_t watchface_load_saved_background(void);
void settings_init(void);
void settings_set_brightness(uint8_t level);
uint8_t settings_get_brightness(void);
void settings_set_display_timeout(uint32_t timeout);
uint32_t settings_get_display_timeout(void);
void settings_set_sound(bool enabled);
bool settings_get_sound(void);
void settings_set_bluetooth_enabled(bool enabled);
bool settings_get_bluetooth_enabled(void);

// Notification volume (0-100)
void settings_set_notify_volume(uint8_t vol_percent);
uint8_t settings_get_notify_volume(void);

// Persist settings to SPIFFS JSON and load from it
bool settings_save(void);
bool settings_load(void);

// Step goal (daily steps target)
void settings_set_step_goal(uint32_t steps);
uint32_t settings_get_step_goal(void);

// Time format (true = 24h, false = 12h)
void settings_set_time_format_24h(bool enabled);
bool settings_get_time_format_24h(void);

// Restore factory defaults and persist
bool settings_reset_defaults(void);

// Maintenance: format SPIFFS storage partition
bool settings_format_spiffs(void);

#ifdef __cplusplus
}
#endif
