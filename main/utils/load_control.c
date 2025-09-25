#include "wifi_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

static const char* TAG = "WIFI_MANAGER";

// Global variables
static EventGroupHandle_t s_wifi_event_group;
static wifi_info_t s_wifi_info;
static int s_retry_num = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Event handler
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        s_wifi_info.status = WIFI_STATUS_CONNECTING;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            s_wifi_info.retry_count = s_retry_num;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_wifi_info.status = WIFI_STATUS_FAILED;
        }
        s_wifi_info.disconnect_count++;
        ESP_LOGI(TAG, "Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_wifi_info.retry_count = 0;
        s_wifi_info.status = WIFI_STATUS_CONNECTED;
        s_wifi_info.ip_address = event->ip_info.ip.addr;
        s_wifi_info.connect_time = esp_timer_get_time() / 1000000; // Convert to seconds
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(void) {
    s_wifi_event_group = xEventGroupCreate();
    
    // Initialize wifi_info structure
    memset(&s_wifi_info, 0, sizeof(wifi_info_t));
    s_wifi_info.status = WIFI_STATUS_DISCONNECTED;
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

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

    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char* ssid, const char* password) {
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    // Store credentials
    strncpy(s_wifi_info.ssid, ssid, sizeof(s_wifi_info.ssid) - 1);
    strncpy(s_wifi_info.password, password, sizeof(s_wifi_info.password) - 1);
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_wifi_info.status = WIFI_STATUS_CONNECTING;
    ESP_LOGI(TAG, "WiFi initialization finished. Connecting to %s", ssid);

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to ap SSID:%s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
}

esp_err_t wifi_manager_disconnect(void) {
    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        s_wifi_info.status = WIFI_STATUS_DISCONNECTED;
        ESP_LOGI(TAG, "WiFi disconnected");
    }
    return ret;
}

esp_err_t wifi_manager_reconnect(void) {
    if (strlen(s_wifi_info.ssid) == 0) {
        ESP_LOGE(TAG, "No stored credentials for reconnection");
        return ESP_ERR_INVALID_STATE;
    }
    
    s_retry_num = 0;
    return wifi_manager_connect(s_wifi_info.ssid, s_wifi_info.password);
}

wifi_status_t wifi_manager_get_status(void) {
    return s_wifi_info.status;
}

wifi_info_t wifi_manager_get_info(void) {
    // Update RSSI if connected
    if (s_wifi_info.status == WIFI_STATUS_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_wifi_info.rssi = ap_info.rssi;
        }
    }
    return s_wifi_info;
}

bool wifi_manager_is_connected(void) {
    return (s_wifi_info.status == WIFI_STATUS_CONNECTED);
}

esp_err_t wifi_manager_save_config(const char* ssid, const char* password) {
    // Simple implementation - just store in memory
    // In real implementation, you would save to NVS
    strncpy(s_wifi_info.ssid, ssid, sizeof(s_wifi_info.ssid) - 1);
    strncpy(s_wifi_info.password, password, sizeof(s_wifi_info.password) - 1);
    ESP_LOGI(TAG, "WiFi config saved");
    return ESP_OK;
}

esp_err_t wifi_manager_load_config(void) {
    // Simple implementation - credentials already in memory
    // In real implementation, you would load from NVS
    ESP_LOGI(TAG, "WiFi config loaded");
    return ESP_OK;
}

void wifi_manager_print_status(void) {
    wifi_info_t info = wifi_manager_get_info();
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                       WIFI STATUS                           ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ SSID                 : %-32s ║\n", info.ssid);
    printf("║ Status               : %-32s ║\n", wifi_manager_get_status_string(info.status));
    printf("║ IP Address           : " IPSTR "                        ║\n", IP2STR(&info.ip_address));
    printf("║ RSSI                 : %d dBm                            ║\n", info.rssi);
    printf("║ Retry Count          : %u                                ║\n", (unsigned int)info.retry_count);
    printf("║ Disconnect Count     : %u                                ║\n", (unsigned int)info.disconnect_count);
    printf("║ Connect Time         : %u s                              ║\n", (unsigned int)info.connect_time);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

const char* wifi_manager_get_status_string(wifi_status_t status) {
    switch (status) {
        case WIFI_STATUS_DISCONNECTED: return "DISCONNECTED";
        case WIFI_STATUS_CONNECTING: return "CONNECTING";
        case WIFI_STATUS_CONNECTED: return "CONNECTED";
        case WIFI_STATUS_FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}