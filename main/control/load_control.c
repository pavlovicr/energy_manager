#include "load_control.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static const char* TAG = "LOAD_CONTROL";

// Global variables
static load_device_t devices[LOAD_MAX_DEVICES];
static load_control_status_t system_status;
static uint8_t device_count = 0;

// Helper functions
static load_device_t* find_device_by_id(uint8_t id);
static void update_system_status(void);

esp_err_t load_control_init(void)
{
    ESP_LOGI(TAG, "Initializing Load Control System");
    
    // Initialize system status
    memset(&system_status, 0, sizeof(load_control_status_t));
    system_status.system_enabled = true;
    system_status.last_update_time = esp_timer_get_time() / 1000000;
    
    // Initialize devices array
    memset(devices, 0, sizeof(devices));
    device_count = 0;
    
    ESP_LOGI(TAG, "Load Control System initialized");
    return ESP_OK;
}

esp_err_t load_control_add_device(uint8_t id, const char* name, uint8_t gpio_pin, 
                                 load_priority_t priority, float rated_power_kw)
{
    if (device_count >= LOAD_MAX_DEVICES) {
        ESP_LOGE(TAG, "Maximum devices limit reached");
        return ESP_ERR_NO_MEM;
    }
    
    if (find_device_by_id(id) != NULL) {
        ESP_LOGE(TAG, "Device with ID %d already exists", id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!name || strlen(name) == 0) {
        ESP_LOGE(TAG, "Device name cannot be empty");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << gpio_pin),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d", gpio_pin);
        return ret;
    }
    
    // Add device
    load_device_t* device = &devices[device_count];
    device->id = id;
    strncpy(device->name, name, sizeof(device->name) - 1);
    device->gpio_pin = gpio_pin;
    device->priority = priority;
    device->mode = LOAD_MODE_MANUAL;
    device->rated_power_kw = rated_power_kw;
    device->current_state = false;
    device->desired_state = false;
    device->on_time_seconds = 0;
    device->last_toggle_time = 0;
    device->enabled = true;
    
    // Set initial GPIO state
    gpio_set_level(gpio_pin, 0);
    
    device_count++;
    
    ESP_LOGI(TAG, "Added device: ID=%d, Name=%s, GPIO=%d, Priority=%d, Power=%.2fkW", 
             id, name, gpio_pin, priority, rated_power_kw);
    
    update_system_status();
    return ESP_OK;
}

esp_err_t load_control_remove_device(uint8_t id)
{
    for (int i = 0; i < device_count; i++) {
        if (devices[i].id == id) {
            // Turn off device before removing
            if (devices[i].current_state) {
                gpio_set_level(devices[i].gpio_pin, 0);
            }
            
            // Shift remaining devices
            for (int j = i; j < device_count - 1; j++) {
                devices[j] = devices[j + 1];
            }
            
            device_count--;
            ESP_LOGI(TAG, "Removed device ID %d", id);
            update_system_status();
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "Device with ID %d not found", id);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t load_control_set_device_state(uint8_t id, bool state)
{
    load_device_t* device = find_device_by_id(id);
    if (!device) {
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!device->enabled) {
        ESP_LOGW(TAG, "Device %s is disabled", device->name);
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!system_status.system_enabled) {
        ESP_LOGW(TAG, "Load control system is disabled");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Set GPIO state
    gpio_set_level(device->gpio_pin, state ? 1 : 0);
    
    // Update device state
    uint32_t current_time = esp_timer_get_time() / 1000000;
    
    if (device->current_state != state) {
        device->current_state = state;
        device->last_toggle_time = current_time;
        system_status.total_switches++;
        
        ESP_LOGI(TAG, "Device %s turned %s", device->name, state ? "ON" : "OFF");
    }
    
    device->desired_state = state;
    update_system_status();
    
    return ESP_OK;
}

esp_err_t load_control_toggle_device(uint8_t id)
{
    load_device_t* device = find_device_by_id(id);
    if (!device) {
        return ESP_ERR_NOT_FOUND;
    }
    
    return load_control_set_device_state(id, !device->current_state);
}

esp_err_t load_control_set_device_mode(uint8_t id, load_control_mode_t mode)
{
    load_device_t* device = find_device_by_id(id);
    if (!device) {
        return ESP_ERR_NOT_FOUND;
    }
    
    device->mode = mode;
    ESP_LOGI(TAG, "Device %s mode set to %d", device->name, mode);
    
    return ESP_OK;
}

void load_control_enable_flexible_loads(void)
{
    ESP_LOGI(TAG, "Enabling flexible loads due to excess power");
    
    for (int i = 0; i < device_count; i++) {
        if (devices[i].priority == LOAD_PRIORITY_FLEXIBLE && 
            devices[i].mode == LOAD_MODE_AUTO &&
            devices[i].enabled && 
            !devices[i].current_state) {
            
            load_control_set_device_state(devices[i].id, true);
        }
    }
}

void load_control_disable_non_essential_loads(void)
{
    ESP_LOGI(TAG, "Disabling non-essential loads due to low battery");
    
    for (int i = 0; i < device_count; i++) {
        if ((devices[i].priority == LOAD_PRIORITY_NORMAL || 
             devices[i].priority == LOAD_PRIORITY_FLEXIBLE ||
             devices[i].priority == LOAD_PRIORITY_OPPORTUNISTIC) &&
            devices[i].mode == LOAD_MODE_AUTO &&
            devices[i].enabled && 
            devices[i].current_state) {
            
            load_control_set_device_state(devices[i].id, false);
        }
    }
}

void load_control_enable_high_power_loads(void)
{
    ESP_LOGI(TAG, "Enabling high power loads to reduce grid feed-in");
    
    for (int i = 0; i < device_count; i++) {
        if (devices[i].priority == LOAD_PRIORITY_OPPORTUNISTIC && 
            devices[i].mode == LOAD_MODE_AUTO &&
            devices[i].enabled && 
            !devices[i].current_state &&
            devices[i].rated_power_kw > 1.0f) {
            
            load_control_set_device_state(devices[i].id, true);
        }
    }
}

void load_control_maintain_normal_state(void)
{
    ESP_LOGD(TAG, "Maintaining normal load state");
    
    // Turn off opportunistic loads if not needed
    for (int i = 0; i < device_count; i++) {
        if (devices[i].priority == LOAD_PRIORITY_OPPORTUNISTIC && 
            devices[i].mode == LOAD_MODE_AUTO &&
            devices[i].current_state) {
            
            load_control_set_device_state(devices[i].id, false);
        }
    }
}

void load_control_enable_system(bool enable)
{
    system_status.system_enabled = enable;
    
    if (!enable) {
        // Turn off all non-essential devices
        ESP_LOGI(TAG, "System disabled - turning off non-essential loads");
        for (int i = 0; i < device_count; i++) {
            if (devices[i].priority != LOAD_PRIORITY_ESSENTIAL && 
                devices[i].current_state) {
                load_control_set_device_state(devices[i].id, false);
            }
        }
    }
    
    ESP_LOGI(TAG, "Load control system %s", enable ? "enabled" : "disabled");
}

void load_control_emergency_shutdown(void)
{
    ESP_LOGW(TAG, "EMERGENCY SHUTDOWN - turning off all loads");
    
    for (int i = 0; i < device_count; i++) {
        if (devices[i].current_state) {
            gpio_set_level(devices[i].gpio_pin, 0);
            devices[i].current_state = false;
            devices[i].last_toggle_time = esp_timer_get_time() / 1000000;
        }
    }
    
    system_status.system_enabled = false;
    update_system_status();
}

void load_control_update(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000000;
    
    // Update on-time for active devices
    for (int i = 0; i < device_count; i++) {
        if (devices[i].current_state) {
            devices[i].on_time_seconds = current_time - devices[i].last_toggle_time;
        }
    }
    
    update_system_status();
}

load_control_status_t load_control_get_status(void)
{
    update_system_status();
    return system_status;
}

load_device_t* load_control_get_device(uint8_t id)
{
    return find_device_by_id(id);
}

void load_control_print_status(void)
{
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                      LOAD CONTROL STATUS                    ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ System Enabled          : %-32s ║\n", 
           system_status.system_enabled ? "YES" : "NO");
    printf("║ Active Loads            : %-8d                        ║\n", 
           system_status.active_loads);
    printf("║ Total Controlled Power  : %8.2f kW                     ║\n", 
           system_status.total_controlled_power);
    printf("║ Total Switches          : %-8lu                        ║\n", 
           (unsigned long)system_status.total_switches);
    printf("║ Device Count            : %-8d                        ║\n", 
           device_count);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

void load_control_print_devices(void)
{
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                        LOAD DEVICES                         ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    
    if (device_count == 0) {
        printf("║                     No devices configured                   ║\n");
    } else {
        for (int i = 0; i < device_count; i++) {
            printf("║ ID: %2d | %-15s | GPIO: %2d | %s | %4.1fkW ║\n",
                   devices[i].id,
                   devices[i].name,
                   devices[i].gpio_pin,
                   devices[i].current_state ? "ON " : "OFF",
                   devices[i].rated_power_kw);
        }
    }
    
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

uint32_t load_control_get_total_switches(void)
{
    return system_status.total_switches;
}

float load_control_get_total_controlled_power(void)
{
    return system_status.total_controlled_power;
}

void load_control_reset_statistics(void)
{
    system_status.total_switches = 0;
    
    for (int i = 0; i < device_count; i++) {
        devices[i].on_time_seconds = 0;
    }
    
    ESP_LOGI(TAG, "Load control statistics reset");
}

// Helper functions
static load_device_t* find_device_by_id(uint8_t id)
{
    for (int i = 0; i < device_count; i++) {
        if (devices[i].id == id) {
            return &devices[i];
        }
    }
    return NULL;
}

static void update_system_status(void)
{
    system_status.active_loads = 0;
    system_status.total_controlled_power = 0.0f;
    
    for (int i = 0; i < device_count; i++) {
        if (devices[i].current_state) {
            system_status.active_loads++;
            system_status.total_controlled_power += devices[i].rated_power_kw;
        }
    }
    
    system_status.last_update_time = esp_timer_get_time() / 1000000;
}