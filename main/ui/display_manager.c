#include "display_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Samo vključi BSP in LVGL če je display omogočen
#ifdef CONFIG_BSP_DISPLAY_ENABLED
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#define DISPLAY_AVAILABLE 1
#else
#define DISPLAY_AVAILABLE 0
#endif

static const char *TAG = "DISPLAY";

// Status strukture
static display_status_t g_display_status = {0};
static bool g_initialized = false;

#if DISPLAY_AVAILABLE
// UI elementi
static lv_obj_t *title_label = NULL;
static lv_obj_t *pv_power_label = NULL;
static lv_obj_t *load_power_label = NULL;
static lv_obj_t *grid_power_label = NULL;
static lv_obj_t *battery_soc_label = NULL;
static lv_obj_t *battery_power_label = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *connection_label = NULL;
static lv_obj_t *soc_bar = NULL;

// Barve
#define COLOR_GREEN     lv_color_hex(0x00FF00)
#define COLOR_RED       lv_color_hex(0xFF0000)
#define COLOR_YELLOW    lv_color_hex(0xFFFF00)
#define COLOR_BLUE      lv_color_hex(0x00AAFF)
#define COLOR_WHITE     lv_color_hex(0xFFFFFF)
#define COLOR_ORANGE    lv_color_hex(0xFF8000)
#define COLOR_GRAY      lv_color_hex(0x808080)
#define COLOR_BLACK     lv_color_hex(0x000000)
#endif

esp_err_t display_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing Display Manager");
    
    g_display_status.available = DISPLAY_AVAILABLE;
    g_display_status.initialized = false;
    g_display_status.error_count = 0;
    g_display_status.update_count = 0;
    
#if DISPLAY_AVAILABLE
    ESP_LOGI(TAG, "Display hardware available - initializing BSP");
    
    esp_err_t ret;
    
    ret = bsp_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP I2C init failed: %s", esp_err_to_name(ret));
        g_display_status.error_count++;
        return ret;
    }
    
    esp_task_wdt_reset();
    
    ret = bsp_display_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display start failed: %s", esp_err_to_name(ret));
        g_display_status.error_count++;
        return ret;
    }
    
    bsp_display_backlight_on();
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_task_wdt_reset();
    
    g_display_status.initialized = true;
    ESP_LOGI(TAG, "Display initialized successfully");
#else
    ESP_LOGW(TAG, "Display hardware not available - headless mode");
#endif
    
    g_initialized = true;
    return ESP_OK;
}

esp_err_t display_manager_create_emma_screen(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
#if DISPLAY_AVAILABLE
    if (!g_display_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Creating EMMA screen");
    esp_task_wdt_reset();
    
    lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

    title_label = lv_label_create(lv_scr_act());
    if (title_label) {
        lv_label_set_text(title_label, "HUAWEI EMMA");
        lv_obj_set_style_text_color(title_label, COLOR_GREEN, LV_PART_MAIN);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 2);
    }

    pv_power_label = lv_label_create(lv_scr_act());
    if (pv_power_label) {
        lv_label_set_text(pv_power_label, "PV: 0.0 kW");
        lv_obj_set_style_text_color(pv_power_label, COLOR_YELLOW, LV_PART_MAIN);
        lv_obj_align(pv_power_label, LV_ALIGN_TOP_LEFT, 5, 25);
    }

    load_power_label = lv_label_create(lv_scr_act());
    if (load_power_label) {
        lv_label_set_text(load_power_label, "Load: 0.0 kW");
        lv_obj_set_style_text_color(load_power_label, COLOR_BLUE, LV_PART_MAIN);
        lv_obj_align(load_power_label, LV_ALIGN_TOP_LEFT, 5, 42);
    }

    battery_soc_label = lv_label_create(lv_scr_act());
    if (battery_soc_label) {
        lv_label_set_text(battery_soc_label, "0%");
        lv_obj_set_style_text_color(battery_soc_label, COLOR_GREEN, LV_PART_MAIN);
        lv_obj_align(battery_soc_label, LV_ALIGN_TOP_RIGHT, -5, 40);
    }

    soc_bar = lv_bar_create(lv_scr_act());
    if (soc_bar) {
        lv_obj_set_size(soc_bar, 70, 10);
        lv_obj_align(soc_bar, LV_ALIGN_TOP_RIGHT, -5, 58);
        lv_bar_set_range(soc_bar, 0, 100);
        lv_bar_set_value(soc_bar, 0, LV_ANIM_OFF);
    }

    status_label = lv_label_create(lv_scr_act());
    if (status_label) {
        lv_label_set_text(status_label, "Init");
        lv_obj_set_style_text_color(status_label, COLOR_BLUE, LV_PART_MAIN);
        lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -5, -12);
    }

    connection_label = lv_label_create(lv_scr_act());
    if (connection_label) {
        lv_label_set_text(connection_label, "Connecting...");
        lv_obj_set_style_text_color(connection_label, COLOR_YELLOW, LV_PART_MAIN);
        lv_obj_align(connection_label, LV_ALIGN_BOTTOM_LEFT, 5, -12);
    }

    esp_task_wdt_reset();
    lv_refr_now(NULL);
    ESP_LOGI(TAG, "Screen created");
#endif

    return ESP_OK;
}

esp_err_t display_manager_update_emma_measurements(const emma_measurements_t *measurements)
{
    if (!g_initialized || !measurements) {
        return ESP_ERR_INVALID_ARG;
    }

#if DISPLAY_AVAILABLE
    if (!g_display_status.initialized || !measurements->data_valid) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_task_wdt_reset();
    char text[50];
    
    if (pv_power_label) {
        snprintf(text, sizeof(text), "PV: %.1f kW", measurements->pv_output_power);
        lv_label_set_text(pv_power_label, text);
    }
    
    if (load_power_label) {
        snprintf(text, sizeof(text), "Load: %.1f kW", measurements->load_power);
        lv_label_set_text(load_power_label, text);
    }
    
    if (battery_soc_label) {
        snprintf(text, sizeof(text), "%.0f%%", measurements->soc_percent);
        lv_label_set_text(battery_soc_label, text);
    }
    
    if (soc_bar) {
        lv_bar_set_value(soc_bar, (int)measurements->soc_percent, LV_ANIM_OFF);
    }
    
    static uint32_t last_refresh = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_refresh > 500) {
        lv_refr_now(NULL);
        last_refresh = now;
    }
    
    g_display_status.update_count++;
#endif

    return ESP_OK;
}

esp_err_t display_manager_update_connection_status(bool emma_connected, bool wifi_connected, float success_rate)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

#if DISPLAY_AVAILABLE
    if (!g_display_status.initialized || !connection_label) {
        return ESP_ERR_INVALID_STATE;
    }

    char text[40];
    
    if (emma_connected && success_rate > 80.0f) {
        snprintf(text, sizeof(text), "EMMA: OK");
        lv_obj_set_style_text_color(connection_label, COLOR_GREEN, LV_PART_MAIN);
    } else {
        snprintf(text, sizeof(text), "EMMA: Err");
        lv_obj_set_style_text_color(connection_label, COLOR_RED, LV_PART_MAIN);
    }
    lv_label_set_text(connection_label, text);
#endif

    return ESP_OK;
}

esp_err_t display_manager_update_system_status(const char* status_text)
{
    if (!g_initialized || !status_text) {
        return ESP_ERR_INVALID_ARG;
    }

#if DISPLAY_AVAILABLE
    if (!g_display_status.initialized || !status_label) {
        return ESP_ERR_INVALID_STATE;
    }

    lv_label_set_text(status_label, status_text);
#endif

    return ESP_OK;
}

esp_err_t display_manager_show_error(const char* error_msg)
{
    if (!g_initialized || !error_msg) {
        return ESP_ERR_INVALID_ARG;
    }

#if DISPLAY_AVAILABLE
    if (title_label) {
        lv_label_set_text(title_label, "ERROR");
        lv_obj_set_style_text_color(title_label, COLOR_RED, LV_PART_MAIN);
    }
#endif

    ESP_LOGE(TAG, "Display error: %s", error_msg);
    return ESP_OK;
}

esp_err_t display_manager_clear_screen(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

#if DISPLAY_AVAILABLE
    if (g_display_status.initialized) {
        lv_obj_clean(lv_scr_act());
        title_label = NULL;
        pv_power_label = NULL;
        load_power_label = NULL;
        battery_soc_label = NULL;
        soc_bar = NULL;
        status_label = NULL;
        connection_label = NULL;
    }
#endif

    return ESP_OK;
}

esp_err_t display_manager_test(void)
{
    ESP_LOGI(TAG, "Display test");
    
#if DISPLAY_AVAILABLE
    if (!g_display_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    lv_obj_t *test = lv_label_create(lv_scr_act());
    if (test) {
        lv_label_set_text(test, "TEST OK");
        lv_obj_align(test, LV_ALIGN_CENTER, 0, 0);
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(2000));
        lv_obj_del(test);
    }
#endif

    return ESP_OK;
}

bool display_manager_is_available(void)
{
    return g_display_status.available && g_display_status.initialized;
}

display_status_t display_manager_get_status(void)
{
    return g_display_status;
}

void display_manager_print_status(void)
{
    printf("\n=== DISPLAY STATUS ===\n");
    printf("Available: %s\n", g_display_status.available ? "YES" : "NO");
    printf("Initialized: %s\n", g_display_status.initialized ? "YES" : "NO");
    printf("Updates: %lu\n", (unsigned long)g_display_status.update_count);
    printf("Errors: %lu\n", (unsigned long)g_display_status.error_count);
    printf("======================\n\n");
}