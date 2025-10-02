/*
 * ESP32 Energy Manager - Glavna aplikacija
 * Minimalna main.c z modularno strukturo
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"

// Core moduli
#include "core/energy_manager.h"
#include "core/system_config.h" 
#include "core/app_tasks.h"

// Communication moduli
#include "communication/wifi/wifi_manager.h"

// UI moduli
#include "ui/display_manager.h"

#define TAG "MAIN"

// Prototipov
static esp_err_t nvs_flash_init_or_erase(void);
static void system_heartbeat(void);

esp_err_t display_manager_init(void);

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Energy Manager Starting...");
    
    // 1. Osnovne sistem inicializacije
    ESP_ERROR_CHECK(nvs_flash_init_or_erase());
    
    // 2. Watchdog konfiguracija
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000,  // 10 sekund
        .idle_core_mask = 0,
        .trigger_panic = false,
    };
    //////////////////////////////////////////////////////////////////////////////////////////
    //ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config));
    ////////////////////////////////////////////////////////////////////////////////////////////
    // 3. Sistem konfiguracija
    ESP_ERROR_CHECK(system_config_init());
    
    
    // 4. Core energetski manager
    ESP_ERROR_CHECK(energy_manager_init());
    
    // 5. Komunikacijski moduli
    ESP_ERROR_CHECK(wifi_manager_init());
    wifi_manager_connect("ONEfourTWO", "markoskacepozelenitrati");
    
    // 6. UI komponenta
    esp_err_t display_ret = display_manager_init();
    if (display_ret != ESP_OK) {
        ESP_LOGW(TAG, "Display init failed, continuing without UI");
    }
    
    // 7. Start aplikacijskih taskov
    ESP_ERROR_CHECK(app_tasks_start());
    
    ESP_LOGI(TAG, "System initialized successfully");
    
    // Main loop - minimalen monitoring
    while (1) {
       // esp_task_wdt_reset();
        system_heartbeat();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static esp_err_t nvs_flash_init_or_erase(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static void system_heartbeat(void)
{
    static int counter = 0;
    if (++counter % 12 == 0) { // vsakih 60 sekund
        ESP_LOGI(TAG, "System heartbeat: %s | Free heap: %lu bytes", 
                 energy_manager_get_status_string(),
                 (unsigned long)esp_get_free_heap_size());
    }
}