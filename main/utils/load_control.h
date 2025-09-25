#ifndef LOAD_CONTROL_H
#define LOAD_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Load priority levels
typedef enum {
    LOAD_PRIORITY_ESSENTIAL = 0,      // Always on (refrigerator, lights)
    LOAD_PRIORITY_NORMAL = 1,         // Normal household loads
    LOAD_PRIORITY_FLEXIBLE = 2,       // Can be delayed (dishwasher, washing machine)
    LOAD_PRIORITY_OPPORTUNISTIC = 3,  // Only when excess power (water heater, heat pump)
} load_priority_t;

// Load control modes
typedef enum {
    LOAD_MODE_MANUAL,      // Manual control only
    LOAD_MODE_AUTO,        // Automatic energy management
    LOAD_MODE_SCHEDULED,   // Time-based scheduling
    LOAD_MODE_DISABLED,    // Load disabled
} load_control_mode_t;

// Individual load definition
typedef struct {
    uint8_t id;
    char name[32];
    uint8_t gpio_pin;
    load_priority_t priority;
    load_control_mode_t mode;
    float rated_power_kw;
    bool current_state;
    bool desired_state;
    uint32_t on_time_seconds;
    uint32_t last_toggle_time;
    bool enabled;
} load_device_t;

// Load control system status
typedef struct {
    bool system_enabled;
    uint8_t active_loads;
    float total_controlled_power;
    uint32_t total_switches;
    uint32_t last_update_time;
} load_control_status_t;

// Function prototypes
esp_err_t load_control_init(void);
esp_err_t load_control_add_device(uint8_t id, const char* name, uint8_t gpio_pin, 
                                 load_priority_t priority, float rated_power_kw);
esp_err_t load_control_remove_device(uint8_t id);

// Manual control
esp_err_t load_control_set_device_state(uint8_t id, bool state);
esp_err_t load_control_toggle_device(uint8_t id);
esp_err_t load_control_set_device_mode(uint8_t id, load_control_mode_t mode);

// Automatic control functions
void load_control_enable_flexible_loads(void);
void load_control_disable_non_essential_loads(void);
void load_control_enable_high_power_loads(void);
void load_control_maintain_normal_state(void);

// System control
void load_control_enable_system(bool enable);
void load_control_emergency_shutdown(void);
void load_control_update(void);

// Status and information
load_control_status_t load_control_get_status(void);
load_device_t* load_control_get_device(uint8_t id);
void load_control_print_status(void);
void load_control_print_devices(void);

// Statistics
uint32_t load_control_get_total_switches(void);
float load_control_get_total_controlled_power(void);
void load_control_reset_statistics(void);

#endif // LOAD_CONTROL_H