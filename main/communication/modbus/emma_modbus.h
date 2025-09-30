#ifndef EMMA_MODBUS_H
#define EMMA_MODBUS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "modbus_tcp.h"

// EMMA configuration
#define EMMA_DEFAULT_IP "192.168.64.101"
#define EMMA_UNIT_ID    0

// EMMA register definitions with gain factors
typedef enum {
    EMMA_REG_ENERGY_CHARGED_TODAY = 30306,
    EMMA_REG_ENERGY_DISCHARGED_TODAY = 30312,
    EMMA_REG_CONSUMPTION_TODAY = 30324,
    EMMA_REG_FEED_IN_GRID_TODAY = 30330,
    EMMA_REG_SUPPLY_FROM_GRID_TODAY = 30336,
    EMMA_REG_INVERTER_YIELD_TODAY = 30342,
    EMMA_REG_PV_YIELD_TODAY = 30346,
    EMMA_REG_PV_OUTPUT_POWER = 30354,
    EMMA_REG_LOAD_POWER = 30356,
    EMMA_REG_FEED_IN_POWER = 30358,
    EMMA_REG_BATTERY_CHARGE_DISCHARGE_POWER = 30360,
    EMMA_REG_INVERTER_RATED_POWER = 30362,
    EMMA_REG_INVERTER_ACTIVE_POWER = 30364,
    EMMA_REG_SOC = 30368,
    EMMA_REG_ESS_CHARGEABLE_CAPACITY = 30369,
    EMMA_REG_ESS_DISCHARGEABLE_CAPACITY = 30371,
    EMMA_REG_BACKUP_POWER_SOC = 30373,
    EMMA_REG_PHASE_A_VOLTAGE = 31639,
    EMMA_REG_PHASE_B_VOLTAGE = 31641,
    EMMA_REG_PHASE_C_VOLTAGE = 31643,
    EMMA_REG_PHASE_A_CURRENT = 31651,
    EMMA_REG_PHASE_B_CURRENT = 31653,
    EMMA_REG_PHASE_C_CURRENT = 31655,
    EMMA_REG_ACTIVE_POWER = 31657,
    EMMA_REG_POWER_FACTOR = 31661,
    // Control registers
    EMMA_REG_ESS_CONTROL_MODE = 40000,
    EMMA_REG_POWER_CONTROL_MODE = 40100,
    EMMA_REG_MAX_GRID_FEED_IN_POWER = 40107,
} emma_register_t;

typedef enum {
    EMMA_ESS_MODE_MAXIMUM_SELF_CONSUMPTION = 2,
    EMMA_ESS_MODE_FULLY_FED_TO_GRID = 4,
    EMMA_ESS_MODE_TIME_OF_USE = 5,
    EMMA_ESS_MODE_THIRD_PARTY_DISPATCH = 6,
} emma_ess_control_mode_t;

typedef enum {
    EMMA_POWER_MODE_UNLIMITED = 0,
    EMMA_POWER_MODE_GRID_CONNECTED_ZERO = 5,
    EMMA_POWER_MODE_LIMITED_FEED_IN_KW = 6,
    EMMA_POWER_MODE_LIMITED_FEED_IN_PERCENT = 7,
} emma_power_control_mode_t;

// EMMA measurement structure
typedef struct {
    // Energy measurements (kWh)
    float energy_charged_today;
    float energy_discharged_today;
    float consumption_today;
    float feed_in_grid_today;
    float supply_from_grid_today;
    float inverter_yield_today;
    float pv_yield_today;
    
    // Power measurements (kW)
    float pv_output_power;
    float load_power;
    float feed_in_power;
    float battery_charge_discharge_power;
    float inverter_rated_power;
    float inverter_active_power;
    
    // Battery status
    float soc_percent;
    float ess_chargeable_capacity;
    float ess_dischargeable_capacity;
    float backup_power_soc;
    
    // Grid measurements
    float phase_a_voltage;
    float phase_b_voltage;
    float phase_c_voltage;
    float phase_a_current;
    float phase_b_current;
    float phase_c_current;
    float active_power;
    float power_factor;
    
    // Timestamp and validity
    uint64_t timestamp_us;
    bool data_valid;
} emma_measurements_t;

// EMMA client structure
typedef struct {
    modbus_tcp_client_t modbus_client;
    emma_measurements_t measurements;
    bool initialized;
    uint32_t read_errors;
    uint32_t read_success;
} emma_client_t;

// Function prototypes
esp_err_t emma_init(emma_client_t *emma, const char *ip_address);
esp_err_t emma_connect(emma_client_t *emma);
void emma_disconnect(emma_client_t *emma);
esp_err_t emma_read_all_measurements(emma_client_t *emma);
esp_err_t emma_read_single_measurement(emma_client_t *emma, emma_register_t reg, float *value);

// Control functions
esp_err_t emma_set_ess_control_mode(emma_client_t *emma, emma_ess_control_mode_t mode);
esp_err_t emma_set_power_control_mode(emma_client_t *emma, emma_power_control_mode_t mode);
esp_err_t emma_set_max_grid_feed_in_power(emma_client_t *emma, float power_kw);

// Utility functions
void emma_print_measurements(const emma_measurements_t *measurements);
const char* emma_get_ess_mode_string(emma_ess_control_mode_t mode);
const char* emma_get_power_mode_string(emma_power_control_mode_t mode);

// Statistics
float emma_get_success_rate(const emma_client_t *emma);
void emma_reset_statistics(emma_client_t *emma);

#endif // EMMA_MODBUS_H