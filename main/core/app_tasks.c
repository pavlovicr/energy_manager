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
#define DISPLAY_UPDATE_INTERVAL     1000    // 1 sekunda
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
    ESP_LOGI(TAG, "EMMA communication task started");
    
    // Energetska analiza task
    ret = xTaskCreate(energy_analysis_task, "energy_analysis", 
                     ANALYSIS_TASK_STACK_SIZE, NULL, 
                     ANALYSIS_TASK_PRIORITY, &analysis_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create energy analysis task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Energy analysis task started");
    
    // Display posodabljanje task
    ret = xTaskCreate(display_update_task, "display_ui", 
                     DISPLAY_TASK_STACK_SIZE, NULL, 
                     DISPLAY_TASK_PRIORITY, &display_task_handle);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to create display task - continuing without UI");
    } else {
        ESP_LOGI(TAG, "Display update task started");
    }
    
    // WiFi monitoring task
    ret = xTaskCreate(wifi_monitor_task, "wifi_monitor", 
                     WIFI_MONITOR_STACK_SIZE, NULL, 
                     WIFI_MONITOR_PRIORITY, &wifi_monitor_handle);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to create WiFi monitor task");
    } else {
        ESP_LOGI(TAG, "WiFi monitor task started");
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
    ESP_LOGI(TAG, "EMMA communication task running");
    
    // Poskusi povezavo z EMMA
    energy_manager_connect_emma();
    
    TickType_t last_wake = xTaskGetTickCount();
    
    while (1) {
        // Posodobi EMMA meritve
        esp_err_t ret = energy_manager_update_measurements();
        
        if (ret == ESP_OK) {
            ESP_LOGD(TAG, "EMMA measurements updated successfully");
        } else {
            ESP_LOGW(TAG, "Failed to update EMMA measurements");
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(EMMA_UPDATE_INTERVAL));
    }
}

static void energy_analysis_task(void *param)
{
    ESP_LOGI(TAG, "Energy analysis task running");
    
    TickType_t last_wake = xTaskGetTickCount();
    
    while (1) {
        // Analiziraj energetsko situacijo in izvedi kontrolo
        energy_state_t state = energy_manager_analyze_and_control();
        
        ESP_LOGD(TAG, "Energy state: %s", energy_manager_get_status_string());
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ANALYSIS_INTERVAL));
    }
}

static void display_update_task(void *param)
{
    ESP_LOGI(TAG, "Display update task running");
    
    TickType_t last_wake = xTaskGetTickCount();
    
    while (1) {
        // Pridobi najnovejše meritve - POPRAVLJENO IME FUNKCIJE
        const emma_measurements_t* measurements = energy_manager_get_latest_data();

        ESP_LOGI(TAG, "Display task: measurements=%p", measurements);  // DODAJ
        
        if (measurements) {
            ESP_LOGI(TAG, "Updating display with voltage=%.1f", measurements->phase_a_voltage);  // DODAJ
            // Posodobi meritve na zaslonu
            display_manager_update_emma_measurements(measurements);
            
            // Posodobi statuse povezav
            bool emma_connected = energy_manager_is_emma_connected();
            bool wifi_connected = wifi_manager_is_connected();
            float success_rate = energy_manager_get_success_rate();
            
            ESP_LOGI(TAG, "Updating status: emma=%d, wifi=%d, rate=%.0f", emma_connected, wifi_connected, success_rate);  // DODAJ
            
            display_manager_update_connection_status(emma_connected, wifi_connected, success_rate);
            
            // Posodobi sistemski status
            energy_state_t state = energy_manager_get_current_state();
            const char *state_str = (state == ENERGY_STATE_NORMAL) ? "NORMAL" : 
                                   (state == ENERGY_STATE_INITIALIZING) ? "INIT" : "ERROR";
            display_manager_update_system_status(state_str);
        
        } else {
            ESP_LOGW(TAG, "No measurements available ");
        }   
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(DISPLAY_UPDATE_INTERVAL));
    }
}

static void wifi_monitor_task(void *param)
{
    ESP_LOGI(TAG, "WiFi monitor task running");
    
    TickType_t last_wake = xTaskGetTickCount();
    int wifi_counter = 0;
    
    while (1) {
        // Preveri WiFi status
        if (!wifi_manager_is_connected()) {
            ESP_LOGW(TAG, "WiFi disconnected, attempting reconnect");
            wifi_manager_reconnect();
        }
        
        // Izpiši WiFi status vsakih 60 sekund
        if (++wifi_counter % 6 == 0) {
            wifi_info_t info = wifi_manager_get_info();
            ESP_LOGI(TAG, "WiFi status: %s, RSSI: %d dBm", 
                     wifi_manager_get_status_string(info.status), info.rssi);
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(WIFI_MONITOR_INTERVAL));
    }
}