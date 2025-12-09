#include "ui.h"
#include "ble_sync.h"
#include "bsp/esp-bsp.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "display_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "notifications.h"
#include "sensors.h"
#include "settings_screen.h"
#include "steps_screen.h"
#include "ui_fonts.h"
#include "watchface.h"
#include "batt_screen.h"
#include "brightness_screen.h"
#include "driver/gpio.h"
#include "lvgl_spiffs_fs.h"

static const char* TAG = "UI";

static lv_obj_t* main_screen;
static lv_obj_t* tile1;
static lv_obj_t* tile2;
static lv_obj_t* tile3;
static lv_obj_t* tile4;
static lv_obj_t* active_screen;
static lv_obj_t* dynamic_tile = NULL;
static lv_obj_t* dynamic_subtile = NULL;

static bool tile1_created = false;
static bool tile4_created = false;

lv_obj_t* get_main_screen(void) { return main_screen; }

static lv_style_t main_style;
static void tileview_change_cb(lv_event_t* e);

void init_theme(void) {
  lv_style_init(&main_style);
  lv_style_set_text_color(&main_style, lv_color_white());
  lv_style_set_bg_color(&main_style, lv_color_black());
  lv_style_set_border_color(&main_style, lv_color_black());
}

lv_style_t* ui_get_main_style(void) { return &main_style; }

void load_screen(lv_obj_t* current_screen, lv_obj_t* next_screen,
  lv_screen_load_anim_t anim) {
  if (active_screen != next_screen) {
    bsp_display_lock(300);
    lv_screen_load_anim(next_screen, anim, 300, 0, false);
    bsp_display_unlock();
    active_screen = next_screen;
  }
}

lv_obj_t* active_screen_get(void) { return active_screen; }

void swatch_tileview(void)
{
  ESP_LOGI(TAG, "Creating tileview...");
  main_screen = lv_tileview_create(NULL);
  lv_obj_set_size(main_screen, LV_PCT(100), LV_PCT(100));
  lv_obj_add_style(main_screen, &main_style, 0);
  lv_obj_set_scrollbar_mode(main_screen, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(main_screen, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);

  lv_obj_add_event_cb(main_screen, tileview_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

  ESP_LOGI(TAG, "Creating tile1 (notifications - empty)...");
  tile1 = lv_tileview_add_tile(main_screen, 0, 0, LV_DIR_BOTTOM);
  ESP_LOGI(TAG, "tile1 created (lazy load on demand)");

  ESP_LOGI(TAG, "Creating tile2 (watchface)...");
  tile2 = lv_tileview_add_tile(main_screen, 0, 1, (lv_dir_t)(LV_DIR_TOP | LV_DIR_BOTTOM | LV_DIR_LEFT | LV_DIR_RIGHT));
  watchface_create(tile2);
  ESP_LOGI(TAG, "tile2 done");

  ESP_LOGI(TAG, "Creating tile4 (controls - empty)...");
  tile4 = lv_tileview_add_tile(main_screen, 1, 1, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));
  ESP_LOGI(TAG, "tile4 created (lazy load on demand)");
  
  ESP_LOGI(TAG, "All tiles created successfully");
}

static void tileview_change_cb(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  
  lv_obj_t* act = lv_tileview_get_tile_active(main_screen);
  
  if (act == tile1 && !tile1_created) {
    ESP_LOGI(TAG, "LAZY LOAD: Creating notifications screen");
    notifications_screen_create(tile1);
    tile1_created = true;
    ESP_LOGI(TAG, "LAZY LOAD: Notifications screen created");
  }
  
  if (act == tile4 && !tile4_created) {
    ESP_LOGI(TAG, "LAZY LOAD: Creating control screen");
    control_screen_create(tile4);
    tile4_created = true;
    ESP_LOGI(TAG, "LAZY LOAD: Control screen created");
  }
  
  if (dynamic_subtile && act != dynamic_subtile) {
    ESP_LOGI(TAG, "Auto-clean: deleting dynamic subtile (3,1)");
    lv_obj_del_async(dynamic_subtile);
    dynamic_subtile = NULL;
  }
  
  if (dynamic_tile && act != dynamic_tile && act != dynamic_subtile) {
    ESP_LOGI(TAG, "Auto-clean: deleting dynamic tile (2,1)");
    lv_obj_del_async(dynamic_tile);
    dynamic_tile = NULL;
  }
}

lv_obj_t* ui_dynamic_tile_acquire(void) {
  if (!main_screen) return NULL;
  if (dynamic_tile) {
    lv_obj_clean(dynamic_tile);
    ESP_LOGI(TAG, "Reusing dynamic tile (2,1)");    
    return dynamic_tile;
  }
  dynamic_tile = lv_tileview_add_tile(main_screen, 2, 1, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));
  if (dynamic_tile) {
    lv_obj_update_layout(main_screen);
    ESP_LOGI(TAG, "Created dynamic tile (2,1)");
  }
  return dynamic_tile;
}

void ui_dynamic_tile_show(void) {  
  if (!dynamic_tile || !main_screen) return;
  ESP_LOGI(TAG, "Showing dynamic tile (2,1)");
  
  if (active_screen_get() != get_main_screen()) {
    load_screen(NULL, get_main_screen(), LV_SCR_LOAD_ANIM_NONE);
  }
  lv_tileview_set_tile(main_screen, dynamic_tile, LV_ANIM_ON);
  lv_tileview_set_tile(main_screen, dynamic_tile, LV_ANIM_ON);
}

lv_obj_t* ui_dynamic_subtile_acquire(void) {
  if (!main_screen) return NULL;
  if (dynamic_subtile) {
    lv_obj_clean(dynamic_subtile);
    return dynamic_subtile;
  }
  dynamic_subtile = lv_tileview_add_tile(main_screen, 3, 1, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));
  if (dynamic_subtile) {
    lv_obj_update_layout(main_screen); 
    lv_obj_update_layout(dynamic_tile); 
    ESP_LOGI(TAG, "Created dynamic subtile (3,1)");
  }
  return dynamic_subtile;
}

void ui_dynamic_subtile_show(void) {
  if (!dynamic_subtile || !main_screen) return;
  ESP_LOGI(TAG, "Showing dynamic tile (3,1)");
  if (active_screen_get() != get_main_screen()) {
    load_screen(NULL, get_main_screen(), LV_SCR_LOAD_ANIM_NONE);
  }
  lv_tileview_set_tile(main_screen, dynamic_subtile, LV_ANIM_ON);
  lv_tileview_set_tile(main_screen, dynamic_subtile, LV_ANIM_ON);
}

void ui_dynamic_subtile_close(void) {
  if (!main_screen) return;
  if (dynamic_subtile) {
    if (dynamic_tile) {
      lv_tileview_set_tile(main_screen, dynamic_tile, LV_ANIM_ON);
    }
    else if (tile4) {
      lv_tileview_set_tile(main_screen, tile4, LV_ANIM_ON);
    }
    ESP_LOGI(TAG, "Deleting dynamic subtile (3,1)");
    lv_obj_del_async(dynamic_subtile);
    dynamic_subtile = NULL;
  }
}

void ui_dynamic_tile_close(void) {
  if (!main_screen) return;
  if (dynamic_tile) {
    if (tile4) {
      lv_tileview_set_tile(main_screen, tile4, LV_ANIM_ON);
    }
    ESP_LOGI(TAG, "Deleting dynamic tile (2,1)");
    lv_obj_del_async(dynamic_tile);
    dynamic_tile = NULL;
  }
}

void create_main_screen(void) {
  ESP_LOGI(TAG, "create_main_screen: START");
  swatch_tileview();
  ESP_LOGI(TAG, "create_main_screen: swatch_tileview done, delaying...");
  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_LOGI(TAG, "create_main_screen: loading screen...");
  load_screen(NULL, get_main_screen(), LV_SCR_LOAD_ANIM_NONE);
  ESP_LOGI(TAG, "create_main_screen: setting tile to watchface...");
  lv_tileview_set_tile(main_screen, tile2, LV_ANIM_OFF);
  ESP_LOGI(TAG, "create_main_screen: COMPLETE");
}

void ui_show_messages_tile(void) {
  if (active_screen_get() != get_main_screen()) {
    load_screen(NULL, get_main_screen(), LV_SCR_LOAD_ANIM_OVER_TOP);
  }
  if (lv_tileview_get_tile_active(main_screen) != tile1) {
    lv_tileview_set_tile(main_screen, tile1, LV_ANIM_ON);
  }
}

void ui_init(void) {
  ESP_LOGI(TAG, "ui_init: START");
  bsp_display_lock(0);
  
  ESP_LOGI(TAG, "ui_init: initializing theme");
  init_theme();
  
  ESP_LOGI(TAG, "ui_init: registering SPIFFS");
  lvgl_spiffs_fs_register();
  
  ESP_LOGI(TAG, "ui_init: creating main screen");
  create_main_screen();

  ESP_LOGI(TAG, "ui_init: setting power state");
  {
    bool vbus = bsp_power_is_vbus_in();
    bool chg = bsp_power_is_charging();
    int pct = bsp_power_get_battery_percent();
    watchface_set_power_state(vbus, chg, pct);
  }

  bsp_display_unlock();
  ESP_LOGI(TAG, "ui_init: COMPLETE");
}

#define UI_BACK_BTN GPIO_NUM_0

static void ui_handle_back_async(void* user) {
  (void)user;

  if (active_screen_get() != get_main_screen()) {
    load_screen(NULL, get_main_screen(), LV_SCR_LOAD_ANIM_OVER_TOP);
  }

  if (dynamic_subtile) {
    ui_dynamic_subtile_close();
    return;
  }
  if (dynamic_tile) {
    ui_dynamic_tile_close();
  }

  if (lv_tileview_get_tile_active(main_screen) != tile2) {
    lv_tileview_set_tile(main_screen, tile2, LV_ANIM_ON);
  }
}

static void ui_back_btn_task(void* arg) {
  (void)arg;
  gpio_config_t io = {
      .pin_bit_mask = 1ULL << UI_BACK_BTN,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  (void)gpio_config(&io);
  int idle = gpio_get_level(UI_BACK_BTN);
  int prev = idle;
  TickType_t last_press = 0;
  const TickType_t debounce = pdMS_TO_TICKS(120);
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    int lvl = gpio_get_level(UI_BACK_BTN);
    if (prev != lvl) {
      prev = lvl;
      if (lvl != idle) {
        TickType_t now = xTaskGetTickCount();
        if (now - last_press > debounce) {
          last_press = now;
          lv_async_call(ui_handle_back_async, NULL);
        }
      }
    }
    if (display_manager_is_on()) {
      if (bsp_power_poll_pwr_button_short()) {
        lv_async_call(ui_handle_back_async, NULL);
      }
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
  }
}

static void power_ui_evt(void* handler_arg, esp_event_base_t base, int32_t id,
  void* event_data) {
  (void)handler_arg;
  (void)base;
  (void)id;
  bsp_power_event_payload_t* pl = (bsp_power_event_payload_t*)event_data;
  if (pl) {
    int pct = bsp_power_get_battery_percent();
    bsp_display_lock(0);
    watchface_set_power_state(pl->vbus_in, pl->charging, pct);
    bsp_display_unlock();
  }
}

static void ble_ui_evt(void* handler_arg, esp_event_base_t base, int32_t id,
  void* event_data) {
  (void)handler_arg;
  (void)base;
  (void)event_data;
  bool connected = (id == BLE_SYNC_EVT_CONNECTED);
  bsp_display_lock(0);
  watchface_set_ble_connected(connected);
  bsp_display_unlock();
}

static void power_poll_cb(lv_timer_t* t) {
  (void)t;
  bool vbus = bsp_power_is_vbus_in();
  bool chg = bsp_power_is_charging();
  int pct = bsp_power_get_battery_percent();
  bsp_display_lock(0);
  watchface_set_power_state(vbus, chg, pct);
  bsp_display_unlock();
}

void ui_task(void* pvParameters) {
  ESP_LOGI(TAG, "UI task started");

  ui_init();
  display_manager_init();

  esp_event_handler_register(BSP_POWER_EVENT_BASE, ESP_EVENT_ANY_ID,
    power_ui_evt, NULL);
  esp_event_handler_register(BLE_SYNC_EVENT_BASE, ESP_EVENT_ANY_ID, ble_ui_evt,
    NULL);

  xTaskCreate(ui_back_btn_task, "ui_back_btn", 2048, NULL, 5, NULL);

  lv_timer_t* t = lv_timer_create(power_poll_cb, 5000, NULL);
  lv_timer_ready(t);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}