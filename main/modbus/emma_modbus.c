#include "emma_modbus.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "EMMA_MODBUS";

// Register configuration table
typedef struct {
    emma_register_t reg;
    uint16_t quantity;
    float gain;
    const char* name;
    const char* unit;
} emma_reg_config_t;

static const emma_reg_config_t reg_configs[] = {
    {EMMA_REG_ENERGY_CHARGED_TODAY, 2, 100.0f, "Energy_Charged_Today", "kWh"},
    {EMMA_REG_ENERGY_DISCHARGED_TODAY, 2, 100.0f, "Energy_Discharged_Today", "kWh"},
    {EMMA_REG_CONSUMPTION_TODAY, 2, 100.0f, "Consumption_Today", "kWh"},
    {EMMA_REG_FEED_IN_GRID_TODAY, 2, 100.0f, "Feed_In_Grid_Today", "kWh"},
    {EMMA_REG_SUPPLY_FROM_GRID_TODAY, 2, 100.0f, "Supply_From_Grid_Today", "kWh"},
    {EMMA_REG_INVERTER_YIELD_TODAY, 2, 100.0f, "Inverter_Yield_Today", "kWh"},
    {EMMA_REG_PV_YIELD_TODAY, 2, 100.0f, "PV_Yield_Today", "kWh"},
    {EMMA_REG_PV_OUTPUT_POWER, 2, 1000.0f, "PV_Output_Power", "kW"},
    {EMMA_REG_LOAD_POWER, 2, 1000.0f, "Load_Power", "kW"},
    {EMMA_REG_FEED_IN_POWER, 2, 1000.0f, "Feed_In_Power", "kW"},
    {EMMA_REG_BATTERY_CHARGE_DISCHARGE_POWER, 2, 1000.0f, "Battery_Charge_Discharge_Power", "kW"},
    {EMMA_REG_INVERTER_RATED_POWER, 2, 1000.0f, "Inverter_Rated_Power", "kW"},
    {EMMA_REG_INVERTER_ACTIVE_POWER, 2, 1000.0f, "Inverter_Active_Power", "kW"},
    {EMMA_REG_SOC, 1, 100.0f, "SOC", "%"},
    {EMMA_REG_ESS_CHARGEABLE_CAPACITY, 2, 1000.0f, "ESS_Chargeable_Capacity", "kWh"},
    {EMMA_REG_ESS_DISCHARGEABLE_CAPACITY, 2, 1000.0f, "ESS_Dischargeable_Capacity", "kWh"},
    {EMMA_REG_BACKUP_POWER_SOC, 1, 100.0f, "Backup_Power_SOC", "%"},
    {EMMA_REG_PHASE_A_VOLTAGE, 2, 100.0f, "Phase_A_Voltage", "V"},
    {EMMA_REG_PHASE_B_VOLTAGE, 2, 100.0f, "Phase_B_Voltage", "V"},
    {EMMA_REG_PHASE_C_VOLTAGE, 2, 100.0f, "Phase_C_Voltage", "V"},
    {EMMA_REG_PHASE_A_CURRENT, 2, 10.0f, "Phase_A_Current", "A"},
    {EMMA_REG_PHASE_B_CURRENT, 2, 10.0f, "Phase_B_Current", "A"},
    {EMMA_REG_PHASE_C_CURRENT, 2, 10.0f, "Phase_C_Current", "A"},
    {EMMA_REG_ACTIVE_POWER, 2, 1000.0f, "Active_Power", "kW"},
    {EMMA_REG_POWER_FACTOR, 2, 1000.0f, "Power_Factor", ""},
};

static const emma_reg_config_t* get_reg_config(emma_register_t reg) {
    for (int i = 0; i < sizeof(reg_configs) / sizeof(reg_configs[0]); i++) {
        if (reg_configs[i].reg == reg) {
            return &reg_configs[i];
        }
    }
    return NULL;
}

esp_err_t emma_init(emma_client_t *emma, const char *ip_address) {
    if (!emma) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = modbus_tcp_init(&emma->modbus_client, 
                                   ip_address ? ip_address : EMMA_DEFAULT_IP, 
                                   EMMA_UNIT_ID);
    if (ret != ESP_OK) {
        return ret;
    }
    
    memset(&emma->measurements, 0, sizeof(emma_measurements_t));
    emma->initialized = true;
    emma->read_errors = 0;
    emma->read_success = 0;
    
    ESP_LOGI(TAG, "EMMA client initialized for %s", 
             ip_address ? ip_address : EMMA_DEFAULT_IP);
    return ESP_OK;
}

esp_err_t emma_connect(emma_client_t *emma) {
    if (!emma || !emma->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return modbus_tcp_connect(&emma->modbus_client);
}

void emma_disconnect(emma_client_t *emma) {
    if (emma) {
        modbus_tcp_disconnect(&emma->modbus_client);
    }
}

static float convert_register_value(const uint16_t *raw_data, uint16_t quantity, 
                                   float gain, bool is_signed) {
    if (quantity == 1) {
        if (is_signed) {
            int16_t signed_val = (int16_t)raw_data[0];
            return (float)signed_val / gain;
        } else {
            return (float)raw_data[0] / gain;
        }
    } else if (quantity == 2) {
        uint32_t combined = ((uint32_t)raw_data[0] << 16) | raw_data[1];
        if (is_signed) {
            int32_t signed_val = (int32_t)combined;
            return (float)signed_val / gain;
        } else {
            return (float)combined / gain;
        }
    }
    return 0.0f;
}

esp_err_t emma_read_single_measurement(emma_client_t *emma, emma_register_t reg, float *value) {
    if (!emma || !emma->initialized || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const emma_reg_config_t *config = get_reg_config(reg);
    if (!config) {
        ESP_LOGE(TAG, "Unknown register: 0x%04X", reg);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint16_t raw_data[4] = {0};
    esp_err_t ret = modbus_tcp_read_registers(&emma->modbus_client, 
                                             config->reg, config->quantity, raw_data);
    
    if (ret == ESP_OK) {
        bool is_signed = (config->reg >= EMMA_REG_FEED_IN_POWER && 
                         config->reg <= EMMA_REG_POWER_FACTOR && 
                         config->quantity == 2) || 
                        (config->reg >= EMMA_REG_PHASE_A_CURRENT && 
                         config->reg <= EMMA_REG_POWER_FACTOR);
        
        *value = convert_register_value(raw_data, config->quantity, config->gain, is_signed);
        emma->read_success++;
        
        ESP_LOGD(TAG, "Read %s: %.3f %s", config->name, *value, config->unit);
    } else {
        emma->read_errors++;
        ESP_LOGW(TAG, "Failed to read %s", config->name);
    }
    
    return ret;
}

esp_err_t emma_read_all_measurements(emma_client_t *emma) {
    if (!emma || !emma->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    emma->measurements.timestamp_us = esp_timer_get_time();
    emma->measurements.data_valid = false;
    
    int successful_reads = 0;
    int total_reads = sizeof(reg_configs) / sizeof(reg_configs[0]);
    
    ESP_LOGI(TAG, "Reading %d EMMA registers...", total_reads);
    
    // Read all measurements
    if (emma_read_single_measurement(emma, EMMA_REG_ENERGY_CHARGED_TODAY, 
                                    &emma->measurements.energy_charged_today) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_ENERGY_DISCHARGED_TODAY, 
                                    &emma->measurements.energy_discharged_today) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_CONSUMPTION_TODAY, 
                                    &emma->measurements.consumption_today) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_FEED_IN_GRID_TODAY, 
                                    &emma->measurements.feed_in_grid_today) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_SUPPLY_FROM_GRID_TODAY, 
                                    &emma->measurements.supply_from_grid_today) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_INVERTER_YIELD_TODAY, 
                                    &emma->measurements.inverter_yield_today) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_PV_YIELD_TODAY, 
                                    &emma->measurements.pv_yield_today) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_PV_OUTPUT_POWER, 
                                    &emma->measurements.pv_output_power) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_LOAD_POWER, 
                                    &emma->measurements.load_power) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_FEED_IN_POWER, 
                                    &emma->measurements.feed_in_power) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_BATTERY_CHARGE_DISCHARGE_POWER, 
                                    &emma->measurements.battery_charge_discharge_power) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_INVERTER_RATED_POWER, 
                                    &emma->measurements.inverter_rated_power) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_INVERTER_ACTIVE_POWER, 
                                    &emma->measurements.inverter_active_power) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_SOC, 
                                    &emma->measurements.soc_percent) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_ESS_CHARGEABLE_CAPACITY, 
                                    &emma->measurements.ess_chargeable_capacity) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_ESS_DISCHARGEABLE_CAPACITY, 
                                    &emma->measurements.ess_dischargeable_capacity) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_BACKUP_POWER_SOC, 
                                    &emma->measurements.backup_power_soc) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_PHASE_A_VOLTAGE, 
                                    &emma->measurements.phase_a_voltage) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_PHASE_B_VOLTAGE, 
                                    &emma->measurements.phase_b_voltage) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_PHASE_C_VOLTAGE, 
                                    &emma->measurements.phase_c_voltage) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_PHASE_A_CURRENT, 
                                    &emma->measurements.phase_a_current) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_PHASE_B_CURRENT, 
                                    &emma->measurements.phase_b_current) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_PHASE_C_CURRENT, 
                                    &emma->measurements.phase_c_current) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_ACTIVE_POWER, 
                                    &emma->measurements.active_power) == ESP_OK) successful_reads++;
    
    if (emma_read_single_measurement(emma, EMMA_REG_POWER_FACTOR, 
                                    &emma->measurements.power_factor) == ESP_OK) successful_reads++;
    
    // Consider data valid if at least 80% of reads were successful
    emma->measurements.data_valid = (successful_reads >= (total_reads * 4 / 5));
    
    ESP_LOGI(TAG, "Register reading completed: %d/%d successful (%.1f%%)", 
             successful_reads, total_reads, 
             (float)successful_reads / total_reads * 100);
    
    return (successful_reads > 0) ? ESP_OK : ESP_FAIL;
}

// Control functions
esp_err_t emma_set_ess_control_mode(emma_client_t *emma, emma_ess_control_mode_t mode) {
    if (!emma || !emma->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = modbus_tcp_write_register(&emma->modbus_client, 
                                             EMMA_REG_ESS_CONTROL_MODE, (uint16_t)mode);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ESS control mode set to: %s", emma_get_ess_mode_string(mode));
    } else {
        ESP_LOGE(TAG, "Failed to set ESS control mode");
    }
    
    return ret;
}

esp_err_t emma_set_power_control_mode(emma_client_t *emma, emma_power_control_mode_t mode) {
    if (!emma || !emma->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = modbus_tcp_write_register(&emma->modbus_client, 
                                             EMMA_REG_POWER_CONTROL_MODE, (uint16_t)mode);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Power control mode set to: %s", emma_get_power_mode_string(mode));
    } else {
        ESP_LOGE(TAG, "Failed to set power control mode");
    }
    
    return ret;
}

esp_err_t emma_set_max_grid_feed_in_power(emma_client_t *emma, float power_kw) {
    if (!emma || !emma->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Convert kW to raw value (gain factor 1000)
    int32_t raw_value = (int32_t)(power_kw * 1000);
    uint16_t reg_data[2];
    reg_data[0] = (uint16_t)(raw_value >> 16);
    reg_data[1] = (uint16_t)(raw_value & 0xFFFF);
    
    esp_err_t ret = modbus_tcp_write_registers(&emma->modbus_client, 
                                              EMMA_REG_MAX_GRID_FEED_IN_POWER, 2, reg_data);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Max grid feed-in power set to: %.3f kW", power_kw);
    } else {
        ESP_LOGE(TAG, "Failed to set max grid feed-in power");
    }
    
    return ret;
}

// Utility functions
void emma_print_measurements(const emma_measurements_t *measurements) {
    if (!measurements) {
        return;
    }
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    HUAWEI EMMA MEASUREMENTS                  ║\n");
    printf("║ Time: %llu us since boot                             ║\n", 
           measurements->timestamp_us);
    printf("║ Data Valid: %s                                           ║\n", 
           measurements->data_valid ? "YES" : "NO");
    printf("╠══════════════════════════════════════════════════════════════╣\n");

    // Energy section
    printf("║ ENERGY (Today)                                               ║\n");
    printf("╟──────────────────────────────────────────────────────────────╢\n");
    printf("║   Energy Charged         : %8.2f kWh                    ║\n", measurements->energy_charged_today);
    printf("║   Energy Discharged      : %8.2f kWh                    ║\n", measurements->energy_discharged_today);
    printf("║   Consumption            : %8.2f kWh                    ║\n", measurements->consumption_today);
    printf("║   Feed-in to Grid        : %8.2f kWh                    ║\n", measurements->feed_in_grid_today);
    printf("║   Supply from Grid       : %8.2f kWh                    ║\n", measurements->supply_from_grid_today);
    printf("║   Inverter Yield         : %8.2f kWh                    ║\n", measurements->inverter_yield_today);
    printf("║   PV Yield               : %8.2f kWh                    ║\n", measurements->pv_yield_today);

    // Power section
    printf("╟──────────────────────────────────────────────────────────────╢\n");
    printf("║ POWER                                                        ║\n");
    printf("╟──────────────────────────────────────────────────────────────╢\n");
    printf("║   PV Output Power        : %8.3f kW                     ║\n", measurements->pv_output_power);
    printf("║   Load Power             : %8.3f kW                     ║\n", measurements->load_power);
    printf("║   Feed-in Power          : %8.3f kW                     ║\n", measurements->feed_in_power);
    printf("║   Battery Charge/Discharge: %8.3f kW                     ║\n", measurements->battery_charge_discharge_power);
    printf("║   Inverter Rated Power   : %8.3f kW                     ║\n", measurements->inverter_rated_power);
    printf("║   Inverter Active Power  : %8.3f kW                     ║\n", measurements->inverter_active_power);

    // Battery section
    printf("╟──────────────────────────────────────────────────────────────╢\n");
    printf("║ BATTERY                                                      ║\n");
    printf("╟──────────────────────────────────────────────────────────────╢\n");
    printf("║   SOC                    : %8.1f %%                      ║\n", measurements->soc_percent);
    printf("║   Chargeable Capacity    : %8.2f kWh                    ║\n", measurements->ess_chargeable_capacity);
    printf("║   Dischargeable Capacity : %8.2f kWh                    ║\n", measurements->ess_dischargeable_capacity);
    printf("║   Backup Power SOC       : %8.1f %%                      ║\n", measurements->backup_power_soc);

    // Grid measurements section
    printf("╟──────────────────────────────────────────────────────────────╢\n");
    printf("║ GRID MEASUREMENTS                                            ║\n");
    printf("╟──────────────────────────────────────────────────────────────╢\n");
    printf("║   Phase A Voltage        : %8.1f V                      ║\n", measurements->phase_a_voltage);
    printf("║   Phase B Voltage        : %8.1f V                      ║\n", measurements->phase_b_voltage);
    printf("║   Phase C Voltage        : %8.1f V                      ║\n", measurements->phase_c_voltage);
    printf("║   Phase A Current        : %8.2f A                      ║\n", measurements->phase_a_current);
    printf("║   Phase B Current        : %8.2f A                      ║\n", measurements->phase_b_current);
    printf("║   Phase C Current        : %8.2f A                      ║\n", measurements->phase_c_current);
    printf("║   Active Power           : %8.3f kW                     ║\n", measurements->active_power);
    printf("║   Power Factor           : %8.3f                        ║\n", measurements->power_factor);

    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

const char* emma_get_ess_mode_string(emma_ess_control_mode_t mode) {
    switch (mode) {
        case EMMA_ESS_MODE_MAXIMUM_SELF_CONSUMPTION: return "Maximum Self-Consumption";
        case EMMA_ESS_MODE_FULLY_FED_TO_GRID: return "Fully Fed to Grid";
        case EMMA_ESS_MODE_TIME_OF_USE: return "Time of Use";
        case EMMA_ESS_MODE_THIRD_PARTY_DISPATCH: return "Third-Party Dispatch";
        default: return "Unknown";
    }
}

const char* emma_get_power_mode_string(emma_power_control_mode_t mode) {
    switch (mode) {
        case EMMA_POWER_MODE_UNLIMITED: return "Unlimited";
        case EMMA_POWER_MODE_GRID_CONNECTED_ZERO: return "Grid Connected Zero Power";
        case EMMA_POWER_MODE_LIMITED_FEED_IN_KW: return "Limited Feed-in (kW)";
        case EMMA_POWER_MODE_LIMITED_FEED_IN_PERCENT: return "Limited Feed-in (%)";
        default: return "Unknown";
    }
}

float emma_get_success_rate(const emma_client_t *emma) {
    if (!emma || (emma->read_success + emma->read_errors) == 0) {
        return 0.0f;
    }
    
    return (float)emma->read_success / (emma->read_success + emma->read_errors) * 100.0f;
}

void emma_reset_statistics(emma_client_t *emma) {
    if (emma) {
        emma->read_success = 0;
        emma->read_errors = 0;
    }
}