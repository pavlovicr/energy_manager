/*
 * ESP32 Energy Management System
 * Author: ESP32 Developer
 * Date: 2025
 *
 * Description:
 * Smart energy management system that reads data from Huawei EMMA
 * and controls household appliances based on energy availability
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"

// Custom modules¸
#include "emma_modbus.h"
#include "wifi_manager.h"
#include "load_control.h"

#define TAG "ENERGY_MANAGER"

// Configuration
#define EMMA_IP_ADDRESS "192.168.64.101"
#define MEASUREMENT_INTERVAL_MS 15000
#define CONTROL_INTERVAL_MS 5000

// Global variables
static emma_client_t g_emma;
static TaskHandle_t measurement_task_handle = NULL;
static TaskHandle_t control_task_handle = NULL;

// WiFi configuration (move to separate module later)
#define WIFI_SSID      "ONEfourTWO"
#define WIFI_PASSWORD  "markoskacepozelenitrati"
#define MAXIMUM_RETRY  5

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

// Energy management parameters
typedef struct {
    float excess_power_threshold;      // kW - minimum excess power to enable loads
    float battery_high_soc_threshold;  // % - battery level to start using excess power
    float battery_low_soc_threshold;   // % - battery level to disable non-essential loads
    float grid_feed_limit;             // kW - maximum grid feed-in allowed
    bool load_control_enabled;
    bool grid_limit_active;
} energy_config_t;

static energy_config_t g_energy_config = {
    .excess_power_threshold = 1.0f,    // 1kW excess needed
    .battery_high_soc_threshold = 90.0f,
    .battery_low_soc_threshold = 20.0f,
    .grid_feed_limit = 8.0f,
    .load_control_enabled = true,
    .grid_limit_active = true,
};

// WiFi Event Handler (temporary - move to wifi_manager module)
static void event_handler(void* arg, esp_event_base_t event_base, 
                         int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization finished.");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Failed to connect WiFi");
    }
}

// Energy Management Logic
typedef enum {
    ENERGY_STATE_NORMAL,
    ENERGY_STATE_EXCESS_AVAILABLE,
    ENERGY_STATE_BATTERY_LOW,
    ENERGY_STATE_GRID_LIMITING,
} energy_state_t;

static energy_state_t analyze_energy_situation(const emma_measurements_t *measurements) {
    if (!measurements || !measurements->data_valid) {
        return ENERGY_STATE_NORMAL;
    }
    
    // Check battery level first
    if (measurements->soc_percent < g_energy_config.battery_low_soc_threshold) {
        ESP_LOGW(TAG, "Battery low: %.1f%% - reducing loads", measurements->soc_percent);
        return ENERGY_STATE_BATTERY_LOW;
    }
    
    // Check if we need to limit grid feed-in
    if (g_energy_config.grid_limit_active && 
        measurements->feed_in_power > g_energy_config.grid_feed_limit) {
        ESP_LOGW(TAG, "Grid feed-in too high: %.2f kW - enabling loads", 
                 measurements->feed_in_power);
        return ENERGY_STATE_GRID_LIMITING;
    }
    
    // Check for excess power availability
    float excess_power = measurements->pv_output_power - measurements->load_power;
    bool battery_full_enough = measurements->soc_percent > g_energy_config.battery_high_soc_threshold;
    
    if (excess_power > g_energy_config.excess_power_threshold && battery_full_enough) {
        ESP_LOGI(TAG, "Excess power available: %.2f kW, Battery: %.1f%%", 
                 excess_power, measurements->soc_percent);
        return ENERGY_STATE_EXCESS_AVAILABLE;
    }
    
    return ENERGY_STATE_NORMAL;
}

static void execute_energy_control(energy_state_t state, const emma_measurements_t *measurements) {
    if (!g_energy_config.load_control_enabled) {
        ESP_LOGD(TAG, "Load control disabled - no action taken");
        return;
    }
    
    switch (state) {
        case ENERGY_STATE_EXCESS_AVAILABLE:
            ESP_LOGI(TAG, "🔋 Enabling loads due to excess power");
            // Enable water heater, heat pump, etc.
            // load_control_enable_flexible_loads();
            break;
            
        case ENERGY_STATE_BATTERY_LOW:
            ESP_LOGI(TAG, "⚠️  Reducing loads due to low battery");
            // Disable non-essential loads
            // load_control_disable_non_essential_loads();
            break;
            
        case ENERGY_STATE_GRID_LIMITING:
            ESP_LOGI(TAG, "⚡ Enabling loads to reduce grid feed-in");
            // Enable high-power loads to consume excess
            // load_control_enable_high_power_loads();
            break;
            
        case ENERGY_STATE_NORMAL:
        default:
            ESP_LOGD(TAG, "Normal operation - maintaining current load state");
            // load_control_maintain_normal_state();
            break;
    }
}

static void print_energy_summary(const emma_measurements_t *measurements, energy_state_t state) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    ENERGY MANAGEMENT STATUS                  ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Current State: ");
    
    switch (state) {
        case ENERGY_STATE_NORMAL:
            printf("🔄 NORMAL OPERATION                        ║\n");
            break;
        case ENERGY_STATE_EXCESS_AVAILABLE:
            printf("🔋 EXCESS POWER AVAILABLE                  ║\n");
            break;
        case ENERGY_STATE_BATTERY_LOW:
            printf("⚠️  BATTERY LOW - REDUCING LOADS            ║\n");
            break;
        case ENERGY_STATE_GRID_LIMITING:
            printf("⚡ GRID LIMITING ACTIVE                     ║\n");
            break;
    }
    
    if (measurements && measurements->data_valid) {
        printf("╟──────────────────────────────────────────────────────────────╢\n");
        printf("║ Key Metrics:                                                 ║\n");
        printf("║   PV Production      : %8.2f kW                         ║\n", measurements->pv_output_power);
        printf("║   House Consumption  : %8.2f kW                         ║\n", measurements->load_power);
        printf("║   Battery Power      : %8.2f kW                         ║\n", measurements->battery_charge_discharge_power);
        printf("║   Grid Feed-in       : %8.2f kW                         ║\n", measurements->feed_in_power);
        printf("║   Battery SOC        : %8.1f %%                          ║\n", measurements->soc_percent);
        printf("╟──────────────────────────────────────────────────────────────╢\n");
        
        float excess_power = measurements->pv_output_power - measurements->load_power;
        printf("║   Calculated Excess  : %8.2f kW                         ║\n", excess_power);
    }
    
    printf("║ Configuration:                                               ║\n");
    printf("║   Load Control       : %s                                ║\n", 
           g_energy_config.load_control_enabled ? "ENABLED " : "DISABLED");
    printf("║   Grid Limit         : %8.2f kW                         ║\n", g_energy_config.grid_feed_limit);
    printf("║   Excess Threshold   : %8.2f kW                         ║\n", g_energy_config.excess_power_threshold);
    printf("║   Success Rate       : %8.1f %%                          ║\n", emma_get_success_rate(&g_emma));
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

// Task for reading EMMA measurements
static void measurement_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting measurement task");
    
    while (1) {
        esp_err_t ret = emma_read_all_measurements(&g_emma);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ EMMA measurements updated successfully");
        } else {
            ESP_LOGW(TAG, "❌ Failed to read EMMA measurements");
            // Try to reconnect
            emma_disconnect(&g_emma);
        }
        
        vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_INTERVAL_MS));
    }
}

// Task for energy management control logic
static void control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting control task");
    
    energy_state_t current_state = ENERGY_STATE_NORMAL;
    energy_state_t previous_state = ENERGY_STATE_NORMAL;
    
    while (1) {
        // Analyze current energy situation
        current_state = analyze_energy_situation(&g_emma.measurements);
        
        // Execute control actions if state changed or every few cycles
        static int control_counter = 0;
        if (current_state != previous_state || (control_counter % 3 == 0)) {
            execute_energy_control(current_state, &g_emma.measurements);
            print_energy_summary(&g_emma.measurements, current_state);
        }
        
        previous_state = current_state;
        control_counter++;
        
        vTaskDelay(pdMS_TO_TICKS(CONTROL_INTERVAL_MS));
    }
}

// Configuration management functions
void energy_config_print(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    ENERGY MANAGER CONFIG                    ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Load Control Enabled     : %s                           ║\n", 
           g_energy_config.load_control_enabled ? "YES" : "NO");
    printf("║ Grid Limit Active        : %s                           ║\n", 
           g_energy_config.grid_limit_active ? "YES" : "NO");
    printf("║ Excess Power Threshold   : %8.2f kW                     ║\n", 
           g_energy_config.excess_power_threshold);
    printf("║ Battery High SOC         : %8.1f %%                      ║\n", 
           g_energy_config.battery_high_soc_threshold);
    printf("║ Battery Low SOC          : %8.1f %%                      ║\n", 
           g_energy_config.battery_low_soc_threshold);
    printf("║ Grid Feed Limit          : %8.2f kW                     ║\n", 
           g_energy_config.grid_feed_limit);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

void energy_config_set_load_control(bool enabled) {
    g_energy_config.load_control_enabled = enabled;
    ESP_LOGI(TAG, "Load control %s", enabled ? "ENABLED" : "DISABLED");
}

void energy_config_set_thresholds(float excess_kw, float high_soc, float low_soc) {
    g_energy_config.excess_power_threshold = excess_kw;
    g_energy_config.battery_high_soc_threshold = high_soc;
    g_energy_config.battery_low_soc_threshold = low_soc;
    ESP_LOGI(TAG, "Thresholds updated: Excess=%.2fkW, High SOC=%.1f%%, Low SOC=%.1f%%", 
             excess_kw, high_soc, low_soc);
}

void energy_config_set_grid_limit(float limit_kw, bool active) {
    g_energy_config.grid_feed_limit = limit_kw;
    g_energy_config.grid_limit_active = active;
    ESP_LOGI(TAG, "Grid limit: %.2f kW (%s)", limit_kw, active ? "ACTIVE" : "INACTIVE");
}

// EMMA control wrapper functions
esp_err_t emma_control_set_mode(emma_ess_control_mode_t mode) {
    ESP_LOGI(TAG, "Setting EMMA ESS mode to: %s", emma_get_ess_mode_string(mode));
    return emma_set_ess_control_mode(&g_emma, mode);
}

esp_err_t emma_control_limit_grid_power(float limit_kw) {
    ESP_LOGI(TAG, "Setting EMMA grid feed-in limit to: %.2f kW", limit_kw);
    esp_err_t ret;
    
    // First set the power control mode to limited feed-in
    ret = emma_set_power_control_mode(&g_emma, EMMA_POWER_MODE_LIMITED_FEED_IN_KW);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Then set the limit value
    ret = emma_set_max_grid_feed_in_power(&g_emma, limit_kw);
    return ret;
}

esp_err_t emma_control_unlimited_grid(void) {
    ESP_LOGI(TAG, "Setting EMMA to unlimited grid feed-in");
    return emma_set_power_control_mode(&g_emma, EMMA_POWER_MODE_UNLIMITED);
}

// System status and diagnostics
void system_print_status(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                      SYSTEM STATUS                          ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ EMMA Connection          : %s                           ║\n", 
           g_emma.modbus_client.connected ? "CONNECTED" : "DISCONNECTED");
    printf("║ EMMA IP Address          : %s                    ║\n", 
           g_emma.modbus_client.ip_address);
    printf("║ Communication Success    : %8.1f %%                      ║\n", 
           emma_get_success_rate(&g_emma));
    printf("║ Read Errors              : %8d                          ║\n", 
           (unsigned int)g_emma.read_errors);
    printf("║ Read Success             : %8d                          ║\n", 
           (unsigned int)g_emma.read_success);
    printf("║ Last Update              : %llu us ago                  ║\n", 
           esp_timer_get_time() - g_emma.measurements.timestamp_us);
    printf("║ Data Valid               : %s                           ║\n", 
           g_emma.measurements.data_valid ? "YES" : "NO");
    printf("║ Free Heap                : %d bytes                     ║\n", 
           (unsigned int)esp_get_free_heap_size());
    printf("║ Tasks Running            : %d                              ║\n", 
           uxTaskGetNumberOfTasks());
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

// Console command handler (for future CLI expansion)
void handle_console_command(const char* command) {
    if (strcmp(command, "status") == 0) {
        system_print_status();
    } else if (strcmp(command, "config") == 0) {
        energy_config_print();
    } else if (strcmp(command, "measurements") == 0) {
        emma_print_measurements(&g_emma.measurements);
    } else if (strcmp(command, "enable_loads") == 0) {
        energy_config_set_load_control(true);
    } else if (strcmp(command, "disable_loads") == 0) {
        energy_config_set_load_control(false);
    } else if (strcmp(command, "reset_stats") == 0) {
        emma_reset_statistics(&g_emma);
        ESP_LOGI(TAG, "Statistics reset");
    } else {
        ESP_LOGW(TAG, "Unknown command: %s", command);
    }
}

// Main application entry point
void app_main(void) {
    ESP_LOGI(TAG, "🏡 Starting ESP32 Energy Management System");
    ESP_LOGI(TAG, "Target EMMA: %s", EMMA_IP_ADDRESS);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi
    wifi_init_sta();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Initialize EMMA client
    ret = emma_init(&g_emma, EMMA_IP_ADDRESS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize EMMA client");
        return;
    }
    
    // Connect to EMMA
    ret = emma_connect(&g_emma);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Connected to EMMA successfully");
    } else {
        ESP_LOGW(TAG, "⚠️  Initial EMMA connection failed - will retry in measurement task");
    }
    
    // Print initial configuration
    energy_config_print();
    system_print_status();
    
    // Create measurement task
    xTaskCreate(measurement_task, "measurement_task", 4096, NULL, 5, &measurement_task_handle);
    if (measurement_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create measurement task");
        return;
    }
    
    // Create control task
    xTaskCreate(control_task, "control_task", 4096, NULL, 4, &control_task_handle);
    if (control_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create control task");
        return;
    }
    
    ESP_LOGI(TAG, "🚀 Energy Management System started successfully");
    ESP_LOGI(TAG, "📊 Measurement interval: %d ms", MEASUREMENT_INTERVAL_MS);
    ESP_LOGI(TAG, "⚡ Control interval: %d ms", CONTROL_INTERVAL_MS);
    
    // Main loop - could be used for console commands or web interface
    while (1) {
        // Every 60 seconds, print a status summary
        static int status_counter = 0;
        if (status_counter % (60000 / 1000) == 0) {
            system_print_status();
            energy_config_print();
        }
        status_counter++;
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}