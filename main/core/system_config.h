#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdbool.h>

// Sistem konfiguracija
typedef struct {
    char wifi_ssid[32];
    char wifi_password[64];
    char emma_ip[16];
    uint32_t measurement_interval_ms;
    uint32_t control_interval_ms;
    uint32_t display_update_interval_ms;
    esp_log_level_t log_level;
    bool enable_load_control;
    bool enable_display;
    uint32_t watchdog_timeout_ms;
} system_config_t;

// API funkcije
esp_err_t system_config_init(void);
const system_config_t* system_config_get(void);

// Setterji
esp_err_t system_config_set_wifi(const char* ssid, const char* password);
esp_err_t system_config_set_emma_ip(const char* ip);
esp_err_t system_config_set_intervals(uint32_t measurement_ms, uint32_t control_ms, uint32_t display_ms);
esp_err_t system_config_set_log_level(esp_log_level_t level);
esp_err_t system_config_set_features(bool load_control, bool display);

// Upravljanje konfiguracije
esp_err_t system_config_save_to_nvs(void);
esp_err_t system_config_load_from_nvs(void);
esp_err_t system_config_reset_to_defaults(void);
void system_config_print(void);

#endif // SYSTEM_CONFIG_H