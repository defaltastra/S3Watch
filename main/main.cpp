#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "display_manager.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lvgl.h"
#include "sensors.h"
#include "settings.h"
#include "ui.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "audio_alert.h"
#include "ble_sync.h"
#include "media_player.h"
#include "esp_lvgl_port.h"
#include "rtc_lib.h"
#include "driver/gpio.h"
#include "power_button.h"
static const char *TAG = "MAIN";

// Power button GPIO (adjust if your hardware uses a different pin)
#define POWER_BUTTON_GPIO GPIO_NUM_0

// Timer for periodic time backup to NVS
static TimerHandle_t s_time_backup_timer = NULL;

static void power_init(void) {
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
}

// Periodic backup callback - saves RTC time to NVS every 5 minutes
static void time_backup_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    struct tm current_time;
    if (rtc_get_time(&current_time) == ESP_OK) {
        settings_save_time(&current_time);
        ESP_LOGI("TIME_BACKUP", "⏰ Time backed up to NVS: %04d-%02d-%02d %02d:%02d:%02d",
                 current_time.tm_year + 1900, current_time.tm_mon + 1, current_time.tm_mday,
                 current_time.tm_hour, current_time.tm_min, current_time.tm_sec);
    } else {
        ESP_LOGW("TIME_BACKUP", "Failed to read RTC for backup");
    }
}

// Start the 5-minute time backup timer
static void start_time_backup(void)
{
    const TickType_t backup_interval = pdMS_TO_TICKS(300000); // 5 minutes
    
    s_time_backup_timer = xTimerCreate(
        "time_backup",           // Timer name
        backup_interval,         // Period: 5 minutes
        pdTRUE,                  // Auto-reload (repeating timer)
        NULL,                    // Timer ID
        time_backup_timer_cb     // Callback function
    );
    
    if (s_time_backup_timer != NULL) {
        if (xTimerStart(s_time_backup_timer, 0) == pdPASS) {
            ESP_LOGI("TIME_BACKUP", "✅ Started 5-minute time backup timer");
        } else {
            ESP_LOGE("TIME_BACKUP", "Failed to start backup timer");
        }
    } else {
        ESP_LOGE("TIME_BACKUP", "Failed to create backup timer");
    }
}

// Enter deep sleep mode - RTC stays powered and running
extern "C" void enter_deep_sleep(void)
{
    ESP_LOGI("POWER", "=== Preparing for deep sleep ===");
    
    // Stop the backup timer
    if (s_time_backup_timer != NULL) {
        xTimerStop(s_time_backup_timer, portMAX_DELAY);
        xTimerDelete(s_time_backup_timer, portMAX_DELAY);
        s_time_backup_timer = NULL;
    }
    
    // Save current time to NVS
    struct tm current_time;
    if (rtc_get_time(&current_time) == ESP_OK) {
        settings_save_time(&current_time);
        ESP_LOGI("POWER", "💾 Time saved: %04d-%02d-%02d %02d:%02d:%02d",
                 current_time.tm_year + 1900, current_time.tm_mon + 1, 
                 current_time.tm_mday, current_time.tm_hour, 
                 current_time.tm_min, current_time.tm_sec);
    }
    
    settings_save();
    bsp_display_brightness_set(0);
    
    // Configure power domains for ESP32-S3
    // Keep RTC peripherals (including I2C for external RTC) powered
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    
    // On ESP32-S3, the external RTC chip stays powered if:
    // 1. It has its own battery/supercap
    // 2. The I2C bus power is maintained (check your schematic)
    // The ESP32-S3's internal RTC will also keep running
    
    // Wake sources
    esp_sleep_enable_ext0_wakeup(POWER_BUTTON_GPIO, 0);
    
    ESP_LOGI("POWER", "💤 Entering deep sleep");
    ESP_LOGI("POWER", "🔋 RTC will continue running!");
    
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_deep_sleep_start();
}

// Forward declaration of power button functions
esp_err_t power_button_init(void);
bool power_button_long_press_detected(void);
void power_button_clear_long_press(void);

extern "C" void app_main(void) {
    // Check wake-up reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI("BOOT", "🔋 Woke from deep sleep (button press)");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI("BOOT", "⏰ Woke from deep sleep (timer)");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            ESP_LOGI("BOOT", "❄️  Cold boot (power on or reset)");
            break;
    }
    
    power_init();
    esp_event_loop_create_default();
    display_manager_pm_early_init();
    
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_priority = 4;
    lvgl_cfg.task_stack = 32768;
    lvgl_cfg.task_affinity = -1;
    lvgl_cfg.task_max_sleep_ms = 500;
    lvgl_cfg.timer_period_ms = 5;
    
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));
    
    bsp_display_start();
    bsp_extra_init();
    
    ESP_LOGI(TAG, "Starting RTC timer...");
    esp_err_t rtc_err = rtc_start();
    if (rtc_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start RTC: %s", esp_err_to_name(rtc_err));
    } else {
        ESP_LOGI(TAG, "RTC started successfully");
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    settings_init();
    
    // Start periodic time backup to NVS (every 5 minutes)
    start_time_backup();
    
    // Initialize power button handler for long-press detection
    ESP_LOGI(TAG, "Initializing power button...");
    esp_err_t btn_err = power_button_init();
    if (btn_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init power button: %s", esp_err_to_name(btn_err));
    } else {
        ESP_LOGI(TAG, "Power button ready (hold 3s to sleep)");
    }
    
    media_player_init_lvgl_fs();
    
    esp_err_t ble_cfg_err = ble_sync_set_enabled(settings_get_bluetooth_enabled());
    if (ble_cfg_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply stored BLE state: %s", esp_err_to_name(ble_cfg_err));
    }
    
    xTaskCreate(ui_task, "ui", 8000, NULL, 4, NULL);
    audio_alert_play_startup();
    
    esp_pm_config_t pm_cfg = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_cfg));
    
    ESP_LOGI(TAG, "✅ Smartwatch initialized. Time will persist through sleep!");
}