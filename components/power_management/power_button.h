#ifndef POWER_BUTTON_H
#define POWER_BUTTON_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t power_button_init(void);
bool power_button_long_press_detected(void);
void power_button_clear_long_press(void);
void enter_deep_sleep(void);

#ifdef __cplusplus
}
#endif

#endif