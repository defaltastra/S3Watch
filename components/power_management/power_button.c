#include "power_button.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "PWR_BTN";
static volatile bool long_press_detected = false;
static TaskHandle_t button_task_handle = NULL;

#define LONG_PRESS_DURATION_MS 3000

static void power_button_task(void *arg) {
    uint32_t press_start = 0;
    bool was_pressed = false;
    bool long_press_triggered = false;
    uint32_t heartbeat = 0;
    
    ESP_LOGI(TAG, "🔋 Power button monitor started (using PMU power key)");
    
    while (1) {
        // Poll the PMU power button
        // Note: bsp_power_poll_pwr_button_short() returns true once per short press
        // We need to implement our own long-press detection
        
        // Check if button is currently being held
        // We'll detect this by checking if short press hasn't fired yet
        bool is_pressed = false;
        
        // Simple polling: call the short press function but track timing ourselves
        static bool button_down = false;
        
        // Heartbeat log every 10 seconds
        if (++heartbeat % 200 == 0) {
            ESP_LOGI(TAG, "💓 Task alive, monitoring PMU button");
        }
        
        // The BSP only gives us short press detection, so we need to implement
        // long press ourselves by monitoring the PMU IRQ status directly
        // For now, let's use a workaround with GPIO 0 (boot button)
        
        // Use GPIO 0 as a temporary solution
        static bool gpio_init_done = false;
        if (!gpio_init_done) {
            gpio_config_t io_conf = {
                .pin_bit_mask = (1ULL << GPIO_NUM_0),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&io_conf);
            gpio_init_done = true;
            ESP_LOGI(TAG, "Also monitoring GPIO 0 (boot button) for long press");
        }
        
        is_pressed = (gpio_get_level(GPIO_NUM_0) == 0);
        
        if (is_pressed && !was_pressed) {
            // Button just pressed
            press_start = xTaskGetTickCount();
            was_pressed = true;
            long_press_triggered = false;
            ESP_LOGI(TAG, "Button pressed, starting timer");
        } else if (is_pressed && was_pressed) {
            // Button still held
            uint32_t hold_time = (xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS;
            
            // Log progress every 500ms
            static uint32_t last_log = 0;
            if (hold_time - last_log >= 500) {
                ESP_LOGI(TAG, "⏱️  Holding: %lu ms / %d ms", hold_time, LONG_PRESS_DURATION_MS);
                last_log = hold_time;
            }
            
            if (hold_time >= LONG_PRESS_DURATION_MS && !long_press_triggered) {
                long_press_triggered = true;
                long_press_detected = true;
                ESP_LOGI(TAG, "🔋 LONG PRESS DETECTED!");
            }
        } else if (!is_pressed && was_pressed) {
            // Button released
            uint32_t hold_time = (xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS;
            was_pressed = false;
            ESP_LOGI(TAG, "Button released after %lu ms", hold_time);
            
            if (hold_time < LONG_PRESS_DURATION_MS) {
                ESP_LOGI(TAG, "Short press (not triggering sleep)");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t power_button_init(void) {
    ESP_LOGI(TAG, "Initializing power button monitor");
    ESP_LOGI(TAG, "Hold boot button (GPIO 0) for 3 seconds to enter deep sleep");
    
    BaseType_t task_ret = xTaskCreate(
        power_button_task,
        "pwr_btn",
        3072,  // Increased stack
        NULL,
        5,
        &button_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create power button task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Power button monitor initialized");
    return ESP_OK;
}

bool power_button_long_press_detected(void) {
    return long_press_detected;
}

void power_button_clear_long_press(void) {
    ESP_LOGI(TAG, "Clearing long press flag");
    long_press_detected = false;
}