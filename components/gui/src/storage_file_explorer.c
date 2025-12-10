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
#include "lvgl_spiffs_fs.h"
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
            ui_dynamic_subtile_close();
            s_screen = NULL;
        }
    }
}

// Check if file is an image by extension
bool is_image_file(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (!ext) return false;

    if (strcasecmp(ext, ".jpg") == 0) return true;
    if (strcasecmp(ext, ".jpeg") == 0) return true;
    if (strcasecmp(ext, ".png") == 0) return true;
    if (strcasecmp(ext, ".raw") == 0) return true;
    if (strcasecmp(ext, ".rgb565") == 0) return true;

    return false;
}

// Get appropriate icon and color based on file extension
static const char* get_file_icon(const char* filename, lv_color_t* color)
{
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        *color = lv_color_hex(0x888888);
        return LV_SYMBOL_FILE;
    }
    
    ext++; // Skip the dot
    
    // Audio files - MP3
    if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "wav") == 0 || 
        strcasecmp(ext, "flac") == 0 || strcasecmp(ext, "aac") == 0) {
        *color = lv_color_hex(0x00BCD4); // Cyan
        return LV_SYMBOL_AUDIO;
    }
    // Video files - MP4
    else if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0 || 
             strcasecmp(ext, "mkv") == 0 || strcasecmp(ext, "mov") == 0) {
        *color = lv_color_hex(0xE91E63); // Pink
        return LV_SYMBOL_VIDEO;
    }
    // Image files (including raw formats)
    else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 || 
             strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
             strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "raw") == 0 ||
             strcasecmp(ext, "rgb565") == 0) {
        *color = lv_color_hex(0x4CAF50); // Green
        return LV_SYMBOL_IMAGE;
    }
    // Text/config files
    else if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "log") == 0 || 
             strcasecmp(ext, "cfg") == 0 || strcasecmp(ext, "ini") == 0 ||
             strcasecmp(ext, "json") == 0) {
        *color = lv_color_hex(0xFF9800); // Orange
        return LV_SYMBOL_EDIT;
    }
    // Default
    else {
        *color = lv_color_hex(0x888888); // Gray
        return LV_SYMBOL_FILE;
    }
}

// Format file size nicely
static void format_size(long bytes, char* buf, size_t bufsize)
{
    if (bytes < 1024) {
        snprintf(buf, bufsize, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, bufsize, "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(buf, bufsize, "%.2f MB", bytes / (1024.0 * 1024.0));
    }
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
        ESP_LOGI(TAG, "Viewing file: %s", filepath);
        
        // Check if it's an image file
        if (is_image_file(filepath)) {
            const char* ext = strrchr(filepath, '.');
            if (ext && (strcasecmp(ext, ".raw") == 0 || strcasecmp(ext, ".rgb565") == 0)) {
                // Use your watch's screen dimensions (410x502)
                media_viewer_show_image_fast(filepath, 410, 502);
            } else {
                media_viewer_show_image(filepath);
            }
        } else {
            ESP_LOGI(TAG, "File type not supported for viewing yet");
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        ESP_LOGI(TAG, "Setting watchface: %s", filepath);
        
        if (is_image_file(filepath)) {
            const char* ext = strrchr(filepath, '.');
            if (ext && (strcasecmp(ext, ".raw") == 0 || strcasecmp(ext, ".rgb565") == 0)) {
                watchface_set_background_from_file_fast(filepath, 410, 502);
            } else {
                watchface_set_background_from_file(filepath);
            }
        }
    }
}

// Add files from a directory to the list with improved styling
static void add_files_from_dir(lv_obj_t* list, const char* dir_path, const char* prefix)
{
    DIR* dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open directory: %s", dir_path);
        return;
    }
    
    struct dirent* de;
    struct stat st;
    int file_count = 0;
    
    while ((de = readdir(dir)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", dir_path, de->d_name);
        
        long sz = 0;
        if (stat(path, &st) == 0) {
            sz = (long)st.st_size;
        }
        
        // Get icon and color based on file type
        lv_color_t icon_color;
        const char* icon = get_file_icon(de->d_name, &icon_color);
        
        // Format file size
        char size_str[32];
        format_size(sz, size_str, sizeof(size_str));
        
        // Create display name with prefix
        char display_name[128];
        snprintf(display_name, sizeof(display_name), "%s%s", prefix ? prefix : "", de->d_name);
        
        ESP_LOGI(TAG, "Found file: %s", de->d_name);
        
        // Create list button with improved styling
        lv_obj_t* btn = lv_list_add_button(list, icon, display_name);
        if (!btn) continue;
        
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a2a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_pad_all(btn, 12, 0);
        lv_obj_set_height(btn, 60);
        
        // Color the icon
        lv_obj_t* icon_label = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_color(icon_label, icon_color, 0);
        
        // Style the filename
        lv_obj_t* text_label = lv_obj_get_child(btn, 1);
        lv_obj_set_style_text_color(text_label, lv_color_white(), 0);
        lv_label_set_long_mode(text_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        
        // Add size label
        lv_obj_t* size_label = lv_label_create(btn);
        lv_label_set_text(size_label, size_str);
        lv_obj_set_style_text_color(size_label, lv_color_hex(0x888888), 0);
        lv_obj_align(size_label, LV_ALIGN_RIGHT_MID, -8, 0);
        
        // Store full path as user data
        char* path_copy = strdup(path);
        if (path_copy) {
            lv_obj_add_event_cb(btn, file_click_cb, LV_EVENT_CLICKED, path_copy);
            lv_obj_add_event_cb(btn, file_click_cb, LV_EVENT_LONG_PRESSED, path_copy);
            lv_obj_add_event_cb(btn, file_path_cleanup_cb, LV_EVENT_DELETE, path_copy);
        }
        
        file_count++;
    }
    
    closedir(dir);
}

// Create file explorer with SPIFFS and SD card support
static void create_explorer_2(lv_obj_t* parent)
{
    lv_obj_t* list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(list, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 8, 0);
    lv_obj_set_style_pad_gap(list, 6, 0);
    
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
        lv_obj_t* sd_title = lv_list_add_text(list, "SD Card");
        lv_obj_set_style_text_color(sd_title, lv_color_hex(0x00BCD4), 0);
        lv_obj_set_style_pad_top(sd_title, 8, 0);
        add_files_from_dir(list, "/sdcard", "[SD] ");
    } else {
        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_WARNING, "SD Card not detected");
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFF5252), 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }
    
    // Add SPIFFS section
    lv_obj_t* int_title = lv_list_add_text(list, "Internal Storage");
    lv_obj_set_style_text_color(int_title, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_pad_top(int_title, 16, 0);
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
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Header with better styling
    lv_obj_t* hdr = lv_obj_create(s_screen);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, lv_pct(100), 65);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x0d0d0d), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(hdr, 16, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    
    // Back icon
    lv_obj_t* back_icon = lv_label_create(hdr);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, lv_color_hex(0x00BCD4), 0);
    lv_obj_set_style_pad_right(back_icon, 12, 0);
    
    // Title
    lv_obj_t* title = lv_label_create(hdr);
    lv_obj_set_style_text_font(title, &font_bold_32, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Files");

    // Content area with padding
    lv_obj_t* content = lv_obj_create(s_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_top(content, 70, 0);
    lv_obj_set_style_pad_bottom(content, 8, 0);
    lv_obj_set_style_pad_left(content, 4, 0);
    lv_obj_set_style_pad_right(content, 4, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

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