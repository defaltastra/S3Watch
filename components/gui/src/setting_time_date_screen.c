#include "setting_time_date_screen.h"
#include "ui.h"
#include "ui_fonts.h"
#include "settings.h"
#include "rtc_lib.h"
#include "esp_log.h"
#include "settings_menu_screen.h"
#include <time.h>

static lv_obj_t* stime_date_screen;
static lv_obj_t* time_display;
static lv_obj_t* date_display;

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

static void update_displays(void)
{
    struct tm time;
    if (rtc_get_time(&time) != ESP_OK) return;
    
    bool locked = bsp_display_lock(0);
    if (time_display) {
        lv_label_set_text_fmt(time_display, "%02d:%02d", time.tm_hour, time.tm_min);
    }
    if (date_display) {
        lv_label_set_text_fmt(date_display, "%04d-%02d-%02d", 
                              time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
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
            if (time.tm_mon > 0) time.tm_mon--;
            else { time.tm_mon = 11; if (time.tm_year > 0) time.tm_year--; }
            break;
        case 3: // Month +
            if (time.tm_mon < 11) time.tm_mon++;
            else { time.tm_mon = 0; if (time.tm_year < 200) time.tm_year++; }
            break;
        case 4: // Day -
            if (time.tm_mday > 1) time.tm_mday--;
            break;
        case 5: // Day +
            if (time.tm_mday < 28) time.tm_mday++;
            break;
    }
    
    rtc_set_time(&time);
    update_displays();
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
    lv_obj_set_style_pad_gap(stime_date_screen, 16, 0);

    // Title
    lv_obj_t* title = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(title, &font_bold_32, 0);
    lv_label_set_text(title, "Set Time & Date");

    // TIME section
    lv_obj_t* time_label = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(time_label, &font_normal_26, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x888888), 0);
    lv_label_set_text(time_label, "TIME");

    time_display = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(time_display, &font_numbers_80, 0);
    lv_label_set_text(time_display, "00:00");

    // Time buttons
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
    lv_obj_t* spacer = lv_obj_create(stime_date_screen);
    lv_obj_set_size(spacer, lv_pct(100), 2);
    lv_obj_set_style_bg_color(spacer, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_50, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);

    // DATE section
    lv_obj_t* date_label = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(date_label, &font_normal_26, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0x888888), 0);
    lv_label_set_text(date_label, "DATE");

    date_display = lv_label_create(stime_date_screen);
    lv_obj_set_style_text_font(date_display, &font_bold_32, 0);
    lv_label_set_text(date_display, "2025-01-01");

    // Date buttons (2 rows)
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