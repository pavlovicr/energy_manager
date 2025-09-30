#include "app_tasks.h"
#include "energy_manager.h"
#include "ui/display_manager.h"
#include "communication/wifi/wifi_manager.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "APP_TASKS";

// Task handles
static TaskHandle_t emma_task_handle = NULL;
static TaskHandle_t analysis_task_handle = NULL;
static TaskHandle_t display_task_handle = NULL;
static TaskHandle_t wifi_monitor_handle = NULL;

// Task parametri
#define EMMA_TASK_STACK_SIZE        4096
#define ANALYSIS_TASK_STACK_SIZE    3072
#define DISPLAY_TASK_STACK_SIZE     4096
#define WIFI_MONITOR_STACK_SIZE     3072

#define EMMA_TASK_PRIORITY          5
#define ANALYSIS_TASK_PRIORITY      4
#define DISPLAY_TASK_PRIORITY       3
#define WIFI_MONITOR_PRIORITY       2

// Intervali (ms)
#define EMMA_UPDATE_INTERVAL        15000   // 15 sekund
#define ANALYSIS_INTERVAL           5000    // 5 sekund
#define DISPLAY_UPDATE_INTERVAL     2000    // 2 sekundi
#define WIFI_MONITOR_INTERVAL       10000   // 10 sekund

// Task funkcije
static void emma_communication_task(void *param);
static void energy_analysis_task(void *param);
static void display_update_task(void *param);
static void wifi_monitor_task(void *param);

esp_err_t app_tasks_start(void)
{
    ESP_LOGI(TAG, "Starting application tasks");
    
    // EMMA komunikacija task
    BaseType_t ret = xTaskCreate(emma_communication_task, "emma_comm", 
                                EMMA_TASK_STACK_SIZE, NULL, 
                                EMMA_TASK_PRIORITY, &emma_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create EMMA communication task");
        return ESP_FAIL;
    }
    
    // Energetska analiza task
    ret = xTaskCreate(energy_analysis_task, "energy_analysis", 
                     ANALYSIS_TASK_STACK_SIZE, NULL, 
                     ANALYSIS_TASK_PRIORITY, &analysis_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create energy analysis task");
        return ESP_FAIL;
    }
    
    // Display posodabljanje task
    ret = xTaskCreate(display_update_task, "display_ui", 
                     DISPLAY_TASK_STACK_SIZE, NULL, 
                     DISPLAY_TASK_PRIORITY, &display_task_handle);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to create display task - continuing without UI");
    }
    
    // WiFi monitoring task
    ret = xTaskCreate(wifi_monitor_task, "wifi_monitor", 
                     WIFI_MONITOR_STACK_SIZE, NULL, 
                     WIFI_MONITOR_PRIORITY, &wifi_monitor_handle);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to create WiFi monitor task");
    }
    
    ESP_LOGI(TAG, "Application tasks started successfully");
    return ESP_OK;
}

esp_err_t app_tasks_stop(void)
{
    ESP_LOGI(TAG, "Stopping application tasks");
    
    if (emma_task_handle) {
        vTaskDelete(emma_task_handle);
        emma_task_handle = NULL;
    }
    
    if (analysis_task_handle) {
        vTaskDelete(analysis_task_handle);
        analysis_task_handle = NULL;
    }
    
    if (display_task_handle) {
        vTaskDelete(display_task_handle);
        display_task_handle = NULL;
    }
    
    if (wifi_monitor_handle) {
        vTaskDelete(wifi_monitor_handle);
        wifi_monitor_handle = NULL;
    }
    
    ESP_LOGI(TAG, "All tasks stopped");
    return ESP_OK;
}

task_status_t app_tasks_get_status(void)
{
    task_status_t status = {0};
    
    status.emma_task_running = (emma_task_handle != NULL);
    status.analysis_task_running = (analysis_task_handle != NULL);
    status.display_task_running = (display_task_handle != NULL);
    status.wifi_monitor_running = (wifi_monitor_handle != NULL);
    
    status.total_tasks = uxTaskGetNumberOfTasks();
    status.free_heap = esp_get_free_heap_size();
    
    return status;
}

// Task implementacije
static void emma_communication_task(void *param)
{
    ESP_LOGI(TAG, "EMMA communication task started");
    
    // Poskusi povezavo z EMMA
    energy_manager_connect_emma();
    
    while (1) {
     //   esp_task_wdt_reset();
        
        // Posodobi EMMA meritve
        esp_err_t ret = energy_manager_update_measurements();
        
        if (ret == ESP_OK) {
            ESP_LOGD(TAG, "EMMA measurements updated successfully");
        } else {
            ESP_LOGW(TAG, "Failed to update EMMA measurements");
        }
        
        vTaskDelay(pdMS_TO_TICKS(EMMA_UPDATE_INTERVAL));
    }
}

static void energy_analysis_task(void *param)
{
    ESP_LOGI(TAG, "Energy analysis task started");
    
    while (1) {
    //    esp_task_wdt_reset();
        
        // Analiziraj energetsko situacijo in izvedi kontrolo
        energy_state_t state = energy_manager_analyze_and_control();
        
        ESP_LOGD(TAG, "Energy state: %s", energy_manager_get_status_string());
        
        vTaskDelay(pdMS_TO_TICKS(ANALYSIS_INTERVAL));
    }
}

static void display_update_task(void *param)
{
    ESP_LOGI(TAG, "Display update task started");
    
    while (1) {
    //    esp_task_wdt_reset();
        
        // Preveri če imamo veljavne EMMA podatke
        const emma_measurements_t* data = energy_manager_get_latest_data();
        if (data != NULL) {
            // Posodobi display z EMMA podatki
            display_manager_update_emma_measurements(data);
            
            // Posodobi status povezave
            display_manager_update_connection_status(
                energy_manager_is_emma_connected(),
                wifi_manager_is_connected(),
                energy_manager_get_success_rate()
            );
            
            // Posodobi sistem status
            display_manager_update_system_status(energy_manager_get_status_string());
        }
        
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_INTERVAL));
    }
}

static void wifi_monitor_task(void *param)
{
    ESP_LOGI(TAG, "WiFi monitor task started");
    
    // Poskusi povezavo z WiFi
    esp_err_t wifi_manager_connect(const char* ssid, const char* password);
    
    while (1) {
    //    esp_task_wdt_reset();
        
        // Preveri WiFi status
        if (!wifi_manager_is_connected()) {
            ESP_LOGW(TAG, "WiFi disconnected, attempting reconnect");
            wifi_manager_reconnect();
        }
        
        // Izpiši WiFi status vsakih 60 sekund
        static int wifi_counter = 0;
        if (++wifi_counter % 6 == 0) {
            wifi_info_t info = wifi_manager_get_info();
            ESP_LOGI(TAG, "WiFi status: %s, RSSI: %d dBm", 
                     wifi_manager_get_status_string(info.status), info.rssi);
        }
        
        vTaskDelay(pdMS_TO_TICKS(WIFI_MONITOR_INTERVAL));
    }
}