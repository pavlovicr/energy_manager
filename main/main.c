#include "system_monitor.h"
#include "emma_manager.h"
#include "energy_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#define TAG "SYSTEM_MONITOR"

// External reference
extern emma_client_t g_emma;

static void monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System monitor task started");
    
    while (1) {
        system_print_status();
        vTaskDelay(pdMS_TO_TICKS(120000)); // Print every 2 minutes
    }
}

void system_print_status(void) {
    printf("\n=== SYSTEM STATUS ===\n");
    printf("EMMA Connection          : %s\n", 
           emma_is_connected() ? "CONNECTED" : "DISCONNECTED");
    printf("EMMA IP Address          : %s\n", 
           g_emma.modbus_client.ip_address);
    printf("Communication Success    : %.1f%%\n", 
           emma_get_success_rate());
    printf("Read Errors              : %d\n", 
           (unsigned int)g_emma.read_errors);
    printf("Read Success             : %d\n", 
           (unsigned int)g_emma.read_success);
    printf("Data Valid               : %s\n", 
           g_emma.measurements.data_valid ? "YES" : "NO");
    printf("Free Heap                : %d bytes\n", 
           (unsigned int)esp_get_free_heap_size());
    printf("Tasks Running            : %d\n", 
           uxTaskGetNumberOfTasks());
    printf("====================\n\n");
}

void system_monitor_start(void) {
    xTaskCreate(monitor_task, "monitor_task", 4096, NULL, 1, NULL);
}