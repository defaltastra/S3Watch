#include "watchface.h"
#include "sensors.h"
#include "ui_fonts.h"
#include "rtc_lib.h"
#include "settings.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "ui.h"
#include "steps_screen.h"
#include "settings_screen.h"
#include "notifications.h"
#include "media_player.h"
#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include "bsp/esp-bsp.h"

// UI Elements
static lv_obj_t* watchface_screen;
static lv_obj_t* label_hour;
static lv_obj_t* label_minute;
static lv_obj_t* label_second;
static lv_obj_t* label_date;
static lv_obj_t* label_weekday;
static lv_obj_t* label_ampm;
static lv_obj_t* img_battery;
static lv_obj_t* lbl_batt_pct;
static lv_obj_t* lbl_charge_icon;
static lv_obj_t* img_ble;
static lv_obj_t* time_container;
static lv_timer_t* s_timer = NULL;

static watchface_style_t current_style = WATCHFACE_STYLE_CLASSIC;

// Layout types
typedef enum {
    LAYOUT_VERTICAL_SPLIT,    // Hour top, minute bottom (original)
    LAYOUT_HORIZONTAL,        // Hour:Minute side by side
    LAYOUT_CENTERED,          // Everything centered together
    LAYOUT_CORNER,            // Time in corner, date prominent
    LAYOUT_DIGITAL_BAR        // Digital bar at bottom
} layout_type_t;

// Style definitions
typedef struct {
    lv_color_t hour_color;
    lv_color_t minute_color;
    lv_color_t second_color;
    lv_color_t date_color;
    const lv_font_t* hour_font;
    const lv_font_t* minute_font;
    const lv_font_t* second_font;
    const lv_font_t* date_font;
    layout_type_t layout;
    bool show_seconds;
} style_config_t;

static const style_config_t style_configs[WATCHFACE_STYLE_COUNT] = {
    // STYLE_CLASSIC - Original vertical split
    {
        .hour_color = LV_COLOR_MAKE(0xF0, 0xB0, 0x00),
        .minute_color = LV_COLOR_MAKE(0x90, 0xF0, 0x90),
        .second_color = LV_COLOR_MAKE(0x90, 0x90, 0x90),
        .date_color = LV_COLOR_MAKE(0xC0, 0xC0, 0xC0),
        .hour_font = &font_numbers_160,
        .minute_font = &font_numbers_160,
        .second_font = &font_numbers_80,
        .date_font = &font_normal_32,
        .layout = LAYOUT_VERTICAL_SPLIT,
        .show_seconds = true
    },
    // STYLE_MINIMAL - Horizontal clean layout
    {
        .hour_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .minute_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .second_color = LV_COLOR_MAKE(0xAA, 0xAA, 0xAA),
        .date_color = LV_COLOR_MAKE(0xCC, 0xCC, 0xCC),
        .hour_font = &font_numbers_160,
        .minute_font = &font_numbers_160,
        .second_font = &font_normal_32,
        .date_font = &font_normal_26,
        .layout = LAYOUT_HORIZONTAL,
        .show_seconds = false
    },
    // STYLE_DIGITAL - Centered digital display
    {
        .hour_color = LV_COLOR_MAKE(0x00, 0xFF, 0x00),
        .minute_color = LV_COLOR_MAKE(0x00, 0xFF, 0x00),
        .second_color = LV_COLOR_MAKE(0x00, 0xCC, 0x00),
        .date_color = LV_COLOR_MAKE(0x00, 0xAA, 0x00),
        .hour_font = &font_numbers_160,
        .minute_font = &font_numbers_160,
        .second_font = &font_numbers_80,
        .date_font = &font_normal_32,
        .layout = LAYOUT_CENTERED,
        .show_seconds = true
    },
    // STYLE_GRUVBOX - Warm retro colors, corner layout
    {
        .hour_color = LV_COLOR_MAKE(0xFB, 0xF1, 0xC7),  // Gruvbox light cream
        .minute_color = LV_COLOR_MAKE(0xFA, 0xBD, 0x2F), // Gruvbox yellow
        .second_color = LV_COLOR_MAKE(0xB8, 0xBB, 0x26), // Gruvbox green
        .date_color = LV_COLOR_MAKE(0xFE, 0x80, 0x19),   // Gruvbox orange
        .hour_font = &font_numbers_80,
        .minute_font = &font_numbers_80,
        .second_font = &font_normal_32,
        .date_font = &font_bold_32,
        .layout = LAYOUT_CORNER,
        .show_seconds = true
    },
    // STYLE_MONOCHROME - Classic black & white, digital bar
    {
        .hour_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .minute_color = LV_COLOR_MAKE(0xEE, 0xEE, 0xEE),
        .second_color = LV_COLOR_MAKE(0x99, 0x99, 0x99),
        .date_color = LV_COLOR_MAKE(0xBB, 0xBB, 0xBB),
        .hour_font = &font_numbers_160,
        .minute_font = &font_numbers_160,
        .second_font = &font_normal_32,
        .date_font = &font_normal_32,
        .layout = LAYOUT_DIGITAL_BAR,
        .show_seconds = false
    },
    // STYLE_NEON - Bright cyberpunk, centered
    {
        .hour_color = LV_COLOR_MAKE(0xFF, 0x00, 0xFF),
        .minute_color = LV_COLOR_MAKE(0x00, 0xFF, 0xFF),
        .second_color = LV_COLOR_MAKE(0xFF, 0xFF, 0x00),
        .date_color = LV_COLOR_MAKE(0xFF, 0x00, 0x80),
        .hour_font = &font_numbers_160,
        .minute_font = &font_numbers_160,
        .second_font = &font_numbers_80,
        .date_font = &font_bold_32,
        .layout = LAYOUT_CENTERED,
        .show_seconds = true
    },
    // STYLE_PASTEL - Soft colors, horizontal layout
    {
        .hour_color = LV_COLOR_MAKE(0xFF, 0xB3, 0xBA),
        .minute_color = LV_COLOR_MAKE(0xBA, 0xE1, 0xFF),
        .second_color = LV_COLOR_MAKE(0xFF, 0xDF, 0xBA),
        .date_color = LV_COLOR_MAKE(0xE0, 0xBB, 0xE4),
        .hour_font = &font_numbers_160,
        .minute_font = &font_numbers_160,
        .second_font = &font_normal_32,
        .date_font = &font_normal_32,
        .layout = LAYOUT_HORIZONTAL,
        .show_seconds = false
    }
};

// Forward declarations
static void screen_events(lv_event_t* e);
static void update_time_task(lv_timer_t* timer);
static void apply_current_style(void);
static void apply_layout(layout_type_t layout);
esp_err_t watchface_load_saved_background(void);

static void update_time_task(lv_timer_t* timer)
{
    (void)timer;
    
    if (!label_hour || !label_minute) return;
    
    bsp_display_lock(0);
    
    int hour = rtc_get_hour();
    bool is_24h = settings_get_time_format_24h();
    
    // Update AM/PM if needed
    if (!is_24h && label_ampm) {
        lv_label_set_text(label_ampm, (hour < 12) ? "AM" : "PM");
    }
    
    if (!is_24h) {
        int hour_12 = hour % 12;
        hour = (hour_12 == 0) ? 12 : hour_12;
    }
    
    lv_label_set_text_fmt(label_hour, "%02d", hour);
    lv_label_set_text_fmt(label_minute, "%02d", rtc_get_minute());
    
    if (label_second && style_configs[current_style].show_seconds) {
        lv_label_set_text_fmt(label_second, "%02d", rtc_get_second());
    }
    
    if (label_date) {
        lv_label_set_text_fmt(label_date, "%02d/%02d", rtc_get_day(), rtc_get_month());
    }
    
    if (label_weekday) {
        const char *weekday_str = rtc_get_weekday_short_string();
        if (weekday_str) {
            lv_label_set_text(label_weekday, weekday_str);
        }
    }
    
    bsp_display_unlock();
}

esp_err_t watchface_load_saved_background(void)
{
    extern sdmmc_card_t *bsp_sdcard;
    
    if (bsp_sdcard == NULL) {
        ESP_LOGI("Watchface", "SD card not mounted, attempting to mount...");
        esp_err_t mount_err = bsp_sdcard_mount();
        if (mount_err != ESP_OK) {
            ESP_LOGW("Watchface", "Failed to mount SD card: %s", esp_err_to_name(mount_err));
            return ESP_ERR_NOT_FOUND;
        }
    }
    
    char filepath[256];
    esp_err_t err = settings_get_wallpaper(filepath, sizeof(filepath));
    
    if (err != ESP_OK) {
        ESP_LOGI("Watchface", "No saved wallpaper found, using default");
        return err;
    }
    
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGW("Watchface", "Wallpaper file not found: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }
    
    const char* ext = strrchr(filepath, '.');
    if (ext && (strcasecmp(ext, ".raw") == 0 || strcasecmp(ext, ".rgb565") == 0)) {
        uint16_t w = 410, h = 502;
        settings_get_wallpaper_dimensions(&w, &h);
        ESP_LOGI("Watchface", "Restoring RAW wallpaper: %s (%dx%d)", filepath, w, h);
        return watchface_set_background_from_file_fast(filepath, w, h);
    }
    
    ESP_LOGI("Watchface", "Restoring wallpaper: %s", filepath);
    return watchface_set_background_from_file_fast(filepath, 0, 0);
}

static void apply_layout(layout_type_t layout)
{
    if (!label_hour || !label_minute) return;
    
    // Reset positioning
    lv_obj_clear_flag(label_hour, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_minute, LV_OBJ_FLAG_HIDDEN);
    if (label_second) lv_obj_clear_flag(label_second, LV_OBJ_FLAG_HIDDEN);
    if (label_ampm) lv_obj_add_flag(label_ampm, LV_OBJ_FLAG_HIDDEN);
    
    switch (layout) {
        case LAYOUT_VERTICAL_SPLIT:
            // Original: Hour top, minute bottom, seconds center
            lv_obj_set_align(label_hour, LV_ALIGN_CENTER);
            lv_obj_set_y(label_hour, -95);
            lv_obj_set_x(label_hour, 0);
            
            lv_obj_set_align(label_minute, LV_ALIGN_CENTER);
            lv_obj_set_y(label_minute, 105);
            lv_obj_set_x(label_minute, 0);
            
            if (label_second) {
                lv_obj_set_align(label_second, LV_ALIGN_CENTER);
                lv_obj_set_y(label_second, 0);
                lv_obj_set_x(label_second, 0);
            }
            break;
            
        case LAYOUT_HORIZONTAL:
            // Side by side: HH:MM with small seconds
            lv_obj_set_align(label_hour, LV_ALIGN_CENTER);
            lv_obj_set_y(label_hour, -20);
            lv_obj_set_x(label_hour, -100);
            
            lv_obj_set_align(label_minute, LV_ALIGN_CENTER);
            lv_obj_set_y(label_minute, -20);
            lv_obj_set_x(label_minute, 100);
            
            if (label_second) {
                lv_obj_set_align(label_second, LV_ALIGN_CENTER);
                lv_obj_set_y(label_second, 80);
                lv_obj_set_x(label_second, 0);
            }
            break;
            
        case LAYOUT_CENTERED:
            // Everything stacked centered
            lv_obj_set_align(label_hour, LV_ALIGN_CENTER);
            lv_obj_set_y(label_hour, -60);
            lv_obj_set_x(label_hour, 0);
            
            lv_obj_set_align(label_minute, LV_ALIGN_CENTER);
            lv_obj_set_y(label_minute, 60);
            lv_obj_set_x(label_minute, 0);
            
            if (label_second) {
                lv_obj_set_align(label_second, LV_ALIGN_CENTER);
                lv_obj_set_y(label_second, 150);
                lv_obj_set_x(label_second, 0);
            }
            break;
            
        case LAYOUT_CORNER:
            // Time in top-left corner, date prominent
            lv_obj_set_align(label_hour, LV_ALIGN_TOP_LEFT);
            lv_obj_set_y(label_hour, 60);
            lv_obj_set_x(label_hour, 30);
            
            lv_obj_set_align(label_minute, LV_ALIGN_TOP_LEFT);
            lv_obj_set_y(label_minute, 150);
            lv_obj_set_x(label_minute, 30);
            
            if (label_second) {
                lv_obj_set_align(label_second, LV_ALIGN_TOP_LEFT);
                lv_obj_set_y(label_second, 230);
                lv_obj_set_x(label_second, 35);
            }
            
            // Show AM/PM for 12h format
            if (!settings_get_time_format_24h() && label_ampm) {
                lv_obj_clear_flag(label_ampm, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_align(label_ampm, LV_ALIGN_TOP_LEFT);
                lv_obj_set_y(label_ampm, 265);
                lv_obj_set_x(label_ampm, 40);
            }
            break;
            
        case LAYOUT_DIGITAL_BAR:
            // Digital style at bottom
            lv_obj_set_align(label_hour, LV_ALIGN_BOTTOM_MID);
            lv_obj_set_y(label_hour, -100);
            lv_obj_set_x(label_hour, -100);
            
            lv_obj_set_align(label_minute, LV_ALIGN_BOTTOM_MID);
            lv_obj_set_y(label_minute, -100);
            lv_obj_set_x(label_minute, 100);
            
            if (label_second) {
                lv_obj_add_flag(label_second, LV_OBJ_FLAG_HIDDEN);
            }
            break;
    }
}

static void apply_current_style(void)
{
    if (!label_hour || !label_minute) return;
    
    const style_config_t* cfg = &style_configs[current_style];
    
    // Apply fonts and colors
    lv_obj_set_style_text_font(label_hour, cfg->hour_font, 0);
    lv_obj_set_style_text_color(label_hour, cfg->hour_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_set_style_text_font(label_minute, cfg->minute_font, 0);
    lv_obj_set_style_text_color(label_minute, cfg->minute_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    if (label_second) {
        lv_obj_set_style_text_font(label_second, cfg->second_font, 0);
        lv_obj_set_style_text_color(label_second, cfg->second_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    
    if (label_date) {
        lv_obj_set_style_text_font(label_date, cfg->date_font, 0);
        lv_obj_set_style_text_color(label_date, cfg->date_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    
    if (label_weekday) {
        lv_obj_set_style_text_color(label_weekday, cfg->date_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    
    // Apply layout
    apply_layout(cfg->layout);
}

void watchface_create(lv_obj_t* parent)
{
    watchface_screen = lv_obj_create(parent);
    lv_obj_remove_style_all(watchface_screen);
    lv_obj_set_size(watchface_screen, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(watchface_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Background image
    LV_IMAGE_DECLARE(background_wf);
    lv_obj_t* image = lv_image_create(watchface_screen);
    lv_image_set_src(image, &background_wf);
    lv_obj_set_align(image, LV_ALIGN_CENTER);
    
    // Create time labels
    label_hour = lv_label_create(watchface_screen);
    lv_obj_set_align(label_hour, LV_ALIGN_CENTER);
    lv_label_set_text(label_hour, "--");
    lv_obj_set_style_text_letter_space(label_hour, 1, 0);
    
    label_minute = lv_label_create(watchface_screen);
    lv_obj_set_align(label_minute, LV_ALIGN_CENTER);
    lv_label_set_text(label_minute, "--");
    lv_obj_set_style_text_letter_space(label_minute, 1, 0);
    
    label_second = lv_label_create(watchface_screen);
    lv_obj_set_align(label_second, LV_ALIGN_CENTER);
    lv_label_set_text(label_second, "--");
    lv_obj_set_style_text_letter_space(label_second, 1, 0);
    
    // AM/PM label (hidden by default)
    label_ampm = lv_label_create(watchface_screen);
    lv_obj_set_style_text_font(label_ampm, &font_normal_26, 0);
    lv_label_set_text(label_ampm, "AM");
    lv_obj_add_flag(label_ampm, LV_OBJ_FLAG_HIDDEN);
    
    // Date container
    lv_obj_t* date_cont = lv_obj_create(watchface_screen);
    lv_obj_remove_style_all(date_cont);
    lv_obj_set_size(date_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_x(date_cont, -20);
    lv_obj_set_align(date_cont, LV_ALIGN_RIGHT_MID);
    lv_obj_set_flex_flow(date_cont, LV_FLEX_FLOW_COLUMN);
    
    label_date = lv_label_create(date_cont);
    lv_label_set_text(label_date, "--/--");
    lv_obj_set_style_text_letter_space(label_date, 1, 0);
    
    label_weekday = lv_label_create(date_cont);
    lv_label_set_text(label_weekday, "---");
    lv_obj_set_style_text_letter_space(label_weekday, 3, 0);
    lv_obj_set_style_text_font(label_weekday, &font_bold_32, 0);
    
    // Battery indicator
    extern const lv_image_dsc_t image_battery_icon;
    img_battery = lv_image_create(watchface_screen);
    lv_image_set_src(img_battery, &image_battery_icon);
    lv_obj_set_align(img_battery, LV_ALIGN_TOP_MID);
    lv_obj_set_x(img_battery, -100);
    lv_obj_set_style_img_recolor_opa(img_battery, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img_battery, lv_color_hex(0x909090), 0);
    
    lbl_batt_pct = lv_label_create(watchface_screen);
    lv_obj_align_to(lbl_batt_pct, img_battery, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_set_style_text_color(lbl_batt_pct, lv_color_hex(0xC0C0C0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(lbl_batt_pct, "--%");
    lv_obj_set_style_text_font(lbl_batt_pct, &font_normal_26, 0);
    
    lbl_charge_icon = lv_label_create(img_battery);
    lv_label_set_text(lbl_charge_icon, LV_SYMBOL_CHARGE);
    lv_obj_center(lbl_charge_icon);
    lv_obj_set_style_text_font(lbl_charge_icon, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(lbl_charge_icon, lv_color_white(), 0);
    lv_obj_add_flag(lbl_charge_icon, LV_OBJ_FLAG_HIDDEN);
    
    // Bluetooth indicator
    extern const lv_image_dsc_t image_bluetooth_icon;
    img_ble = lv_image_create(watchface_screen);
    lv_image_set_src(img_ble, &image_bluetooth_icon);
    lv_obj_set_align(img_ble, LV_ALIGN_TOP_MID);
    lv_obj_set_x(img_ble, 100);
    lv_obj_set_style_img_recolor_opa(img_ble, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img_ble, lv_color_hex(0x606060), 0);
    
    // Add gesture event handler
    lv_obj_add_event_cb(watchface_screen, screen_events, LV_EVENT_GESTURE, NULL);
    
    // Apply initial style (load from settings)
    current_style = (watchface_style_t)settings_get_watchface_style();
    if (current_style >= WATCHFACE_STYLE_COUNT) {
        current_style = WATCHFACE_STYLE_CLASSIC;
    }
    apply_current_style();
    
    // Create update timer
    s_timer = lv_timer_create(update_time_task, 1000, NULL);
    lv_timer_ready(s_timer);
    
    // Restore wallpaper
    watchface_load_saved_background();
}

static void screen_events(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        ESP_LOGI("WFACE", "Gesture direction: %d", dir);
        
        lv_indev_wait_release(lv_indev_active());
        
        switch (dir) {
            case LV_DIR_RIGHT:
                load_screen(watchface_screen, steps_screen_get(), LV_SCR_LOAD_ANIM_MOVE_RIGHT);
                break;
            case LV_DIR_TOP:
                load_screen(watchface_screen, control_screen_get(), LV_SCR_LOAD_ANIM_MOVE_TOP);
                break;
            case LV_DIR_BOTTOM:
                load_screen(watchface_screen, notifications_screen_get(), LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
                break;
            default:
                break;
        }
    }
}

lv_obj_t* watchface_screen_get(void)
{
    if (watchface_screen == NULL) {
        watchface_create(NULL);
    }
    return watchface_screen;
}

void watchface_set_style(watchface_style_t style)
{
    if (style >= WATCHFACE_STYLE_COUNT) return;
    
    current_style = style;
    apply_current_style();
}

watchface_style_t watchface_get_style(void)
{
    return current_style;
}

void watchface_set_power_state(bool vbus_in, bool charging, int battery_percent)
{
    if (!img_battery) return;
    
    lv_color_t col;
    if (charging) {
        col = lv_color_hex(0x00FF00);
    } else if (vbus_in) {
        col = lv_color_hex(0x00BFFF);
    } else {
        col = lv_color_hex(0x909090);
    }
    
    lv_obj_set_style_img_recolor(img_battery, col, 0);
    
    if (lbl_batt_pct && battery_percent >= 0 && battery_percent <= 100) {
        static char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d%%", battery_percent);
        lv_label_set_text(lbl_batt_pct, buf);
    }
    
    if (lbl_charge_icon) {
        if (vbus_in || charging) {
            lv_obj_clear_flag(lbl_charge_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lbl_charge_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void watchface_set_ble_connected(bool connected)
{
    if (!img_ble) return;
    
    lv_color_t col = connected ? lv_color_hex(0x3B82F6) : lv_color_hex(0x606060);
    lv_obj_set_style_img_recolor(img_ble, col, 0);
}