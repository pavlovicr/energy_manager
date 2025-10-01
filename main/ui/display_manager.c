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
static lv_obj_t *label_energy = NULL;
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
    
    // Energy
    label_energy = lv_label_create(screen);
    lv_label_set_text(label_energy, "E: ---");
    lv_obj_align(label_energy, LV_ALIGN_TOP_LEFT, 10, 90);
    
    // State
    label_state = lv_label_create(screen);
    lv_label_set_text(label_state, "State: INIT");
    lv_obj_align(label_state, LV_ALIGN_TOP_LEFT, 10, 110);
    
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
 * Initialize display using BSP
 */
esp_err_t display_init(void) {
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
 * Update display with energy data
 */
void display_update(const energy_data_t *data) {
    if (!display_initialized || !data) {
        return;
    }
    
    bsp_display_lock(0);
    
    lv_label_set_text_fmt(label_voltage, "V: %.1f V", data->voltage_l1);
    lv_label_set_text_fmt(label_current, "I: %.2f A", data->current_l1);
    lv_label_set_text_fmt(label_power, "P: %.0f W", data->power_active);
    lv_label_set_text_fmt(label_energy, "E: %.1f Wh", data->energy_total);
    
    const char *state_str = "UNKNOWN";
    switch (data->state) {
        case ENERGY_STATE_INITIALIZING: state_str = "INIT"; break;
        case ENERGY_STATE_NORMAL: state_str = "NORMAL"; break;
        default: state_str = "ERROR"; break;
    }
    lv_label_set_text_fmt(label_state, "State: %s", state_str);
    
    bsp_display_unlock();
}

/**
 * Update WiFi status
 */
void display_update_wifi_status(bool connected, const char *ssid) {
    if (!display_initialized) return;
    
    bsp_display_lock(0);
    if (connected && ssid) {
        lv_label_set_text_fmt(label_wifi, "WiFi: %.10s", ssid);
    } else {
        lv_label_set_text(label_wifi, "WiFi: --");
    }
    bsp_display_unlock();
}

/**
 * Update EMMA status
 */
void display_update_emma_status(bool connected) {
    if (!display_initialized) return;
    
    bsp_display_lock(0);
    lv_label_set_text(label_emma, connected ? "EMMA: OK" : "EMMA: --");
    bsp_display_unlock();
}

/**
 * Show error message
 */
void display_show_error(const char *message) {
    if (!display_initialized) return;
    ESP_LOGW(TAG, "Error: %s", message);
}

/**
 * Check if display is available
 */
bool display_is_available(void) {
    return display_initialized;
}