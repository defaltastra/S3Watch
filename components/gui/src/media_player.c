#include "media_player.h"
#include "ui.h"
#include "ui_fonts.h"
#include "esp_log.h"
#include "audio_alert.h"
#include "watchface.h"
#include "settings.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Include minimp3 - it's a header-only library
// We need to define MINIMP3_IMPLEMENTATION in one source file
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
// Note: MINIMP3_ONLY_SIMD removed - ESP32-S3 doesn't have SSE/NEON support that minimp3 checks for
#include "minimp3.h"

static const char* TAG = "MediaPlayer";

// MP3 playback task
static TaskHandle_t s_mp3_task = NULL;
static bool s_mp3_playing = false;

// MP3 playback using minimp3 decoder
static void mp3_play_task(void* pv)
{
    const char* filepath = (const char*)pv;
    ESP_LOGI(TAG, "MP3 playback started: %s", filepath);
    
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open MP3 file: %s", filepath);
        free((void*)filepath);
        s_mp3_playing = false;
        s_mp3_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Initialize audio
    if (audio_alert_init() != ESP_OK) {
        ESP_LOGE(TAG, "Audio init failed");
        fclose(f);
        free((void*)filepath);
        s_mp3_playing = false;
        s_mp3_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Get speaker handle from audio_alert (it manages the speaker)
    // We'll use the audio_alert's speaker handle
    extern esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
    esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
    if (!spk) {
        ESP_LOGE(TAG, "Speaker init failed");
        fclose(f);
        free((void*)filepath);
        s_mp3_playing = false;
        s_mp3_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Ensure audio is initialized
    audio_alert_init();
    
    // Read file in chunks and decode MP3
    // For now, use a simple approach: read and decode frame by frame
    // Note: This is a simplified implementation. For full MP3 support, 
    // minimp3 library needs to be properly integrated.
    
    // Simple file-based MP3 playback using frame-by-frame decoding
    // We'll use the basic minimp3 API if available, otherwise fall back to file streaming
    uint8_t* mp3_buf = malloc(4096);
    int16_t* pcm_buf = malloc(1152 * 2 * sizeof(int16_t)); // Max samples per frame * channels
    
    if (!mp3_buf || !pcm_buf) {
        ESP_LOGE(TAG, "Memory allocation failed");
        if (mp3_buf) free(mp3_buf);
        if (pcm_buf) free(pcm_buf);
        fclose(f);
        free((void*)filepath);
        s_mp3_playing = false;
        s_mp3_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Initialize MP3 decoder
    mp3dec_t mp3d;
    mp3dec_init(&mp3d);
    mp3dec_frame_info_t info;
    
    // Configure audio codec - we'll get sample rate/channels from first frame
    esp_codec_dev_sample_info_t fs = {0};
    bool audio_opened = false;
    int vol = (int)settings_get_notify_volume();
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    
    // Read and decode MP3 frames in a loop
    size_t mp3_buf_size = 4096;
    size_t mp3_buf_pos = 0;
    
    while (s_mp3_playing) {
        // Read more data if buffer is low
        if (mp3_buf_pos + 1440 > mp3_buf_size) {
            // Move remaining data to start
            size_t remaining = mp3_buf_size - mp3_buf_pos;
            if (remaining > 0 && remaining < mp3_buf_size) {
                memmove(mp3_buf, mp3_buf + mp3_buf_pos, remaining);
            }
            mp3_buf_pos = 0;
            mp3_buf_size = remaining;
            
            // Read more data
            size_t to_read = 4096 - mp3_buf_size;
            size_t n = fread(mp3_buf + mp3_buf_size, 1, to_read, f);
            if (n == 0) {
                // End of file
                break;
            }
            mp3_buf_size += n;
        }
        
        // Decode one frame
        int samples = mp3dec_decode_frame(&mp3d, mp3_buf + mp3_buf_pos, 
                                         mp3_buf_size - mp3_buf_pos, pcm_buf, &info);
        
        if (samples > 0 && info.frame_bytes > 0) {
            // Update buffer position
            mp3_buf_pos += info.frame_bytes;
            
            // Configure audio on first frame
            if (!audio_opened) {
                fs.sample_rate = info.hz;
                fs.channel = info.channels;
                fs.bits_per_sample = 16;
                
                if (esp_codec_dev_open(spk, &fs) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to open audio codec");
                    break;
                }
                esp_codec_dev_set_out_vol(spk, vol);
                esp_codec_dev_set_out_mute(spk, false);
                audio_opened = true;
                ESP_LOGI(TAG, "Audio opened: %d Hz, %d channels", info.hz, info.channels);
            }
            
            // Write PCM data to audio output
            size_t pcm_samples = samples * info.channels;
            size_t written = 0;
            while (written < pcm_samples && s_mp3_playing) {
                size_t to_write = pcm_samples - written;
                if (to_write > 256) to_write = 256;
                
                if (esp_codec_dev_write(spk, pcm_buf + written, 
                                        to_write * sizeof(int16_t)) != ESP_OK) {
                    ESP_LOGW(TAG, "Audio write failed");
                    break;
                }
                written += to_write;
            }
        } else if (info.frame_bytes > 0) {
            // Skip invalid frame
            mp3_buf_pos += info.frame_bytes;
        } else {
            // Need more data or end of stream
            if (mp3_buf_pos >= mp3_buf_size) {
                // No more data available
                break;
            }
            // Try to find next sync word by skipping a byte
            mp3_buf_pos++;
        }
    }
    
    // Cleanup
    if (audio_opened) {
        esp_codec_dev_set_out_mute(spk, true);
        vTaskDelay(pdMS_TO_TICKS(100)); // Let audio drain
        esp_codec_dev_close(spk);
    }
    
    free(mp3_buf);
    free(pcm_buf);
    fclose(f);
    free((void*)filepath);
    
    ESP_LOGI(TAG, "MP3 playback finished");
    s_mp3_playing = false;
    s_mp3_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t media_player_play_mp3(const char* filepath)
{
    if (!filepath) return ESP_ERR_INVALID_ARG;
    
    // Stop any currently playing file
    if (s_mp3_playing && s_mp3_task) {
        s_mp3_playing = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Check if file exists
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }
    
    char* path_copy = strdup(filepath);
    if (!path_copy) return ESP_ERR_NO_MEM;
    
    s_mp3_playing = true;
    // Increased stack size for MP3 decoding - minimp3 needs significant stack space
    // mp3dec_t structure is ~6KB, plus function call stack and local variables
    xTaskCreate(mp3_play_task, "mp3_play", 24576, path_copy, 5, &s_mp3_task);
    
    return ESP_OK;
}

// Image viewer screen
static lv_obj_t* s_image_viewer = NULL;

static void image_viewer_close_cb(lv_event_t* e)
{
    if (s_image_viewer) {
        lv_obj_del(s_image_viewer);
        s_image_viewer = NULL;
    }
}

void media_viewer_show_image(const char* filepath)
{
    if (!filepath) return;
    
    // Close existing viewer if open
    if (s_image_viewer) {
        lv_obj_del(s_image_viewer);
        s_image_viewer = NULL;
    }
    
    // Get a valid parent - use active screen if layer_top is not available
    lv_obj_t* parent = lv_layer_top();
    if (!parent) {
        parent = lv_scr_act();
    }
    if (!parent) {
        ESP_LOGE(TAG, "No valid parent for image viewer");
        return;
    }
    
    // Create full-screen image viewer
    s_image_viewer = lv_obj_create(parent);
    if (!s_image_viewer) {
        ESP_LOGE(TAG, "Failed to create image viewer");
        return;
    }
    
    lv_obj_remove_style_all(s_image_viewer);
    lv_obj_set_size(s_image_viewer, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_image_viewer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_image_viewer, LV_OPA_COVER, 0);
    
    // Try to load image using LVGL file system
    char lvgl_path[256];
    if (strncmp(filepath, "/sdcard/", 8) == 0) {
        // SD card file - LVGL can't directly access without SD card driver
        // Show informative message
        lv_obj_t* msg = lv_label_create(s_image_viewer);
        if (msg) {
            lv_label_set_text(msg, "SD card image\n(Requires SD card\nLVGL driver)");
            lv_obj_center(msg);
            lv_obj_set_style_text_color(msg, lv_color_white(), 0);
            lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
        }
        ESP_LOGW(TAG, "SD card image viewing requires LVGL SD card driver setup: %s", filepath);
    } else if (strncmp(filepath, "/spiffs/", 8) == 0) {
        snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", filepath + 8);
        lv_obj_t* img = lv_image_create(s_image_viewer);
        if (img) {
            lv_image_set_src(img, lvgl_path);
            lv_obj_center(img);
        }
    } else {
        lv_obj_t* msg = lv_label_create(s_image_viewer);
        if (msg) {
            lv_label_set_text(msg, "Invalid image path");
            lv_obj_center(msg);
            lv_obj_set_style_text_color(msg, lv_color_white(), 0);
        }
    }
    
    // Add close button
    lv_obj_t* btn_close = lv_btn_create(s_image_viewer);
    if (btn_close) {
        lv_obj_set_pos(btn_close, 10, 10);
        lv_obj_t* lbl_close = lv_label_create(btn_close);
        if (lbl_close) {
            lv_label_set_text(lbl_close, "X");
        }
        lv_obj_add_event_cb(btn_close, image_viewer_close_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_image_viewer) {
        lv_obj_add_event_cb(s_image_viewer, image_viewer_close_cb, LV_EVENT_GESTURE, NULL);
    }
}

// Watchface background changer
esp_err_t watchface_set_background_from_file(const char* filepath)
{
    if (!filepath) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Setting watchface background from: %s", filepath);
    
    // Get watchface screen and background image
    extern lv_obj_t* watchface_screen_get(void);
    lv_obj_t* wf = watchface_screen_get();
    if (!wf) {
        ESP_LOGE(TAG, "Watchface screen not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Find background image object (it's the first image child)
    lv_obj_t* bg_img = NULL;
    uint32_t child_cnt = lv_obj_get_child_cnt(wf);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(wf, i);
        if (lv_obj_check_type(child, &lv_image_class)) {
            bg_img = child;
            break;
        }
    }
    
    if (!bg_img) {
        ESP_LOGE(TAG, "Background image not found in watchface");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Try to set image source from file
    // For SPIFFS files, use LVGL file system path
    char lvgl_path[256];
    if (strncmp(filepath, "/spiffs/", 8) == 0) {
        snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", filepath + 8);
        lv_image_set_src(bg_img, lvgl_path);
        ESP_LOGI(TAG, "Watchface background set to: %s", lvgl_path);
        return ESP_OK;
    } else if (strncmp(filepath, "/sdcard/", 8) == 0) {
        // SD card files need special handling - copy to SPIFFS or use SD card driver
        ESP_LOGW(TAG, "SD card watchface backgrounds require SD card LVGL driver");
        return ESP_ERR_NOT_SUPPORTED;
    } else {
        ESP_LOGE(TAG, "Invalid file path: %s", filepath);
        return ESP_ERR_INVALID_ARG;
    }
}

