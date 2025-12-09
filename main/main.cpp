#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

static const char *TAG = "MAIN";

static void power_init(void) {
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
}

extern "C" void app_main(void) {
  power_init();
  esp_event_loop_create_default();
  display_manager_pm_early_init();

  // Override LVGL stack before BSP init
  lvgl_port_cfg_t lvgl_cfg = {
    .task_priority = 4,
    .task_stack = 32768,
    .task_affinity = -1,
    .task_max_sleep_ms = 500,
    .timer_period_ms = 5
  };

  // Initialize LVGL port first with custom config
  ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

  // Then initialize BSP display (it will use existing LVGL port)
  bsp_display_start();
  
  bsp_extra_init();
  settings_init();

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
}