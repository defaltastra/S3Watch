#include "rtc_lib.h"
#include "pcf85063a.h"
#include <time.h>
#include "esp_timer.h"

static struct tm current_time;
static esp_timer_handle_t rtc_timer;

static const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *weekdaysshort[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
extern int rtc_register_read(uint8_t regAddr, uint8_t *data, uint8_t len);

static uint8_t bcd_to_dec(uint8_t val)
{
    return (val / 16 * 10) + (val % 16);
}

static void rtc_update_task(void *arg)
{
    pcf85063a_get_time(&current_time);
}

esp_err_t rtc_start(void)
{
    esp_err_t ret = pcf85063a_init();
    if (ret != ESP_OK) {
        return ret;
    }

    const esp_timer_create_args_t rtc_timer_args = {
        .callback = &rtc_update_task,
        .name = "rtc_timer"
    };

    ret = esp_timer_create(&rtc_timer_args, &rtc_timer);
    if (ret != ESP_OK) {
        return ret;
    }

    return esp_timer_start_periodic(rtc_timer, 1000000); // 1 second
}

esp_err_t rtc_get_time(struct tm *time)
{
    *time = current_time;
    return ESP_OK;
}

esp_err_t rtc_set_time(const struct tm *time)
{
    esp_err_t ret = pcf85063a_set_time(time);
    if (ret == ESP_OK) {
        // Update cached time immediately to avoid race condition
        current_time = *time;
    }
    return ret;
}

int rtc_get_hour(void)
{
    return current_time.tm_hour;
}

int rtc_get_minute(void)
{
    return current_time.tm_min;
}

int rtc_get_second(void)
{
    return current_time.tm_sec;
}

int rtc_get_day(void)
{
    return current_time.tm_mday;
}

int rtc_get_month(void)
{
    return current_time.tm_mon + 1;
}

int rtc_get_year(void)
{
    return current_time.tm_year + 1900;
}

const char *rtc_get_weekday_string(void)
{
    int wday = current_time.tm_wday;
    if (wday < 0 || wday > 6) wday = 0; // Default to Sunday if invalid
    return weekdays[wday];
}

const char *rtc_get_weekday_short_string(void)
{
    int wday = current_time.tm_wday;
    if (wday < 0 || wday > 6) wday = 0; // Default to Sunday if invalid
    return weekdaysshort[wday];
}

const char *rtc_get_month_string(void)
{
    int mon = current_time.tm_mon;
    if (mon < 0 || mon > 11) mon = 0; // Default to January if invalid
    return months[mon];
}

void rtc_debug_dump(void)
{
    uint8_t ctrl1, ctrl2;
    uint8_t time_regs[7];
    
    rtc_register_read(0x00, &ctrl1, 1);  // Control 1
    rtc_register_read(0x01, &ctrl2, 1);  // Control 2
    rtc_register_read(0x04, time_regs, 7);  // Time registers
    
    ESP_LOGI("RTC_DEBUG", "=== PCF85063 Register Dump ===");
    ESP_LOGI("RTC_DEBUG", "CTRL1: 0x%02X (STOP=%d, 12/24=%d, CAP_SEL=%d)",
             ctrl1, (ctrl1 >> 5) & 1, (ctrl1 >> 2) & 1, ctrl1 & 1);
    ESP_LOGI("RTC_DEBUG", "CTRL2: 0x%02X", ctrl2);
    ESP_LOGI("RTC_DEBUG", "Raw time: SEC=0x%02X MIN=0x%02X HOUR=0x%02X DAY=0x%02X WDAY=0x%02X MON=0x%02X YEAR=0x%02X",
             time_regs[0], time_regs[1], time_regs[2], time_regs[3], 
             time_regs[4], time_regs[5], time_regs[6]);
    ESP_LOGI("RTC_DEBUG", "Decoded: %02d:%02d:%02d %02d/%02d/20%02d",
             bcd_to_dec(time_regs[2] & 0x3F),
             bcd_to_dec(time_regs[1] & 0x7F),
             bcd_to_dec(time_regs[0] & 0x7F),
             bcd_to_dec(time_regs[3] & 0x3F),
             bcd_to_dec(time_regs[5] & 0x1F),
             bcd_to_dec(time_regs[6]));
    ESP_LOGI("RTC_DEBUG", "=============================");
}