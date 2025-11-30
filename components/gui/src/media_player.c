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
#include "esp_heap_caps.h"

static const char* TAG = "MediaPlayerOpt";

// -------------------------
// LVGL filesystem driver
// Maps LVGL drive letters to actual mountpoints and normalizes paths.
// 'A:' -> /sdcard/
// 'S:' -> /spiffs/
// If LVGL passes an absolute path (starts with '/'), we use it as-is.

static void normalize_lvgl_path(const char* lv_path, char* out, size_t out_sz)
{
    if (!lv_path || !out) return;
    out[0] = '\0';

    // If LVGL supplies drive letter like "A:foo" or "A:/foo"
    if (strlen(lv_path) >= 2 && lv_path[1] == ':') {
        char drive = lv_path[0];
        const char* rest = lv_path + 2;
        if (*rest == '/') rest++; // skip optional slash
        if (drive == 'A') {
            snprintf(out, out_sz, "/sdcard/%s", rest);
        } else if (drive == 'S') {
            snprintf(out, out_sz, "/spiffs/%s", rest);
        } else {
            // Unknown drive -> pass remainder
            snprintf(out, out_sz, "%s", rest);
        }
        return;
    }

    // Absolute unix path
    if (lv_path[0] == '/') {
        strncpy(out, lv_path, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    // Fallback: assume sdcard
    snprintf(out, out_sz, "/sdcard/%s", lv_path);
}

static void* fs_open_cb(lv_fs_drv_t* drv, const char* path, lv_fs_mode_t mode)
{
    const char* flags = "rb";
    if (mode == LV_FS_MODE_WR) flags = "wb";
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) flags = "r+b";

    char actual[256];
    normalize_lvgl_path(path, actual, sizeof(actual));
    FILE* f = fopen(actual, flags);
    ESP_LOGI(TAG, "fs_open: LVGL->'%s' -> '%s' -> %p", path, actual, f);
    return (void*)f;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t* drv, void* file_p)
{
    FILE* f = (FILE*)file_p;
    if (f) {
        fclose(f);
        ESP_LOGI(TAG, "fs_close: %p", f);
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t* drv, void* file_p, void* buf, uint32_t btr, uint32_t* br)
{
    FILE* f = (FILE*)file_p;
    if (!f) { *br = 0; return LV_FS_RES_UNKNOWN; }
    size_t r = fread(buf, 1, btr, f);
    *br = (uint32_t)r;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t* drv, void* file_p, uint32_t pos, lv_fs_whence_t whence)
{
    FILE* f = (FILE*)file_p;
    if (!f) return LV_FS_RES_UNKNOWN;
    int w = SEEK_SET;
    if (whence == LV_FS_SEEK_CUR) w = SEEK_CUR;
    else if (whence == LV_FS_SEEK_END) w = SEEK_END;
    return (fseek(f, pos, w) == 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t* drv, void* file_p, uint32_t* pos_p)
{
    FILE* f = (FILE*)file_p;
    if (!f) return LV_FS_RES_UNKNOWN;
    long p = ftell(f);
    if (p < 0) return LV_FS_RES_UNKNOWN;
    *pos_p = (uint32_t)p;
    return LV_FS_RES_OK;
}

esp_err_t media_player_init_lvgl_fs(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'A'; // map 'A:' to sdcard
    fs_drv.open_cb = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb = fs_read_cb;
    fs_drv.seek_cb = fs_seek_cb;
    fs_drv.tell_cb = fs_tell_cb;
    lv_fs_drv_register(&fs_drv);
    ESP_LOGI(TAG, "LVGL filesystem driver 'A' registered (maps to /sdcard/)");

    // Also register 'S' for spiffs if you want to keep supporting it
    static lv_fs_drv_t spiffs_drv;
    lv_fs_drv_init(&spiffs_drv);
    spiffs_drv.letter = 'S';
    spiffs_drv.open_cb = fs_open_cb;
    spiffs_drv.close_cb = fs_close_cb;
    spiffs_drv.read_cb = fs_read_cb;
    spiffs_drv.seek_cb = fs_seek_cb;
    spiffs_drv.tell_cb = fs_tell_cb;
    lv_fs_drv_register(&spiffs_drv);
    ESP_LOGI(TAG, "LVGL filesystem driver 'S' registered (maps to /spiffs/)");

    return ESP_OK;
}

// -------------------------
// Fast raw RGB565 loader (instant blit)
// Raw file format expected: width*height*2 bytes (RGB565 little endian)
// No header. Caller must know width/height.
// The routine allocates an lv_img_dsc_t with PSRAM and returns it; free with lv_mem_free() or free().

lv_image_dsc_t *load_raw_rgb565_image(const char *path, uint32_t width, uint32_t height)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE("MediaPlayer", "Failed to open raw file: %s", path);
        return NULL;
    }

    uint32_t size = width * height * 2; // RGB565 = 2 bytes per pixel

    lv_image_dsc_t *dsc = malloc(sizeof(lv_image_dsc_t));
    if (!dsc) {
        fclose(f);
        ESP_LOGE("MediaPlayer", "Failed to alloc lv_image_dsc_t");
        return NULL;
    }

    void *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        fclose(f);
        free(dsc);
        ESP_LOGE("MediaPlayer", "Failed to alloc image buffer (%ld bytes)", size);
        return NULL;
    }

    fread(buf, 1, size, f);
    fclose(f);

    // LVGL 9 IMAGE HEADER
    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;    // LVGL 9 color format
    dsc->header.flags = 0;
    dsc->header.w = width;
    dsc->header.h = height;

    dsc->data_size = size;
    dsc->data = buf;

    return dsc;
}


// -------------------------
// New image viewer that prefers raw RGB565 or direct SD JPG path

static lv_obj_t* s_image_viewer = NULL;
static lv_img_dsc_t* s_raw_dsc = NULL; // if we loaded a raw image
static char* s_current_filepath = NULL;

void media_viewer_close_fast(void)
{
    if (s_image_viewer) {
        lv_obj_del(s_image_viewer);
        s_image_viewer = NULL;
    }
    if (s_raw_dsc) {
        free(s_raw_dsc);
        s_raw_dsc = NULL;
    }
    if (s_current_filepath) { free(s_current_filepath); s_current_filepath = NULL; }
}

void media_viewer_show_image_fast(const char* filepath, uint16_t raw_w, uint16_t raw_h)
{
    if (!filepath) return;

    // Close previous
    media_viewer_close_fast();

    lv_obj_t* parent = lv_layer_top();
    if (!parent) parent = lv_scr_act();

    s_image_viewer = lv_obj_create(parent);
    lv_obj_remove_style_all(s_image_viewer);
    lv_obj_set_size(s_image_viewer, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_image_viewer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_image_viewer, LV_OPA_COVER, 0);

    // Determine extension
    const char* ext = strrchr(filepath, '.');
    char lv_path[256];

    // If user provided a .raw/.rgb565 and supplied dimensions, load as raw
    if (ext && (strcasecmp(ext, ".raw") == 0 || strcasecmp(ext, ".rgb565") == 0) && raw_w > 0 && raw_h > 0) {
        ESP_LOGI(TAG, "Loading raw RGB565 image %s (%dx%d)", filepath, raw_w, raw_h);
        // raw files are absolute paths on the filesystem; allow both sdcard and spiffs
        char actual[256];
        if (filepath[0] == '/') strncpy(actual, filepath, sizeof(actual)-1);
        else snprintf(actual, sizeof(actual), "/sdcard/%s", filepath);

        s_raw_dsc = load_raw_rgb565_image(actual, raw_w, raw_h);
        if (!s_raw_dsc) {
            lv_obj_t* lbl = lv_label_create(s_image_viewer);
            lv_label_set_text(lbl, "Failed to load RAW image");
            lv_obj_center(lbl);
            return;
        }

        lv_obj_t* img = lv_img_create(s_image_viewer);
        lv_img_set_src(img, s_raw_dsc);
        lv_obj_center(img);
        ESP_LOGI(TAG, "Raw image shown instantaneously");
    } else if (ext && (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)) {
        // Use direct SD card path via LVGL driver 'A:' which maps to /sdcard/
        // If filepath already absolute (/sdcard/...), convert to A: format
        if (strncmp(filepath, "/sdcard/", 8) == 0) {
            snprintf(lv_path, sizeof(lv_path), "A:%s", filepath + 8);
        } else {
            // assume given path is relative to sdcard
            snprintf(lv_path, sizeof(lv_path), "A:%s", filepath);
        }
        ESP_LOGI(TAG, "Loading JPG via LVGL path %s", lv_path);
        lv_obj_t* img = lv_img_create(s_image_viewer);
        lv_img_set_src(img, lv_path); // LVGL's TJpgd decoder will handle file reads (faster than SPIFFS PNG)
        lv_obj_center(img);
    } else if (ext && (strcasecmp(ext, ".png") == 0)) {
        // PNG decoding is slow on SD + SPIFFS. We will still load PNG directly from sdcard if asked,
        // but recommend converting to JPG or raw for speed.
        if (strncmp(filepath, "/sdcard/", 8) == 0) {
            snprintf(lv_path, sizeof(lv_path), "A:%s", filepath + 8);
        } else {
            snprintf(lv_path, sizeof(lv_path), "A:%s", filepath);
        }
        ESP_LOGI(TAG, "Loading PNG via LVGL path %s (may be slow)", lv_path);
        lv_obj_t* img = lv_img_create(s_image_viewer);
        lv_img_set_src(img, lv_path);
        lv_obj_center(img);
    } else {
        // Unknown extension: attempt to load as absolute path
        ESP_LOGI(TAG, "Attempt loading as absolute path: %s", filepath);
        lv_obj_t* img = lv_img_create(s_image_viewer);
        lv_img_set_src(img, filepath);
        lv_obj_center(img);
    }

    // store current filepath
    s_current_filepath = strdup(filepath);

    // Add a hint label
    lv_obj_t* hint = lv_label_create(s_image_viewer);
    lv_label_set_text(hint, "Swipe down or right to close");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(hint, lv_color_white(), 0);
    lv_obj_set_style_text_opa(hint, LV_OPA_70, 0);
}

// -------------------------
// Watchface set function: prefer direct SD JPG or raw descriptor

esp_err_t watchface_set_background_from_file_fast(const char* filepath, uint16_t raw_w, uint16_t raw_h)
{
    if (!filepath) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "Set watchface background from: %s", filepath);

    extern lv_obj_t* watchface_screen_get(void);
    lv_obj_t* wf = watchface_screen_get();
    if (!wf) return ESP_ERR_INVALID_STATE;

    // find first image child
    lv_obj_t* bg_img = NULL;
    uint32_t child_cnt = lv_obj_get_child_cnt(wf);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(wf, i);
        if (lv_obj_check_type(child, &lv_image_class)) { bg_img = child; break; }
    }
    if (!bg_img) return ESP_ERR_NOT_FOUND;

    const char* ext = strrchr(filepath, '.');
    char lv_path[256];

    if (ext && (strcasecmp(ext, ".raw") == 0 || strcasecmp(ext, ".rgb565") == 0) && raw_w > 0 && raw_h > 0) {
        // load raw and set src from memory
        lv_img_dsc_t* dsc = load_raw_rgb565_image(filepath, raw_w, raw_h);
        if (!dsc) return ESP_FAIL;
        lv_img_set_src(bg_img, dsc);
        // keep dsc allocated as long as watchface uses it; the caller can manage persistence
        // user is responsible for freeing previous background buffer if needed
        ESP_LOGI(TAG, "Watchface raw background set (fast)");
        return ESP_OK;
    }

    if (strncmp(filepath, "/sdcard/", 8) == 0) snprintf(lv_path, sizeof(lv_path), "A:%s", filepath + 8);
    else snprintf(lv_path, sizeof(lv_path), "A:%s", filepath);

    lv_img_set_src(bg_img, lv_path);
    lv_obj_invalidate(bg_img);
    lv_refr_now(NULL);
    ESP_LOGI(TAG, "Watchface background set to %s", lv_path);
    return ESP_OK;
}

esp_err_t watchface_set_background_from_file(const char *path)
{
    if (!path) {
        return ESP_FAIL;
    }

    // This loads the image instantly if RAW/JPG, slower if PNG
    media_viewer_show_image_fast(path, 0, 0);

    return ESP_OK;
}
// Legacy API compatibility — keeps older code working
// Legacy API wrapper — matches header type
void media_viewer_show_image(const char *filepath)
{
    if (!filepath) return;

    // Call the new fast image loader
    media_viewer_show_image_fast(filepath, 0, 0);
}


