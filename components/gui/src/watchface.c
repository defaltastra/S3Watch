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
#include <math.h>
#include "bsp/esp-bsp.h"

// UI Elements - Digital
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
static lv_obj_t* date_container;

// UI Elements - Analog
static lv_obj_t* analog_container;
static lv_obj_t* clock_face;
static lv_obj_t* hour_hand;
static lv_obj_t* minute_hand;
static lv_obj_t* second_hand;
static lv_obj_t* center_dot;
static lv_obj_t* analog_date_label;

static lv_timer_t* s_timer = NULL;

static watchface_style_t current_style = WATCHFACE_STYLE_CLASSIC;

#define PI 3.14159265359
#define CLOCK_RADIUS_LARGE 180
#define CLOCK_RADIUS_MEDIUM 140
#define CLOCK_RADIUS_SMALL 100

// Layout types
typedef enum {
    LAYOUT_VERTICAL_SPLIT,
    LAYOUT_HORIZONTAL,
    LAYOUT_CENTERED,
    LAYOUT_CORNER,
    LAYOUT_DIGITAL_BAR,
    LAYOUT_ANALOG_CLASSIC,
    LAYOUT_ANALOG_MODERN,
    LAYOUT_ANALOG_MINIMAL,
    LAYOUT_ANALOG_SKELETON
} layout_type_t;

typedef struct {
    int radius;
    int hour_hand_len;
    int minute_hand_len;
    int second_hand_len;
    int hour_hand_width;
    int minute_hand_width;
    int second_hand_width;
    bool show_numbers;
    bool show_markers;
    bool show_second_hand;
    lv_color_t face_color;
    lv_color_t border_color;
    lv_color_t hand_color;
    lv_color_t second_hand_color;
} analog_config_t;

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
    analog_config_t analog_cfg;
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
        .hour_color = LV_COLOR_MAKE(0xFB, 0xF1, 0xC7),
        .minute_color = LV_COLOR_MAKE(0xFA, 0xBD, 0x2F),
        .second_color = LV_COLOR_MAKE(0xB8, 0xBB, 0x26),
        .date_color = LV_COLOR_MAKE(0xFE, 0x80, 0x19),
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
    },
    // STYLE_ANALOG_CLASSIC - Traditional large analog clock
    {
        .hour_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .minute_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .second_color = LV_COLOR_MAKE(0xFF, 0x00, 0x00),
        .date_color = LV_COLOR_MAKE(0xC0, 0xC0, 0xC0),
        .hour_font = &font_normal_26,
        .minute_font = &font_normal_26,
        .second_font = &font_normal_26,
        .date_font = &font_bold_32,
        .layout = LAYOUT_ANALOG_CLASSIC,
        .show_seconds = true,
        .analog_cfg = {
            .radius = CLOCK_RADIUS_LARGE,
            .hour_hand_len = 100,
            .minute_hand_len = 140,
            .second_hand_len = 150,
            .hour_hand_width = 8,
            .minute_hand_width = 6,
            .second_hand_width = 2,
            .show_numbers = true,
            .show_markers = true,
            .show_second_hand = true,
            .face_color = LV_COLOR_MAKE(0x00, 0x00, 0x00),
            .border_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
            .hand_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
            .second_hand_color = LV_COLOR_MAKE(0xFF, 0x00, 0x00)
        }
    },
    // STYLE_ANALOG_MODERN - Medium size, colorful, clean
    {
        .hour_color = LV_COLOR_MAKE(0x00, 0xBF, 0xFF),
        .minute_color = LV_COLOR_MAKE(0x00, 0xFF, 0xBF),
        .second_color = LV_COLOR_MAKE(0xFF, 0x00, 0xFF),
        .date_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .hour_font = &font_normal_26,
        .minute_font = &font_normal_26,
        .second_font = &font_normal_26,
        .date_font = &font_bold_32,
        .layout = LAYOUT_ANALOG_MODERN,
        .show_seconds = true,
        .analog_cfg = {
            .radius = CLOCK_RADIUS_MEDIUM,
            .hour_hand_len = 75,
            .minute_hand_len = 105,
            .second_hand_len = 115,
            .hour_hand_width = 6,
            .minute_hand_width = 4,
            .second_hand_width = 2,
            .show_numbers = false,
            .show_markers = true,
            .show_second_hand = true,
            .face_color = LV_COLOR_MAKE(0x20, 0x20, 0x20),
            .border_color = LV_COLOR_MAKE(0x00, 0xBF, 0xFF),
            .hand_color = LV_COLOR_MAKE(0x00, 0xFF, 0xBF),
            .second_hand_color = LV_COLOR_MAKE(0xFF, 0x00, 0xFF)
        }
    },
    // STYLE_ANALOG_MINIMAL - Small, minimalist, no seconds
    {
        .hour_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .minute_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .second_color = LV_COLOR_MAKE(0x80, 0x80, 0x80),
        .date_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .hour_font = &font_normal_26,
        .minute_font = &font_normal_26,
        .second_font = &font_normal_26,
        .date_font = &font_numbers_80,
        .layout = LAYOUT_ANALOG_MINIMAL,
        .show_seconds = false,
        .analog_cfg = {
            .radius = CLOCK_RADIUS_SMALL,
            .hour_hand_len = 50,
            .minute_hand_len = 75,
            .second_hand_len = 0,
            .hour_hand_width = 5,
            .minute_hand_width = 3,
            .second_hand_width = 0,
            .show_numbers = false,
            .show_markers = false,
            .show_second_hand = false,
            .face_color = LV_COLOR_MAKE(0x10, 0x10, 0x10),
            .border_color = LV_COLOR_MAKE(0x60, 0x60, 0x60),
            .hand_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
            .second_hand_color = LV_COLOR_MAKE(0x80, 0x80, 0x80)
        }
    },
    // STYLE_ANALOG_SKELETON - Transparent, skeleton style
    {
        .hour_color = LV_COLOR_MAKE(0xFA, 0xBD, 0x2F),
        .minute_color = LV_COLOR_MAKE(0xFE, 0x80, 0x19),
        .second_color = LV_COLOR_MAKE(0xFB, 0x49, 0x34),
        .date_color = LV_COLOR_MAKE(0xFA, 0xBD, 0x2F),
        .hour_font = &font_bold_32,
        .minute_font = &font_bold_32,
        .second_font = &font_bold_32,
        .date_font = &font_bold_32,
        .layout = LAYOUT_ANALOG_SKELETON,
        .show_seconds = true,
        .analog_cfg = {
            .radius = CLOCK_RADIUS_LARGE,
            .hour_hand_len = 90,
            .minute_hand_len = 130,
            .second_hand_len = 145,
            .hour_hand_width = 10,
            .minute_hand_width = 6,
            .second_hand_width = 3,
            .show_numbers = true,
            .show_markers = false,
            .show_second_hand = true,
            .face_color = LV_COLOR_MAKE(0x00, 0x00, 0x00),
            .border_color = LV_COLOR_MAKE(0xFA, 0xBD, 0x2F),
            .hand_color = LV_COLOR_MAKE(0xFE, 0x80, 0x19),
            .second_hand_color = LV_COLOR_MAKE(0xFB, 0x49, 0x34)
        }
    }
};

// Forward declarations
static void screen_events(lv_event_t* e);
static void update_time_task(lv_timer_t* timer);
static void apply_current_style(void);
static void apply_layout(layout_type_t layout);
static void create_analog_clock(const analog_config_t* cfg);
static void update_analog_hands(void);
static void hide_digital_elements(void);
static void show_digital_elements(void);
static void destroy_analog_clock(void);
esp_err_t watchface_load_saved_background(void);

static void update_time_task(lv_timer_t* timer)
{
    (void)timer;
    
    bsp_display_lock(0);
    
    const style_config_t* cfg = &style_configs[current_style];
    
    if (cfg->layout >= LAYOUT_ANALOG_CLASSIC) {
        update_analog_hands();
        
        // Update analog date label
        if (analog_date_label) {
            lv_label_set_text_fmt(analog_date_label, "%s %02d", 
                                  rtc_get_weekday_short_string(), rtc_get_day());
        }
    } else {
        if (!label_hour || !label_minute) {
            bsp_display_unlock();
            return;
        }
        
        int hour = rtc_get_hour();
        bool is_24h = settings_get_time_format_24h();
        
        if (!is_24h && label_ampm) {
            lv_label_set_text(label_ampm, (hour < 12) ? "AM" : "PM");
        }
        
        if (!is_24h) {
            int hour_12 = hour % 12;
            hour = (hour_12 == 0) ? 12 : hour_12;
        }
        
        lv_label_set_text_fmt(label_hour, "%02d", hour);
        lv_label_set_text_fmt(label_minute, "%02d", rtc_get_minute());
        
        if (label_second && cfg->show_seconds) {
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
    }
    
    bsp_display_unlock();
}

static void hide_digital_elements(void)
{
    if (label_hour) lv_obj_add_flag(label_hour, LV_OBJ_FLAG_HIDDEN);
    if (label_minute) lv_obj_add_flag(label_minute, LV_OBJ_FLAG_HIDDEN);
    if (label_second) lv_obj_add_flag(label_second, LV_OBJ_FLAG_HIDDEN);
    if (label_ampm) lv_obj_add_flag(label_ampm, LV_OBJ_FLAG_HIDDEN);
    if (date_container) lv_obj_add_flag(date_container, LV_OBJ_FLAG_HIDDEN);
}

static void show_digital_elements(void)
{
    if (label_hour) lv_obj_clear_flag(label_hour, LV_OBJ_FLAG_HIDDEN);
    if (label_minute) lv_obj_clear_flag(label_minute, LV_OBJ_FLAG_HIDDEN);
    if (label_second) lv_obj_clear_flag(label_second, LV_OBJ_FLAG_HIDDEN);
    if (date_container) lv_obj_clear_flag(date_container, LV_OBJ_FLAG_HIDDEN);
}

static void destroy_analog_clock(void)
{
    if (analog_container) {
        lv_obj_del(analog_container);
        analog_container = NULL;
        clock_face = NULL;
        hour_hand = NULL;
        minute_hand = NULL;
        second_hand = NULL;
        center_dot = NULL;
        analog_date_label = NULL;
    }
}

static void create_analog_clock(const analog_config_t* cfg)
{
    destroy_analog_clock();
    
    const style_config_t* style_cfg = &style_configs[current_style];
    
    analog_container = lv_obj_create(watchface_screen);
    lv_obj_remove_style_all(analog_container);
    lv_obj_set_size(analog_container, cfg->radius * 2 + 60, cfg->radius * 2 + 60);
    lv_obj_center(analog_container);
    
    // Clock face circle
    clock_face = lv_obj_create(analog_container);
    lv_obj_remove_style_all(clock_face);
    lv_obj_set_size(clock_face, cfg->radius * 2, cfg->radius * 2);
    lv_obj_center(clock_face);
    lv_obj_set_style_radius(clock_face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(clock_face, 4, 0);
    lv_obj_set_style_border_color(clock_face, cfg->border_color, 0);
    lv_obj_set_style_bg_opa(clock_face, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(clock_face, cfg->face_color, 0);
    
    // Hour numbers or markers
    if (cfg->show_numbers) {
        for (int i = 1; i <= 12; i++) {
            float angle = (i * 30 - 90) * PI / 180.0;
            int x = cfg->radius + (int)(cos(angle) * (cfg->radius - 30));
            int y = cfg->radius + (int)(sin(angle) * (cfg->radius - 30));
            
            lv_obj_t* num = lv_label_create(clock_face);
            lv_label_set_text_fmt(num, "%d", i);
            lv_obj_set_style_text_font(num, &font_normal_26, 0);
            lv_obj_set_style_text_color(num, cfg->border_color, 0);
            lv_obj_set_pos(num, x - 10, y - 10);
        }
    } else if (cfg->show_markers) {
        for (int i = 0; i < 60; i++) {
            float angle = (i * 6 - 90) * PI / 180.0;
            int x = cfg->radius + (int)(cos(angle) * (cfg->radius - 15));
            int y = cfg->radius + (int)(sin(angle) * (cfg->radius - 15));
            
            lv_obj_t* marker = lv_obj_create(clock_face);
            lv_obj_remove_style_all(marker);
            
            if (i % 5 == 0) {
                lv_obj_set_size(marker, 3, 10);
                lv_obj_set_style_bg_color(marker, cfg->border_color, 0);
            } else {
                lv_obj_set_size(marker, 2, 6);
                lv_obj_set_style_bg_color(marker, lv_color_hex(0x606060), 0);
            }
            lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
            lv_obj_set_pos(marker, x - 1, y - 3);
        }
    }
    
    // Hour hand
    hour_hand = lv_obj_create(clock_face);
    lv_obj_remove_style_all(hour_hand);
    lv_obj_set_size(hour_hand, cfg->hour_hand_width, cfg->hour_hand_len);
    lv_obj_set_style_bg_color(hour_hand, cfg->hand_color, 0);
    lv_obj_set_style_bg_opa(hour_hand, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hour_hand, cfg->hour_hand_width / 2, 0);
    lv_obj_set_pos(hour_hand, cfg->radius - cfg->hour_hand_width / 2, 
                   cfg->radius - cfg->hour_hand_len);
    lv_obj_set_style_transform_pivot_x(hour_hand, cfg->hour_hand_width / 2, 0);
    lv_obj_set_style_transform_pivot_y(hour_hand, cfg->hour_hand_len, 0);
    
    // Minute hand
    minute_hand = lv_obj_create(clock_face);
    lv_obj_remove_style_all(minute_hand);
    lv_obj_set_size(minute_hand, cfg->minute_hand_width, cfg->minute_hand_len);
    lv_obj_set_style_bg_color(minute_hand, cfg->hand_color, 0);
    lv_obj_set_style_bg_opa(minute_hand, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(minute_hand, cfg->minute_hand_width / 2, 0);
    lv_obj_set_pos(minute_hand, cfg->radius - cfg->minute_hand_width / 2, 
                   cfg->radius - cfg->minute_hand_len);
    lv_obj_set_style_transform_pivot_x(minute_hand, cfg->minute_hand_width / 2, 0);
    lv_obj_set_style_transform_pivot_y(minute_hand, cfg->minute_hand_len, 0);
    
    // Second hand
    if (cfg->show_second_hand) {
        second_hand = lv_obj_create(clock_face);
        lv_obj_remove_style_all(second_hand);
        lv_obj_set_size(second_hand, cfg->second_hand_width, cfg->second_hand_len);
        lv_obj_set_style_bg_color(second_hand, cfg->second_hand_color, 0);
        lv_obj_set_style_bg_opa(second_hand, LV_OPA_COVER, 0);
        lv_obj_set_pos(second_hand, cfg->radius - cfg->second_hand_width / 2, 
                       cfg->radius - cfg->second_hand_len);
        lv_obj_set_style_transform_pivot_x(second_hand, cfg->second_hand_width / 2, 0);
        lv_obj_set_style_transform_pivot_y(second_hand, cfg->second_hand_len, 0);
    }
    
    // Center dot
    center_dot = lv_obj_create(clock_face);
    lv_obj_remove_style_all(center_dot);
    lv_obj_set_size(center_dot, 14, 14);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center_dot, cfg->second_hand_color, 0);
    lv_obj_set_style_bg_opa(center_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(center_dot, 2, 0);
    lv_obj_set_style_border_color(center_dot, cfg->hand_color, 0);
    lv_obj_set_pos(center_dot, cfg->radius - 7, cfg->radius - 7);
    
    // Date label position based on layout
    analog_date_label = lv_label_create(watchface_screen);
    lv_obj_set_style_text_font(analog_date_label, style_cfg->date_font, 0);
    lv_obj_set_style_text_color(analog_date_label, style_cfg->date_color, 0);
    lv_label_set_text(analog_date_label, "MON 15");
    
    // Position date based on clock size
    if (current_style == WATCHFACE_STYLE_ANALOG_MINIMAL) {
        // Large date below small clock
        lv_obj_align(analog_date_label, LV_ALIGN_BOTTOM_MID, 0, -50);
    } else if (current_style == WATCHFACE_STYLE_ANALOG_MODERN) {
        // Date at bottom center
        lv_obj_align(analog_date_label, LV_ALIGN_BOTTOM_MID, 0, -30);
    } else {
        // Date inside clock at 6 o'clock position
        lv_obj_align(analog_date_label, LV_ALIGN_CENTER, 0, cfg->radius - 50);
    }
    
    update_analog_hands();
}

static void update_analog_hands(void)
{
    if (!hour_hand || !minute_hand) return;
    
    int hour = rtc_get_hour();
    int minute = rtc_get_minute();
    int second = rtc_get_second();
    
    // Calculate angles (0 degrees = 12 o'clock, multiply by 10 for LVGL tenths of degrees)
    float hour_angle = ((hour % 12) * 30 + minute * 0.5) * 10;
    float minute_angle = (minute * 6) * 10;
    float second_angle = (second * 6) * 10;
    
    lv_obj_set_style_transform_angle(hour_hand, (int)hour_angle, 0);
    lv_obj_set_style_transform_angle(minute_hand, (int)minute_angle, 0);
    
    if (second_hand) {
        lv_obj_set_style_transform_angle(second_hand, (int)second_angle, 0);
    }
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
    
    destroy_analog_clock();
    show_digital_elements();
    if (label_ampm) lv_obj_add_flag(label_ampm, LV_OBJ_FLAG_HIDDEN);
    
    const style_config_t* cfg = &style_configs[current_style];
    
    if (layout >= LAYOUT_ANALOG_CLASSIC) {
        hide_digital_elements();
        create_analog_clock(&cfg->analog_cfg);
        return;
    }
    
    switch (layout) {
        case LAYOUT_VERTICAL_SPLIT:
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
            
            if (!settings_get_time_format_24h() && label_ampm) {
                lv_obj_clear_flag(label_ampm, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_align(label_ampm, LV_ALIGN_TOP_LEFT);
                lv_obj_set_y(label_ampm, 265);
                lv_obj_set_x(label_ampm, 40);
            }
            break;
            
        case LAYOUT_DIGITAL_BAR:
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
            
        default:
            break;
    }
}

static void apply_current_style(void)
{
    if (!label_hour || !label_minute) return;
    
    const style_config_t* cfg = &style_configs[current_style];
    
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
    
    apply_layout(cfg->layout);
}

void watchface_create(lv_obj_t* parent)
{
    watchface_screen = lv_obj_create(parent);
    lv_obj_remove_style_all(watchface_screen);
    lv_obj_set_size(watchface_screen, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(watchface_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    LV_IMAGE_DECLARE(background_wf);
    lv_obj_t* image = lv_image_create(watchface_screen);
    lv_image_set_src(image, &background_wf);
    lv_obj_set_align(image, LV_ALIGN_CENTER);
    
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
    
    label_ampm = lv_label_create(watchface_screen);
    lv_obj_set_style_text_font(label_ampm, &font_normal_26, 0);
    lv_label_set_text(label_ampm, "AM");
    lv_obj_add_flag(label_ampm, LV_OBJ_FLAG_HIDDEN);
    
    date_container = lv_obj_create(watchface_screen);
    lv_obj_remove_style_all(date_container);
    lv_obj_set_size(date_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_x(date_container, -20);
    lv_obj_set_align(date_container, LV_ALIGN_RIGHT_MID);
    lv_obj_set_flex_flow(date_container, LV_FLEX_FLOW_COLUMN);
    
    label_date = lv_label_create(date_container);
    lv_label_set_text(label_date, "--/--");
    lv_obj_set_style_text_letter_space(label_date, 1, 0);
    
    label_weekday = lv_label_create(date_container);
    lv_label_set_text(label_weekday, "---");
    lv_obj_set_style_text_letter_space(label_weekday, 3, 0);
    lv_obj_set_style_text_font(label_weekday, &font_bold_32, 0);
    
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
    
    extern const lv_image_dsc_t image_bluetooth_icon;
    img_ble = lv_image_create(watchface_screen);
    lv_image_set_src(img_ble, &image_bluetooth_icon);
    lv_obj_set_align(img_ble, LV_ALIGN_TOP_MID);
    lv_obj_set_x(img_ble, 100);
    lv_obj_set_style_img_recolor_opa(img_ble, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img_ble, lv_color_hex(0x606060), 0);
    
    lv_obj_add_event_cb(watchface_screen, screen_events, LV_EVENT_GESTURE, NULL);
    
    current_style = (watchface_style_t)settings_get_watchface_style();
    if (current_style >= WATCHFACE_STYLE_COUNT) {
        current_style = WATCHFACE_STYLE_CLASSIC;
    }
    apply_current_style();
    
    s_timer = lv_timer_create(update_time_task, 1000, NULL);
    lv_timer_ready(s_timer);
    
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