#pragma once
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

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

