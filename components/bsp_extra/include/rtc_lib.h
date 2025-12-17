#ifndef RTC_LIB_H
#define RTC_LIB_H

#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rtc_start(void);
esp_err_t rtc_get_time(struct tm *time);
esp_err_t rtc_set_time(const struct tm *time);
int rtc_get_hour(void);
int rtc_get_minute(void);
int rtc_get_second(void);
int rtc_get_day(void);
int rtc_get_month(void);
int rtc_get_year(void);
const char *rtc_get_weekday_string(void);
const char *rtc_get_weekday_short_string(void);
const char *rtc_get_month_string(void);
void rtc_debug_dump(void);  

#ifdef __cplusplus
}
#endif

#endif // RTC_LIB_H