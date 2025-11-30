#include "storage_file_explorer.h"
#include "ui.h"
#include "ui_fonts.h"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "setting_storage_screen.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "media_player.h"

static lv_obj_t* s_screen;
static void on_delete(lv_event_t* e);
static const char* TAG = "FileExplorer";
static void screen_events(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
        if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
            lv_indev_wait_release(lv_indev_active());
            // Close second-level tile to return to Settings Menu
            ui_dynamic_subtile_close();
            s_screen = NULL;
        }
    }
}

/*#if defined(LV_USE_FILE_EXPLORER) && LV_USE_FILE_EXPLORER
#  if __has_include("lv_file_explorer.h")
#    define LV_HAS_FILE_EXPLORER 1
#  else
#    define LV_HAS_FILE_EXPLORER 0
#  endif
#else
#  define LV_HAS_FILE_EXPLORER 0
#endif*/

#include "lvgl_spiffs_fs.h"
#include "media_player.h"

// Check if file is an image by extension
static bool is_image_file(const char* filename)
{
    if (!filename) return false;
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    ext++; // Skip the dot
    return (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
            strcasecmp(ext, "png") == 0 || strcasecmp(ext, "bmp") == 0 ||
            strcasecmp(ext, "gif") == 0 ||
            strcasecmp(ext, "raw") == 0 ||
            strcasecmp(ext, "rgb565") == 0);
}


// Cleanup handler for file path strings
static void file_path_cleanup_cb(lv_event_t* e)
{
    char* filepath = (char*)lv_event_get_user_data(e);
    if (filepath) {
        free(filepath);
    }
}

// File click handler - view image or set as watchface
static void file_click_cb(lv_event_t* e)
{
    const char* filepath = (const char*)lv_event_get_user_data(e);
    if (!filepath) return;
    
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Single click - view image
        ESP_LOGI(TAG, "Viewing image: %s", filepath);
        media_viewer_show_image(filepath);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        // Long press - set as watchface
        ESP_LOGI(TAG, "Setting watchface: %s", filepath);
        if (watchface_set_background_from_file(filepath) == ESP_OK) {
            ESP_LOGI(TAG, "Watchface background set successfully");
        } else {
            ESP_LOGW(TAG, "Failed to set watchface background");
        }
    }
    ESP_LOGI(TAG, "Button clicked, path = %s", filepath);
media_viewer_show_image(filepath);

}

// Add files from a directory to the list
static void add_files_from_dir(lv_obj_t* list, const char* dir_path, const char* prefix)
{
    DIR* dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open directory: %s", dir_path);
        return;
    }
    
    struct dirent* de;
    struct stat st;
    while ((de = readdir(dir)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", dir_path, de->d_name);
        
        // Only show files we can handle (raw, rgb565, jpg, jpeg, png)
        const char* ext = strrchr(de->d_name, '.');
        if (!ext) continue;
        ext++; // skip dot
        if (!(strcasecmp(ext, "raw") == 0 || strcasecmp(ext, "rgb565") == 0 ||
              strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
              strcasecmp(ext, "png") == 0)) continue;
    
        long sz = 0;
        if (stat(path, &st) == 0) {
            sz = (long)st.st_size;
        }
    
        char line[128];
        snprintf(line, sizeof(line), "%s%s  (%ld KB)", prefix ? prefix : "", de->d_name, sz / 1024);
        ESP_LOGI(TAG, "Found file: %s", de->d_name);
    
        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_IMAGE, line);
        if (btn) {
            // Store full path as user data
            char* path_copy = strdup(path);
            if (path_copy) {
                lv_obj_add_event_cb(btn, file_click_cb, LV_EVENT_CLICKED, path_copy);
                lv_obj_add_event_cb(btn, file_click_cb, LV_EVENT_LONG_PRESSED, path_copy);
                lv_obj_add_event_cb(btn, file_path_cleanup_cb, LV_EVENT_DELETE, path_copy);
            }
        }
    }
    
    closedir(dir);
}

// Create file explorer with SPIFFS and SD card support
static void create_explorer_2(lv_obj_t* parent)
{
    lv_obj_t* list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    
    // Try to mount SD card if not already mounted
    extern sdmmc_card_t *bsp_sdcard;
    bool sdcard_mounted = (bsp_sdcard != NULL);
    
    if (!sdcard_mounted) {
        ESP_LOGI(TAG, "Attempting to mount SD card...");
        esp_err_t ret = bsp_sdcard_mount();
        if (ret == ESP_OK) {
            sdcard_mounted = true;
            ESP_LOGI(TAG, "SD card mounted successfully");
        } else {
            ESP_LOGW(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        }
    }
    
    // Add SD card section
    if (sdcard_mounted) {
        lv_list_add_text(list, "SD Card:");
        add_files_from_dir(list, "/sdcard", "[SD] ");
    } else {
        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_WARNING, "SD Card not detected");
        (void)btn; // Suppress unused warning
    }
    
    // Add SPIFFS section
    lv_list_add_text(list, "Internal Storage:");
    add_files_from_dir(list, "/spiffs", "[INT] ");
}

void storage_file_explorer_screen_create(lv_obj_t* parent)
{
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_bg_opa(&style, LV_OPA_COVER);

    s_screen = lv_obj_create(parent);
    lv_obj_remove_style_all(s_screen);
    lv_obj_add_style(s_screen, &style, 0);
    lv_obj_set_size(s_screen, lv_pct(100), lv_pct(100));
    lv_obj_add_event_cb(s_screen, screen_events, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s_screen, on_delete, LV_EVENT_DELETE, NULL);
    // Allow gestures to bubble for tileview swipes
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_GESTURE_BUBBLE);
    // Mark as "back to Storage" destination for HW back button
    //lv_obj_add_flag(s_screen, LV_OBJ_FLAG_USER_3);

    // Header
    lv_obj_t* hdr = lv_obj_create(s_screen);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_t* title = lv_label_create(hdr);
    lv_obj_set_style_text_font(title, &font_bold_32, 0);
    lv_label_set_text(title, "Files");

    // Content
    lv_obj_t* content = lv_obj_create(s_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, lv_pct(100), lv_pct(80));
    lv_obj_set_style_pad_top(content, 80, 0);
    /*lv_obj_set_style_pad_bottom(content, 10, 0);
    lv_obj_set_style_pad_left(content, 12, 0);
    lv_obj_set_style_pad_right(content, 12, 0);*/
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    //create_explorer_1(content);
    create_explorer_2(content);
}

static void on_delete(lv_event_t* e)
{
    (void)e;
    ESP_LOGI(TAG, "File explorer screen deleted");
    s_screen = NULL;
}

lv_obj_t* storage_file_explorer_screen_get(void)
{
    if (!s_screen) {
        bsp_display_lock(0);
        storage_file_explorer_screen_create(NULL);
        bsp_display_unlock();
    }
    return s_screen;
}
