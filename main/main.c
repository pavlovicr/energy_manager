/*
 * ESP32 Energy Management System s Box3 displayom
 * Author: ESP32 Developer  
 * Date: 2025
 *
 * Description:
 * Smart energy management sistem ki bere podatke iz Huawei EMMA
 * in jih prikazuje na Box3 displayu namesto v konzoli
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

// Custom moduli
#include "emma_modbus.h"
#include "wifi_manager.h"
#include "load_control.h"
#include "display_manager.h"  // Dodano za Box3 display

#define TAG "ENERGY_MANAGER"

// Configuration
#define EMMA_IP_ADDRESS "192.168.64.101"
#define MEASUREMENT_INTERVAL_MS 15000
#define CONTROL_INTERVAL_MS 5000 // prej 5000
#define DISPLAY_UPDATE_INTERVAL_MS 2000  // Display se posodobi prej 2000

// Global variables
static emma_client_t g_emma; //struct emma_modbus.h
static TaskHandle_t measurement_task_handle = NULL;
static TaskHandle_t control_task_handle = NULL;
static TaskHandle_t display_task_handle = NULL;  // Dodano za display task

// WiFi configuration
#define WIFI_SSID      "ONEfourTWO"
#define WIFI_PASSWORD  "markoskacepozelenitrati"
#define MAXIMUM_RETRY  5

static EventGroupHandle_t s_wifi_event_group;//za spremljanje stanja povezave
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Energy management parameters - Pragovi za odločanje, kdaj vklopiti/izklopiti obremenitve
typedef struct {
    float excess_power_threshold_prag_moci;
    float battery_high_soc_threshold_zgornji_prag_napolnjenosti_baterije;
    float battery_low_soc_threshold_spodnji_prag_napolnjenosti_baterije;
    float grid_feed_limit_meja_za_prodajo;
    bool load_control_enabled;
    bool grid_limit_active;
} energy_config_t;

static energy_config_t g_energy_config = {
    .excess_power_threshold_prag_moci = 1.0f,
    .battery_high_soc_threshold_zgornji_prag_napolnjenosti_baterije = 90.0f,
    .battery_low_soc_threshold_spodnji_prag_napolnjenosti_baterije = 20.0f,
    .grid_feed_limit_meja_za_prodajo = 8.0f,
    .load_control_enabled = true,
    .grid_limit_active = true,
};
//777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777

/* WiFi Event Handler - 
funkcija, ki se kliče ob wifi dogodkih : 
    - Ob zagonu WiFi-ja začne povezovati
    - Ob prekinitvi povezave poskuša ponovno povezovati
    - Ko dobi IP naslov, zabeleži uspešno povezavo */

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
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip)); //MAKRO #define IPSTR "%d.%d.%d.%d" ,    ip_info.ip je instanca structa, ki vsebuje IP, mask in gateway
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
//inicializacija wifi
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
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Energy Management Logic - Logika energy managementa
typedef enum {
    ENERGY_STATE_NORMAL,
    ENERGY_STATE_EXCESS_AVAILABLE_NA_VOLJO_PRESEZEK_ENERGIJE,
    ENERGY_STATE_BATTERY_LOW,
    ENERGY_STATE_GRID_LIMITING,
} energy_state_t;

static energy_state_t analyze_energy_situation(const emma_measurements_t *measurements) {
    if (!measurements || !measurements->data_valid) {
        return ENERGY_STATE_NORMAL;
    }
    
    if (measurements->soc_percent < g_energy_config.battery_low_soc_threshold_spodnji_prag_napolnjenosti_baterije) {
        ESP_LOGW(TAG, "Battery low: %.1f%% - reducing loads", measurements->soc_percent);
        return ENERGY_STATE_BATTERY_LOW;
    }
    
    if (g_energy_config.grid_limit_active && 
        measurements->feed_in_power > g_energy_config.grid_feed_limit_meja_za_prodajo) {
        ESP_LOGW(TAG, "Grid feed-in too high: %.2f kW - enabling loads", 
                 measurements->feed_in_power);
        return ENERGY_STATE_GRID_LIMITING;
    }
    
    float excess_power = measurements->pv_output_power - measurements->load_power;
    bool battery_full_enough = measurements->soc_percent > g_energy_config.battery_high_soc_threshold_zgornji_prag_napolnjenosti_baterije;
    
    if (excess_power > g_energy_config.excess_power_threshold_prag_moci && battery_full_enough) {
        ESP_LOGI(TAG, "Excess power available: %.2f kW, Battery: %.1f%%", 
                 excess_power, measurements->soc_percent);
        return ENERGY_STATE_EXCESS_AVAILABLE_NA_VOLJO_PRESEZEK_ENERGIJE;
    }
    
    return ENERGY_STATE_NORMAL;
}

static void execute_energy_control(energy_state_t state, const emma_measurements_t *measurements) {
    if (!g_energy_config.load_control_enabled) {
        ESP_LOGD(TAG, "Load control disabled - no action taken");
        return;
    }
    
    switch (state) {
        case ENERGY_STATE_EXCESS_AVAILABLE_NA_VOLJO_PRESEZEK_ENERGIJE:
            ESP_LOGI(TAG, "Enabling loads due to excess power");
            break;
        case ENERGY_STATE_BATTERY_LOW:
            ESP_LOGI(TAG, "Reducing loads due to low battery");
            break;
        case ENERGY_STATE_GRID_LIMITING:
            ESP_LOGI(TAG, "Enabling loads to reduce grid feed-in");
            break;
        case ENERGY_STATE_NORMAL:
        default:
            ESP_LOGD(TAG, "Normal operation - maintaining current load state");
            break;
    }
}

static const char* get_energy_state_string(energy_state_t state) {
    switch (state) {
        case ENERGY_STATE_NORMAL: return "NORMALNO";
        case ENERGY_STATE_EXCESS_AVAILABLE_NA_VOLJO_PRESEZEK_ENERGIJE: return "PRESEZEK ENERGIJE";
        case ENERGY_STATE_BATTERY_LOW: return "NIZKA BATERIJA";
        case ENERGY_STATE_GRID_LIMITING: return "OMEJITEV OMREZJA";
        default: return "NEZNAN";
    }
}

// Task za branje EMMA meritev
static void measurement_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting measurement task");
    
    while (1) {
        esp_err_t ret = emma_read_all_measurements(&g_emma);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "EMMA measurements updated successfully");
        } else {
            ESP_LOGW(TAG, "Failed to read EMMA measurements");
            emma_disconnect(&g_emma);
        }
        
        vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_INTERVAL_MS));
    }
}

// Task za energijski management control
static void control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting control task");
    
    energy_state_t current_state = ENERGY_STATE_NORMAL;
    
    while (1) {
        current_state = analyze_energy_situation(&g_emma.measurements);
        execute_energy_control(current_state, &g_emma.measurements);
        
        vTaskDelay(pdMS_TO_TICKS(CONTROL_INTERVAL_MS));
    }
}

// Novi task za posodabljanje Box3 displaya
static void display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting Box3 display task");
    
    energy_state_t last_state = ENERGY_STATE_NORMAL;
    bool last_emma_connected = false;
    bool wifi_connected = true;
    
    while (1) {
        // Preveri EMMA povezavo
        bool emma_connected = g_emma.modbus_client.connected && g_emma.measurements.data_valid;
        float success_rate = emma_get_success_rate(&g_emma);
        
        // Posodobi EMMA meritve na displayu
        if (emma_connected) {
            display_update_emma_measurements(&g_emma.measurements);
        }
        
        // Posodobi status povezave (samo ce se je spremenil)
        if (emma_connected != last_emma_connected || 
            (int)success_rate % 10 == 0) { // vsakih 10% spremembe
            display_update_connection_status(emma_connected, wifi_connected, success_rate);
            last_emma_connected = emma_connected;
        }
        
        // Posodobi sistem status
        energy_state_t current_state = analyze_energy_situation(&g_emma.measurements);
        if (current_state != last_state) {
            display_update_system_status(get_energy_state_string(current_state));
            last_state = current_state;
        }
        
        // Preveri za napake
        if (!emma_connected && success_rate < 10.0f) {
            display_show_error("EMMA CONNECTION LOST");
        }
        
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_INTERVAL_MS));
    }
}

// Configuration management functions (ostane enako)
void energy_config_print(void) {
    printf("\n=== ENERGY MANAGER CONFIG ===\n");
    printf("Load Control Enabled     : %s\n", 
           g_energy_config.load_control_enabled ? "YES" : "NO");
    printf("Grid Limit Active        : %s\n", 
           g_energy_config.grid_limit_active ? "YES" : "NO");
    printf("Excess Power Threshold   : %.2f kW\n", 
           g_energy_config.excess_power_threshold_prag_moci);
    printf("Battery High SOC         : %.1f%%\n", 
           g_energy_config.battery_high_soc_threshold_zgornji_prag_napolnjenosti_baterije);
    printf("Battery Low SOC          : %.1f%%\n", 
           g_energy_config.battery_low_soc_threshold_spodnji_prag_napolnjenosti_baterije);
    printf("Grid Feed Limit          : %.2f kW\n", 
           g_energy_config.grid_feed_limit_meja_za_prodajo);
    printf("===============================\n\n");
}

void energy_config_set_load_control(bool enabled) {
    g_energy_config.load_control_enabled = enabled;
    ESP_LOGI(TAG, "Load control %s", enabled ? "ENABLED" : "DISABLED");
}

// System status and diagnostics
void system_print_status(void) {
    printf("\n=== SYSTEM STATUS ===\n");
    printf("EMMA Connection          : %s\n", 
           g_emma.modbus_client.connected ? "CONNECTED" : "DISCONNECTED");
    printf("EMMA IP Address          : %s\n", 
           g_emma.modbus_client.ip_address);
    printf("Communication Success    : %.1f%%\n", 
           emma_get_success_rate(&g_emma));
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

// Main application entry point
void app_main(void) {
    ESP_LOGI(TAG, "Starting ESP32 Energy Management System with Box3 Display");
    ESP_LOGI(TAG, "Target EMMA: %s", EMMA_IP_ADDRESS);
   
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize Box3 Display NAJPREJ
    ESP_LOGI(TAG, "Initializing Box3 Display...");
    display_init();
    display_create_emma_screen();
    display_update_system_status("INICIALIZACIJA");
    
    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init_sta();
    display_update_connection_status(false, true, 0.0f);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Initialize EMMA client
    ESP_LOGI(TAG, "Initializing EMMA client...");
    ret = emma_init(&g_emma, EMMA_IP_ADDRESS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize EMMA client");
        display_show_error("EMMA INIT FAILED");
        return;
    }
    
    // Connect to EMMA
    ret = emma_connect(&g_emma);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Connected to EMMA successfully");
        display_update_connection_status(true, true, 100.0f);
        display_update_system_status("POVEZANO");
    } else {
        ESP_LOGW(TAG, "Initial EMMA connection failed - will retry in measurement task");
        display_update_connection_status(false, true, 0.0f);
        display_update_system_status("CONNECTING");
    }
    
    // Print initial configuration (samo v konzolo)
    energy_config_print();
    system_print_status();
    
    // Create measurement task
    xTaskCreate(measurement_task, "measurement_task", 4096, NULL, 5, &measurement_task_handle);
    if (measurement_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create measurement task");
        display_show_error("TASK CREATE FAILED");
        return;
    }
    
    // Create control task
    xTaskCreate(control_task, "control_task", 4096, NULL, 4, &control_task_handle);
    if (control_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create control task");
        display_show_error("CONTROL TASK FAILED");
        return;
    }
    
    // Create display task
    xTaskCreate(display_task, "display_task", 4096, NULL, 3, &display_task_handle);
    if (display_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create display task");
        display_show_error("DISPLAY TASK FAILED");
        return;
    }
    
    ESP_LOGI(TAG, "Energy Management System with Box3 started successfully");
    ESP_LOGI(TAG, "Measurement interval: %d ms", MEASUREMENT_INTERVAL_MS);
    ESP_LOGI(TAG, "Control interval: %d ms", CONTROL_INTERVAL_MS);
    ESP_LOGI(TAG, "Display update interval: %d ms", DISPLAY_UPDATE_INTERVAL_MS);
    
    display_update_system_status("SISTEM AKTIVEN");

   
    // Main loop - reduciran, ker Box3 prikazuje vse informacije
    while (1) {
        // Samo periodicen izpis v konzolo za debug
        static int status_counter = 0;
        if (status_counter % 120 == 0) { // vsakih 2 minuti
            system_print_status();
            ESP_LOGI(TAG, "System running normally, check Box3 display for details");
        }
        status_counter++;
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }


    
}