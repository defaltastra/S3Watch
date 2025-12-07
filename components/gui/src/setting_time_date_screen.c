#include "setting_time_date_screen.h"
#include "ui.h"
#include "ui_fonts.h"
#include "settings.h"
#include "rtc_lib.h"
#include "esp_log.h"
#include "settings_menu_screen.h"
#include <time.h>

static lv_obj_t* stime_date_screen;
static lv_obj_t* format_switch;
static lv_obj_t* hour_label;
static lv_obj_t* minute_label;
static lv_obj_t* year_label;
static lv_obj_t* month_label;
static lv_obj_t* day_label;
static void on_delete(lv_event_t* e);
static const char* TAG = "TimeDate";

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

static void update_time_display(void)
{
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    
    int hour = time.tm_hour;
    int minute = time.tm_min;
    bool is_24h = settings_get_time_format_24h();
    
    if (hour_label) {
        if (is_24h) {
            lv_label_set_text_fmt(hour_label, "%02d", hour);
        } else {
            int hour_12 = hour % 12;
            if (hour_12 == 0) hour_12 = 12;
            lv_label_set_text_fmt(hour_label, "%02d", hour_12);
        }
    }
    if (minute_label) {
        lv_label_set_text_fmt(minute_label, "%02d", minute);
    }
}

static void update_date_display(void)
{
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    
    if (year_label) {
        lv_label_set_text_fmt(year_label, "%04d", time.tm_year + 1900);
    }
    if (month_label) {
        lv_label_set_text_fmt(month_label, "%02d", time.tm_mon + 1);
    }
    if (day_label) {
        lv_label_set_text_fmt(day_label, "%02d", time.tm_mday);
    }
}

static void format_toggle(lv_event_t* e)
{
    bool is_24h = lv_obj_has_state(format_switch, LV_STATE_CHECKED);
    settings_set_time_format_24h(is_24h);
    update_time_display();
}

static void hour_minus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    time.tm_hour = (time.tm_hour > 0) ? time.tm_hour - 1 : 23;
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_time_display();
}

static void hour_plus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    time.tm_hour = (time.tm_hour < 23) ? time.tm_hour + 1 : 0;
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_time_display();
}

static void minute_minus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    if (time.tm_min > 0) {
        time.tm_min--;
    } else {
        time.tm_min = 59;
        time.tm_hour = (time.tm_hour > 0) ? time.tm_hour - 1 : 23;
    }
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_time_display();
}

static void minute_plus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    if (time.tm_min < 59) {
        time.tm_min++;
    } else {
        time.tm_min = 0;
        time.tm_hour = (time.tm_hour < 23) ? time.tm_hour + 1 : 0;
    }
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_time_display();
}

static void year_minus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    if (time.tm_year > 0) {
        time.tm_year--;
    }
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_date_display();
}

static void year_plus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    if (time.tm_year < 200) {
        time.tm_year++;
    }
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_date_display();
}

static void month_minus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    if (time.tm_mon > 0) {
        time.tm_mon--;
    } else {
        time.tm_mon = 11;
        if (time.tm_year > 0) time.tm_year--;
    }
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_date_display();
}

static void month_plus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    if (time.tm_mon < 11) {
        time.tm_mon++;
    } else {
        time.tm_mon = 0;
        if (time.tm_year < 200) time.tm_year++;
    }
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_date_display();
}

static void day_minus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    if (time.tm_mday > 1) {
        time.tm_mday--;
    } else {
        if (time.tm_mon > 0) {
            time.tm_mon--;
        } else {
            time.tm_mon = 11;
            if (time.tm_year > 0) time.tm_year--;
        }
        time.tm_mday = 31;
    }
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_date_display();
}

static void day_plus(lv_event_t* e)
{
    (void)e;
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    if (time.tm_mday < 28) {
        time.tm_mday++;
    } else {
        if (time.tm_mon < 11) {
            time.tm_mon++;
        } else {
            time.tm_mon = 0;
            if (time.tm_year < 200) time.tm_year++;
        }
        time.tm_mday = 1;
    }
    rtc_set_time(&time);
    settings_save_time(&time);  
    update_date_display();
}

static lv_obj_t* make_value_control(lv_obj_t* parent, const char* label_text, lv_obj_t** value_label, 
                                     lv_event_cb_t minus_cb, lv_event_cb_t plus_cb)
{
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(container, 8, 0);
    lv_obj_set_style_margin_bottom(container, 12, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* label = lv_label_create(container);
    lv_obj_set_style_text_font(label, &font_normal_28, 0);
    lv_label_set_text(label, label_text);
    
    lv_obj_t* box = lv_obj_create(container);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* btn_m = lv_btn_create(box);
    lv_obj_set_size(btn_m, 60, 60);
    lv_obj_add_event_cb(btn_m, minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lm = lv_label_create(btn_m);
    lv_obj_set_style_text_font(lm, &font_bold_32, 0);
    lv_label_set_text(lm, "-");
    
    *value_label = lv_label_create(box);
    lv_obj_set_style_text_font(*value_label, &font_numbers_80, 0);
    lv_obj_set_style_pad_hor(*value_label, 12, 0);
    lv_label_set_text(*value_label, "--");
    
    lv_obj_t* btn_p = lv_btn_create(box);
    lv_obj_set_size(btn_p, 60, 60);
    lv_obj_add_event_cb(btn_p, plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lp = lv_label_create(btn_p);
    lv_obj_set_style_text_font(lp, &font_bold_32, 0);
    lv_label_set_text(lp, "+");
    
    return container;
}

void setting_time_date_screen_create(lv_obj_t* parent)
{
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_bg_opa(&style, LV_OPA_COVER);

    stime_date_screen = lv_obj_create(parent);
    lv_obj_remove_style_all(stime_date_screen);
    lv_obj_add_style(stime_date_screen, &style, 0);
    lv_obj_set_size(stime_date_screen, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(stime_date_screen, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(stime_date_screen, LV_OBJ_FLAG_USER_1);
    lv_obj_add_event_cb(stime_date_screen, screen_events, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(stime_date_screen, on_delete, LV_EVENT_DELETE, NULL);

    // Header
    lv_obj_t* hdr = lv_obj_create(stime_date_screen);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_t* title = lv_label_create(hdr);
    lv_obj_set_style_text_font(title, &font_bold_32, 0);
    lv_label_set_text(title, "Time & Date");

    // Scrollable content
    lv_obj_t* content = lv_obj_create(stime_date_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, lv_pct(100), lv_pct(85));
    lv_obj_add_flag(content, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_style_pad_all(content, 16, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);

    // Time format toggle
    lv_obj_t* format_row = lv_obj_create(content);
    lv_obj_remove_style_all(format_row);
    lv_obj_set_size(format_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(format_row, 8, 0);
    lv_obj_set_style_margin_bottom(format_row, 20, 0);
    lv_obj_set_flex_flow(format_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(format_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* format_label = lv_label_create(format_row);
    lv_obj_set_style_text_font(format_label, &font_normal_28, 0);
    lv_label_set_text(format_label, "24-hour format");
    
    format_switch = lv_switch_create(format_row);
    lv_obj_set_size(format_switch, 100, 50);
    if (settings_get_time_format_24h()) {
        lv_obj_add_state(format_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(format_switch, format_toggle, LV_EVENT_VALUE_CHANGED, NULL);

    // Time controls
    make_value_control(content, "Hour", &hour_label, hour_minus, hour_plus);
    make_value_control(content, "Minute", &minute_label, minute_minus, minute_plus);

    // Date controls
    make_value_control(content, "Year", &year_label, year_minus, year_plus);
    make_value_control(content, "Month", &month_label, month_minus, month_plus);
    make_value_control(content, "Day", &day_label, day_minus, day_plus);

    update_time_display();
    update_date_display();
}

static void on_delete(lv_event_t* e)
{
    (void)e;
    ESP_LOGI(TAG, "Time & Date screen deleted");
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


