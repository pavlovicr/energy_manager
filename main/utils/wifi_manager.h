#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"

// WiFi configuration
#define WIFI_MAXIMUM_RETRY 5
#define WIFI_CONNECT_TIMEOUT_MS 30000

// WiFi status
typedef enum {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED,
} wifi_status_t;

typedef struct {
    char ssid[33];
    char password[65];
    wifi_status_t status;
    int8_t rssi;
    uint32_t ip_address;
    uint32_t retry_count;
    uint32_t connect_time;
    uint32_t disconnect_count;
} wifi_info_t;

// Function prototypes
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(const char* ssid, const char* password);
esp_err_t wifi_manager_disconnect(void);
esp_err_t wifi_manager_reconnect(void);

// Status functions
wifi_status_t wifi_manager_get_status(void);
wifi_info_t wifi_manager_get_info(void);
bool wifi_manager_is_connected(void);

// Configuration
esp_err_t wifi_manager_save_config(const char* ssid, const char* password);
esp_err_t wifi_manager_load_config(void);

// Utilities
void wifi_manager_print_status(void);
const char* wifi_manager_get_status_string(wifi_status_t status);

#endif // WIFI_MANAGER_H