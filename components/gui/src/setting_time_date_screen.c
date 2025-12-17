#include "setting_time_date_screen.h"
#include "ui.h"
#include "ui_fonts.h"
#include "settings.h"
#include "rtc_lib.h"
#include "esp_log.h"
#include "settings_menu_screen.h"
#include "watchface.h"
#include <time.h>
#include <string.h>

static lv_obj_t* stime_date_screen;
static lv_obj_t* time_display;
static lv_obj_t* date_display;
static lv_obj_t* style_dropdown;

static const char* TAG = "TimeDate";

// Style names matching watchface styles
static const char* style_names[] = {
    "Classic (Vertical)",
    "Minimal (Horizontal)",
    "Digital (Centered)",
    "Gruvbox (Corner)",
    "Monochrome (Bar)",
    "Neon (Centered)",
    "Pastel (Horizontal)",
    "Analog Classic",
    "Analog Modern",
    "Analog Minimal",
    "Analog Skeleton"
};

static void on_delete(lv_event_t* e);
static void screen_events(lv_event_t* e);

static void screen_events(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
        if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
            lv_indev_wait_release(lv_indev_active());
            ui_dynamic_subtile_close();
            stime_date_screen = NULL;
        }
    }
}

static void update_displays(void)
{
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    
    bool locked = bsp_display_lock(0);
    
    if (time_display) {
        bool is_24h = settings_get_time_format_24h();
        int hour = time.tm_hour;
        
        if (is_24h) {
            lv_label_set_text_fmt(time_display, "%02d:%02d", hour, time.tm_min);
        } else {
            int hour_12 = hour % 12;
            if (hour_12 == 0) hour_12 = 12;
            const char* am_pm = (hour < 12) ? "AM" : "PM";
            lv_label_set_text_fmt(time_display, "%02d:%02d %s", hour_12, time.tm_min, am_pm);
        }
    }
    
    if (date_display) {
        const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        
        lv_label_set_text_fmt(date_display, "%s, %s %02d, %04d", 
                              weekdays[time.tm_wday],
                              months[time.tm_mon],
                              time.tm_mday, 
                              time.tm_year + 1900);
    }
    
    if (locked) bsp_display_unlock();
}

static void time_btn_clicked(lv_event_t* e)
{
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    
    int btn_id = (int)(uintptr_t)lv_event_get_user_data(e);
    
    switch(btn_id) {
        case 0: // Hour -
            time.tm_hour = (time.tm_hour > 0) ? time.tm_hour - 1 : 23;
            break;
        case 1: // Hour +
            time.tm_hour = (time.tm_hour < 23) ? time.tm_hour + 1 : 0;
            break;
        case 2: // Min -
            time.tm_min = (time.tm_min > 0) ? time.tm_min - 1 : 59;
            break;
        case 3: // Min +
            time.tm_min = (time.tm_min < 59) ? time.tm_min + 1 : 0;
            break;
    }
    
    // Recalculate day of week
    mktime(&time);
    
    rtc_set_time(&time);
    update_displays();
}

static void date_btn_clicked(lv_event_t* e)
{
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    
    int btn_id = (int)(uintptr_t)lv_event_get_user_data(e);
    
    switch(btn_id) {
        case 0: // Year -
            if (time.tm_year > 0) time.tm_year--;
            break;
        case 1: // Year +
            if (time.tm_year < 200) time.tm_year++;
            break;
        case 2: // Month -
            if (time.tm_mon > 0) {
                time.tm_mon--;
            } else {
                time.tm_mon = 11;
                if (time.tm_year > 0) time.tm_year--;
            }
            break;
        case 3: // Month +
            if (time.tm_mon < 11) {
                time.tm_mon++;
            } else {
                time.tm_mon = 0;
                if (time.tm_year < 200) time.tm_year++;
            }
            break;
        case 4: // Day -
            if (time.tm_mday > 1) {
                time.tm_mday--;
            } else {
                // Go to previous month's last day
                if (time.tm_mon > 0) {
                    time.tm_mon--;
                } else {
                    time.tm_mon = 11;
                    if (time.tm_year > 0) time.tm_year--;
                }
                // Set to last day of month (mktime will normalize)
                time.tm_mday = 31;
            }
            break;
        case 5: // Day +
            // Let mktime handle month overflow
            time.tm_mday++;
            break;
    }
    
    // Normalize and calculate day of week
    mktime(&time);
    
    rtc_set_time(&time);
    update_displays();
}

static void format_toggle_clicked(lv_event_t* e)
{
    bool current_24h = settings_get_time_format_24h();
    settings_set_time_format_24h(!current_24h);
    
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    
    if (lbl) {
        lv_label_set_text(lbl, current_24h ? "12H" : "24H");
    }
    
    update_displays();
    ESP_LOGI(TAG, "Time format changed to %s", current_24h ? "12H" : "24H");
}

static void style_changed(lv_event_t* e)
{
    lv_obj_t* dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    
    if (selected < WATCHFACE_STYLE_COUNT) {
        watchface_set_style((watchface_style_t)selected);
        settings_set_watchface_style(selected);
        ESP_LOGI(TAG, "Watchface style changed to: %s", style_names[selected]);
    }
}

void setting_time_date_screen_create(lv_obj_t* parent)
{
    stime_date_screen = lv_obj_create(parent);
    lv_obj_set_style_bg_color(stime_date_screen, lv_color_black(), 0);
    lv_obj_set_size(stime_date_screen, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(stime_date_screen, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(stime_date_screen, screen_events, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(stime_date_screen, on_delete, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(stime_date_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(stime_date_screen, 20, 0);
    lv_obj_set_style_pad_gap(stime_date_screen, 14, 0);
    lv_obj_set_scrollbar_mode(stime_date_screen, LV_SCROLLBAR_MODE_AUTO);

    // Title
    lv_obj_t* title = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(title, &font_bold_32, 0);
    lv_label_set_text(title, "Time & Display");

    // ===== WATCHFACE STYLE SECTION =====
    lv_obj_t* style_label = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(style_label, &font_normal_26, 0);
    lv_obj_set_style_text_color(style_label, lv_color_hex(0x888888), 0);
    lv_label_set_text(style_label, "WATCHFACE STYLE");

    // Style dropdown
    style_dropdown = lv_dropdown_create(stime_date_screen);
    lv_obj_set_width(style_dropdown, lv_pct(100));
    lv_obj_set_style_text_font(style_dropdown, &font_normal_26, 0);
    
    // Build options string
    char options[256] = {0};
    for (int i = 0; i < WATCHFACE_STYLE_COUNT; i++) {
        strcat(options, style_names[i]);
        if (i < WATCHFACE_STYLE_COUNT - 1) strcat(options, "\n");
    }
    lv_dropdown_set_options(style_dropdown, options);
    
    // Set current selection
    uint8_t current_style = settings_get_watchface_style();
    if (current_style >= WATCHFACE_STYLE_COUNT) current_style = 0;
    lv_dropdown_set_selected(style_dropdown, current_style);
    
    lv_obj_add_event_cb(style_dropdown, style_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Spacer
    lv_obj_t* spacer1 = lv_obj_create(stime_date_screen);
    lv_obj_set_size(spacer1, lv_pct(100), 2);
    lv_obj_set_style_bg_color(spacer1, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(spacer1, LV_OPA_50, 0);
    lv_obj_set_style_border_width(spacer1, 0, 0);

    // ===== TIME SECTION =====
    lv_obj_t* time_header = lv_obj_create(stime_date_screen);
    lv_obj_set_size(time_header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(time_header, 0, 0);
    lv_obj_set_style_border_width(time_header, 0, 0);
    lv_obj_set_style_pad_all(time_header, 0, 0);
    lv_obj_set_flex_flow(time_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* time_label = lv_label_create(time_header);
    lv_obj_set_style_text_font(time_label, &font_normal_26, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x888888), 0);
    lv_label_set_text(time_label, "TIME");

    // Time format toggle button
    lv_obj_t* format_btn = lv_btn_create(time_header);
    lv_obj_set_size(format_btn, 70, 40);
    lv_obj_add_event_cb(format_btn, format_toggle_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* format_lbl = lv_label_create(format_btn);
    lv_obj_set_style_text_font(format_lbl, &font_normal_26, 0);
    bool is_24h = settings_get_time_format_24h();
    lv_label_set_text(format_lbl, is_24h ? "24H" : "12H");
    lv_obj_center(format_lbl);

    time_display = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(time_display, &font_numbers_80, 0);
    lv_obj_set_style_text_color(time_display, lv_color_hex(0x00BFFF), 0);
    lv_label_set_text(time_display, "00:00");

    // Time adjustment buttons
    lv_obj_t* time_btns = lv_obj_create(stime_date_screen);
    lv_obj_set_size(time_btns, lv_pct(100), 60);
    lv_obj_set_style_bg_opa(time_btns, 0, 0);
    lv_obj_set_style_border_width(time_btns, 0, 0);
    lv_obj_set_style_pad_all(time_btns, 0, 0);
    lv_obj_set_flex_flow(time_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char* time_btn_labels[] = {"H-", "H+", "M-", "M+"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(time_btns);
        lv_obj_set_size(btn, 80, 55);
        lv_obj_add_event_cb(btn, time_btn_clicked, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        
        lv_obj_t* lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, &font_bold_32, 0);
        lv_label_set_text(lbl, time_btn_labels[i]);
        lv_obj_center(lbl);
    }

    // Spacer
    lv_obj_t* spacer2 = lv_obj_create(stime_date_screen);
    lv_obj_set_size(spacer2, lv_pct(100), 2);
    lv_obj_set_style_bg_color(spacer2, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(spacer2, LV_OPA_50, 0);
    lv_obj_set_style_border_width(spacer2, 0, 0);

    // ===== DATE SECTION =====
    lv_obj_t* date_label = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(date_label, &font_normal_26, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0x888888), 0);
    lv_label_set_text(date_label, "DATE");

    date_display = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(date_display, &font_normal_28, 0);
    lv_obj_set_style_text_color(date_display, lv_color_hex(0x90F090), 0);
    lv_label_set_text(date_display, "Mon, Jan 01, 2025");
    lv_label_set_long_mode(date_display, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(date_display, lv_pct(100));

    // Date adjustment buttons - Row 1
    lv_obj_t* date_btns1 = lv_obj_create(stime_date_screen);
    lv_obj_set_size(date_btns1, lv_pct(100), 55);
    lv_obj_set_style_bg_opa(date_btns1, 0, 0);
    lv_obj_set_style_border_width(date_btns1, 0, 0);
    lv_obj_set_style_pad_all(date_btns1, 0, 0);
    lv_obj_set_flex_flow(date_btns1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(date_btns1, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char* date_btn_labels1[] = {"Y-", "Y+", "M-", "M+"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(date_btns1);
        lv_obj_set_size(btn, 80, 50);
        lv_obj_add_event_cb(btn, date_btn_clicked, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        
        lv_obj_t* lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, &font_normal_28, 0);
        lv_label_set_text(lbl, date_btn_labels1[i]);
        lv_obj_center(lbl);
    }

    // Date adjustment buttons - Row 2
    lv_obj_t* date_btns2 = lv_obj_create(stime_date_screen);
    lv_obj_set_size(date_btns2, lv_pct(100), 55);
    lv_obj_set_style_bg_opa(date_btns2, 0, 0);
    lv_obj_set_style_border_width(date_btns2, 0, 0);
    lv_obj_set_style_pad_all(date_btns2, 0, 0);
    lv_obj_set_flex_flow(date_btns2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(date_btns2, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char* date_btn_labels2[] = {"D-", "D+"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t* btn = lv_btn_create(date_btns2);
        lv_obj_set_size(btn, 80, 50);
        lv_obj_add_event_cb(btn, date_btn_clicked, LV_EVENT_CLICKED, (void*)(uintptr_t)(i + 4));
        
        lv_obj_t* lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, &font_normal_28, 0);
        lv_label_set_text(lbl, date_btn_labels2[i]);
        lv_obj_center(lbl);
    }

    update_displays();
}

static void on_delete(lv_event_t* e)
{
    struct tm time;
    if (rtc_get_time(&time) == ESP_OK) {
        settings_save_time(&time);
        ESP_LOGI(TAG, "Time saved");
    }
    stime_date_screen = NULL;
}

lv_obj_t* setting_time_date_screen_get(void)
{
    if (!stime_date_screen) {
        bsp_display_lock(0);
        setting_time_date_screen_create(NULL);
        bsp_display_unlock();
    }
    return stime_date_screen;
}