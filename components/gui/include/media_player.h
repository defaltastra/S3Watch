#pragma once
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif
// Add these function declarations to media_player.h

/**
 * @brief Show image with optional raw dimensions
 * @param filepath Path to image file
 * @param raw_w Width for raw images (0 for auto-detect)
 * @param raw_h Height for raw images (0 for auto-detect)
 */
 void media_viewer_show_image_fast(const char* filepath, uint16_t raw_w, uint16_t raw_h);

 /**
  * @brief Set watchface background with optional raw dimensions
  * @param filepath Path to image file
  * @param raw_w Width for raw images (0 for auto-detect)
  * @param raw_h Height for raw images (0 for auto-detect)
  * @return ESP_OK on success
  */
 esp_err_t watchface_set_background_from_file_fast(const char* filepath, uint16_t raw_w, uint16_t raw_h);
// Play MP3 audio file
esp_err_t media_player_play_mp3(const char* filepath);

esp_err_t media_player_init_lvgl_fs(void);
// Show image in full-screen viewer
void media_viewer_show_image(const char* filepath);

// Set watchface background from image file
esp_err_t watchface_set_background_from_file(const char* filepath);

#ifdef __cplusplus
}
#endif

