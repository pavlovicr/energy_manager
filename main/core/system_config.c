#include "system_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char* TAG = "SYS_CONFIG";
static const char* NVS_NAMESPACE = "energy_config";

// Privzeta konfiguracija
static system_config_t g_config = {
    .wifi_ssid = "ONEfourTWO",
    .wifi_password = "markoskacepozelenitrati",
    .emma_ip = "192.168.64.101",
    .measurement_interval_ms = 15000,
    .control_interval_ms = 5000,
    .display_update_interval_ms = 2000,
    .log_level = ESP_LOG_INFO,
    .enable_load_control = true,
    .enable_display = true,
    .watchdog_timeout_ms = 10000
};

static bool g_initialized = false;

esp_err_t system_config_init(void)
{
    ESP_LOGI(TAG, "Initializing system configuration");
    
    // Poskusi naložiti konfiguracijo iz NVS
    esp_err_t ret = system_config_load_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config from NVS, using defaults");
    }
    
    // Nastavi log level
    esp_log_level_set("*", g_config.log_level);
    
    g_initialized = true;
    
    ESP_LOGI(TAG, "System configuration initialized");
    ESP_LOGI(TAG, "WiFi SSID: %s", g_config.wifi_ssid);
    ESP_LOGI(TAG, "EMMA IP: %s", g_config.emma_ip);
    ESP_LOGI(TAG, "Measurement interval: %lu ms", g_config.measurement_interval_ms);
    
    return ESP_OK;
}

const system_config_t* system_config_get(void)
{
    if (!g_initialized) {
        return NULL;
    }
    return &g_config;
}

esp_err_t system_config_set_wifi(const char* ssid, const char* password)
{
    if (!g_initialized || !ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(g_config.wifi_ssid, ssid, sizeof(g_config.wifi_ssid) - 1);
    strncpy(g_config.wifi_password, password, sizeof(g_config.wifi_password) - 1);
    
    ESP_LOGI(TAG, "WiFi configuration updated");
    return system_config_save_to_nvs();
}

esp_err_t system_config_set_emma_ip(const char* ip)
{
    if (!g_initialized || !ip) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(g_config.emma_ip, ip, sizeof(g_config.emma_ip) - 1);
    
    ESP_LOGI(TAG, "EMMA IP updated: %s", ip);
    return system_config_save_to_nvs();
}

esp_err_t system_config_set_intervals(uint32_t measurement_ms, uint32_t control_ms, uint32_t display_ms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (measurement_ms < 1000 || measurement_ms > 60000) {
        ESP_LOGE(TAG, "Invalid measurement interval: %lu ms", measurement_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (control_ms < 1000 || control_ms > 30000) {
        ESP_LOGE(TAG, "Invalid control interval: %lu ms", control_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (display_ms < 500 || display_ms > 10000) {
        ESP_LOGE(TAG, "Invalid display interval: %lu ms", display_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    g_config.measurement_interval_ms = measurement_ms;
    g_config.control_interval_ms = control_ms;
    g_config.display_update_interval_ms = display_ms;
    
    ESP_LOGI(TAG, "Intervals updated: Measurement=%lums, Control=%lums, Display=%lums",
             measurement_ms, control_ms, display_ms);
    
    return system_config_save_to_nvs();
}

esp_err_t system_config_set_log_level(esp_log_level_t level)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    g_config.log_level = level;
    esp_log_level_set("*", level);
    
    ESP_LOGI(TAG, "Log level updated: %d", level);
    return system_config_save_to_nvs();
}

esp_err_t system_config_set_features(bool load_control, bool display)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    g_config.enable_load_control = load_control;
    g_config.enable_display = display;
    
    ESP_LOGI(TAG, "Features updated: Load control=%s, Display=%s",
             load_control ? "ON" : "OFF", display ? "ON" : "OFF");
    
    return system_config_save_to_nvs();
}

esp_err_t system_config_save_to_nvs(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Shrani konfiguracijo
    ret = nvs_set_blob(nvs_handle, "config", &g_config, sizeof(system_config_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save configuration: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t system_config_load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "NVS namespace not found, will use defaults");
        return ret;
    }
    
    size_t required_size = sizeof(system_config_t);
    ret = nvs_get_blob(nvs_handle, "config", &g_config, &required_size);
    
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration loaded from NVS");
    } else {
        ESP_LOGD(TAG, "Failed to load configuration from NVS: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t system_config_reset_to_defaults(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Reset na privzete vrednosti
    strncpy(g_config.wifi_ssid, "ONEfourTWO", sizeof(g_config.wifi_ssid));
    strncpy(g_config.wifi_password, "markoskacepozelenitrati", sizeof(g_config.wifi_password));
    strncpy(g_config.emma_ip, "192.168.64.101", sizeof(g_config.emma_ip));
    
    g_config.measurement_interval_ms = 15000;
    g_config.control_interval_ms = 5000;
    g_config.display_update_interval_ms = 2000;
    g_config.log_level = ESP_LOG_INFO;
    g_config.enable_load_control = true;
    g_config.enable_display = true;
    g_config.watchdog_timeout_ms = 10000;
    
    ESP_LOGI(TAG, "Configuration reset to defaults");
    return system_config_save_to_nvs();
}

void system_config_print(void)
{
    if (!g_initialized) {
        ESP_LOGW(TAG, "Configuration not initialized");
        return;
    }
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    SYSTEM CONFIGURATION                     ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ WiFi SSID            : %-32s ║\n", g_config.wifi_ssid);
    printf("║ EMMA IP              : %-32s ║\n", g_config.emma_ip);
    printf("║ Measurement Interval : %-8lu ms                        ║\n", g_config.measurement_interval_ms);
    printf("║ Control Interval     : %-8lu ms                        ║\n", g_config.control_interval_ms);
    printf("║ Display Interval     : %-8lu ms                        ║\n", g_config.display_update_interval_ms);
    printf("║ Log Level            : %-8d                            ║\n", g_config.log_level);
    printf("║ Load Control         : %-8s                            ║\n", g_config.enable_load_control ? "ENABLED" : "DISABLED");
    printf("║ Display              : %-8s                            ║\n", g_config.enable_display ? "ENABLED" : "DISABLED");
    printf("║ Watchdog Timeout     : %-8lu ms                        ║\n", g_config.watchdog_timeout_ms);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}