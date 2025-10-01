#ifndef ENERGY_MANAGER_H
#define ENERGY_MANAGER_H

#include "esp_err.h"
#include "communication/modbus/emma_modbus.h"
#include <stdint.h>
#include <stdbool.h>

// 1. NAJPREJ definicije tipov (enum)
typedef enum {
    ENERGY_STATE_INITIALIZING,
    ENERGY_STATE_NO_DATA,
    ENERGY_STATE_NORMAL,
    ENERGY_STATE_EXCESS_AVAILABLE,
    ENERGY_STATE_BATTERY_LOW,
    ENERGY_STATE_GRID_LIMITING
} energy_state_t;

// 2. POTEM strukture
typedef struct {
    float voltage_l1;
    float voltage_l2;
    float voltage_l3;
    float current_l1;
    float current_l2;
    float current_l3;
    float power_active;
    float power_reactive;
    float power_apparent;
    float power_factor;
    float frequency;
    float energy_total;
    energy_state_t state;
    uint32_t timestamp;
} energy_data_t;

typedef struct {
    float excess_power_threshold_kw;
    float battery_high_soc_threshold;
    float battery_low_soc_threshold;
    float grid_feed_limit_kw;
    bool load_control_enabled;
    bool grid_limit_active;
} energy_config_t;

typedef struct {
    uint32_t successful_reads;
    uint32_t failed_reads;
    uint32_t state_changes;
    uint64_t total_runtime_us;
} energy_statistics_t;

typedef struct {
    energy_state_t current_state;
    emma_measurements_t latest_data;
    energy_config_t config;
    energy_statistics_t stats;
    bool emma_connected;
    bool data_valid;
    uint64_t last_update_time;
} energy_manager_t;

// 3. NAZADNJE funkcijske deklaracije
esp_err_t energy_manager_init(void);
esp_err_t energy_manager_connect_emma(void);
esp_err_t energy_manager_update_measurements(void);
energy_state_t energy_manager_analyze_and_control(void);
energy_state_t energy_manager_get_current_state(void);  // <-- Dodaj tukaj

const emma_measurements_t* energy_manager_get_latest_data(void);
energy_statistics_t energy_manager_get_statistics(void);
const char* energy_manager_get_status_string(void);
float energy_manager_get_success_rate(void);
bool energy_manager_is_emma_connected(void);

#endif // ENERGY_MANAGER_H