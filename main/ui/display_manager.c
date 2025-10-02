/**
 * Display Manager using ESP-BOX-3 BSP
 */

#include "display_manager.h"
#include "energy_manager.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "DISPLAY";

// UI elements
static lv_obj_t *label_voltage = NULL;
static lv_obj_t *label_current = NULL;
static lv_obj_t *label_power = NULL;
static lv_obj_t *label_state = NULL;
static lv_obj_t *label_wifi = NULL;
static lv_obj_t *label_emma = NULL;

static bool display_initialized = false;

/**
 * Create UI layout
 */
static void create_ui(void) {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "EMMA Monitor");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Voltage
    label_voltage = lv_label_create(screen);
    lv_label_set_text(label_voltage, "V: ---");
    lv_obj_align(label_voltage, LV_ALIGN_TOP_LEFT, 10, 30);
    
    // Current
    label_current = lv_label_create(screen);
    lv_label_set_text(label_current, "I: ---");
    lv_obj_align(label_current, LV_ALIGN_TOP_LEFT, 10, 50);
    
    // Power
    label_power = lv_label_create(screen);
    lv_label_set_text(label_power, "P: ---");
    lv_obj_align(label_power, LV_ALIGN_TOP_LEFT, 10, 70);
    
    // State
    label_state = lv_label_create(screen);
    lv_label_set_text(label_state, "State: INIT");
    lv_obj_align(label_state, LV_ALIGN_TOP_LEFT, 10, 90);
    
    // WiFi status
    label_wifi = lv_label_create(screen);
    lv_label_set_text(label_wifi, "WiFi: --");
    lv_obj_align(label_wifi, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    
    // EMMA status
    label_emma = lv_label_create(screen);
    lv_label_set_text(label_emma, "EMMA: --");
    lv_obj_align(label_emma, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    
    ESP_LOGI(TAG, "UI created");
}

/**
 * Initialize display using BSP - PREIMENOVANO
 */
esp_err_t display_manager_init(void) {
    ESP_LOGI(TAG, "Initializing BOX-3 display via BSP");
    
    // Initialize display via BSP
    bsp_display_start();
    
    // BSP already initializes LVGL, just lock and create UI
    bsp_display_lock(0);
    create_ui();
    bsp_display_unlock();
    
    display_initialized = true;
    ESP_LOGI(TAG, "Display initialized successfully");
    
    return ESP_OK;
}

/**
 * Update EMMA measurements on display - NOVA FUNKCIJA
 */
esp_err_t display_manager_update_emma_measurements(const emma_measurements_t *measurements) {
    if (!display_initialized || !measurements) {
        return ESP_ERR_INVALID_STATE;
    }
    
    bsp_display_lock(0);
    
    lv_label_set_text_fmt(label_voltage, "V: %.1f V", measurements->phase_a_voltage);
    lv_label_set_text_fmt(label_current, "I: %.2f A", measurements->phase_a_current);
    lv_label_set_text_fmt(label_power, "P: %.0f W", measurements->active_power);
    
    bsp_display_unlock();
    
    return ESP_OK;
}

/**
 * Update connection status - NOVA FUNKCIJA
 */
esp_err_t display_manager_update_connection_status(bool emma_connected, bool wifi_connected, float success_rate) {
    if (!display_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    bsp_display_lock(0);
    
    lv_label_set_text(label_wifi, wifi_connected ? "WiFi: OK" : "WiFi: --");
    lv_label_set_text_fmt(label_emma, "EMMA: %s (%.0f%%)", 
                         emma_connected ? "OK" : "--", success_rate);
    
    bsp_display_unlock();
    
    return ESP_OK;
}

/**
 * Update system status - NOVA FUNKCIJA
 */
esp_err_t display_manager_update_system_status(const char* status_text) {
    if (!display_initialized || !status_text) {
        return ESP_ERR_INVALID_STATE;
    }
    
    bsp_display_lock(0);
    lv_label_set_text_fmt(label_state, "State: %s", status_text);
    bsp_display_unlock();
    
    return ESP_OK;
}

/**
 * Check if display is available
 */
bool display_manager_is_available(void) {
    return display_initialized;
}