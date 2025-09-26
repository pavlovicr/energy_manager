#include "display_manager.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

static const char *TAG = "DISPLAY";

// UI elementi
static lv_obj_t *title_label;
static lv_obj_t *pv_power_label, *load_power_label, *grid_power_label;
static lv_obj_t *battery_soc_label, *battery_power_label;
static lv_obj_t *voltage_label, *energy_today_label;
static lv_obj_t *status_label, *connection_label;
static lv_obj_t *soc_bar;

// Barve
#define COLOR_GREEN     lv_color_hex(0x00FF00)
#define COLOR_RED       lv_color_hex(0xFF0000)
#define COLOR_YELLOW    lv_color_hex(0xFFFF00)
#define COLOR_BLUE      lv_color_hex(0x00AAFF)
#define COLOR_WHITE     lv_color_hex(0xFFFFFF)
#define COLOR_ORANGE    lv_color_hex(0xFF8000)
#define COLOR_GRAY      lv_color_hex(0x808080)
#define COLOR_BLACK     lv_color_hex(0x000000)

void display_init(void)
{
    ESP_LOGI(TAG, "Inicializacija Box3 displaya za EMMA sistem...");
    
    // Inicializacija BSP
    bsp_i2c_init();
    bsp_display_start();
    bsp_display_backlight_on();
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Box3 display pripravljen za EMMA podatke");
}

void display_create_emma_screen(void)
{
    // Črno ozadje
    lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

    // === NASLOV ===
    title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(title_label, "HUAWEI EMMA SISTEM");
    lv_obj_set_style_text_color(title_label, COLOR_GREEN, LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 2);

    // === LEVA STRAN - MOCI ===
    // PV produkcija
    pv_power_label = lv_label_create(lv_scr_act());
    lv_label_set_text(pv_power_label, "PV: 0.0 kW");
    lv_obj_set_style_text_color(pv_power_label, COLOR_YELLOW, LV_PART_MAIN);
    lv_obj_set_style_text_font(pv_power_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(pv_power_label, LV_ALIGN_TOP_LEFT, 5, 25);

    // Poraba hise
    load_power_label = lv_label_create(lv_scr_act());
    lv_label_set_text(load_power_label, "Poraba: 0.0 kW");
    lv_obj_set_style_text_color(load_power_label, COLOR_BLUE, LV_PART_MAIN);
    lv_obj_set_style_text_font(load_power_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(load_power_label, LV_ALIGN_TOP_LEFT, 5, 42);

    // Grid moc
    grid_power_label = lv_label_create(lv_scr_act());
    lv_label_set_text(grid_power_label, "Omrezje: 0.0 kW");
    lv_obj_set_style_text_color(grid_power_label, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(grid_power_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(grid_power_label, LV_ALIGN_TOP_LEFT, 5, 59);

    // Napetost
    voltage_label = lv_label_create(lv_scr_act());
    lv_label_set_text(voltage_label, "U: 230 V");
    lv_obj_set_style_text_color(voltage_label, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(voltage_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(voltage_label, LV_ALIGN_TOP_LEFT, 5, 78);

    // === DESNA STRAN - BATERIJA ===
    // SOC naslov
    lv_obj_t *soc_title = lv_label_create(lv_scr_act());
    lv_label_set_text(soc_title, "Baterija:");
    lv_obj_set_style_text_color(soc_title, COLOR_ORANGE, LV_PART_MAIN);
    lv_obj_set_style_text_font(soc_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(soc_title, LV_ALIGN_TOP_RIGHT, -5, 25);

    // SOC vrednost
    battery_soc_label = lv_label_create(lv_scr_act());
    lv_label_set_text(battery_soc_label, "0%");
    lv_obj_set_style_text_color(battery_soc_label, COLOR_GREEN, LV_PART_MAIN);
    lv_obj_set_style_text_font(battery_soc_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(battery_soc_label, LV_ALIGN_TOP_RIGHT, -5, 40);

    // SOC progress bar
    soc_bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(soc_bar, 70, 10);
    lv_obj_align(soc_bar, LV_ALIGN_TOP_RIGHT, -5, 58);
    lv_bar_set_range(soc_bar, 0, 100);
    lv_bar_set_value(soc_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(soc_bar, COLOR_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(soc_bar, COLOR_GREEN, LV_PART_INDICATOR);

    // Baterijska moc
    battery_power_label = lv_label_create(lv_scr_act());
    lv_label_set_text(battery_power_label, "Bat: 0.0 kW");
    lv_obj_set_style_text_color(battery_power_label, COLOR_ORANGE, LV_PART_MAIN);
    lv_obj_set_style_text_font(battery_power_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(battery_power_label, LV_ALIGN_TOP_RIGHT, -5, 75);

    // === SPODNJA SEKCIJA ===
    // Dnesna energija
    energy_today_label = lv_label_create(lv_scr_act());
    lv_label_set_text(energy_today_label, "Danes: PV 0kWh | Poraba 0kWh");
    lv_obj_set_style_text_color(energy_today_label, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(energy_today_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(energy_today_label, LV_ALIGN_BOTTOM_LEFT, 5, -25);

    // Status povezava
    connection_label = lv_label_create(lv_scr_act());
    lv_label_set_text(connection_label, "EMMA: Connecting...");
    lv_obj_set_style_text_color(connection_label, COLOR_YELLOW, LV_PART_MAIN);
    lv_obj_set_style_text_font(connection_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(connection_label, LV_ALIGN_BOTTOM_LEFT, 5, -12);

    // Status sistem
    status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(status_label, "Sistem: Inicializacija");
    lv_obj_set_style_text_color(status_label, COLOR_BLUE, LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -5, -12);

    ESP_LOGI(TAG, "EMMA zaslon ustvarjen");
}

void display_update_emma_measurements(const emma_measurements_t *measurements)
{
    if (!measurements || !measurements->data_valid) {
        ESP_LOGW(TAG, "Neveljavni EMMA podatki");
        return;
    }
    
    char text[50];
    
    // === POSODOBI MOCI ===
    // PV produkcija
    snprintf(text, sizeof(text), "PV: %.1f kW", measurements->pv_output_power);
    lv_label_set_text(pv_power_label, text);
    
    // Poraba hise
    snprintf(text, sizeof(text), "Poraba: %.1f kW", measurements->load_power);
    lv_label_set_text(load_power_label, text);
    
    // Grid moc (pozitivno = iz omrezja, negativno = v omrezje)
    if (measurements->feed_in_power > 0.1f) {
        snprintf(text, sizeof(text), "V omr: %.1f kW", measurements->feed_in_power);
        lv_obj_set_style_text_color(grid_power_label, COLOR_GREEN, LV_PART_MAIN);
    } else if (measurements->supply_from_grid_today > 0.1f) {
        snprintf(text, sizeof(text), "Iz omr: %.1f kW", measurements->active_power);
        lv_obj_set_style_text_color(grid_power_label, COLOR_RED, LV_PART_MAIN);
    } else {
        snprintf(text, sizeof(text), "Omrezje: 0.0 kW");
        lv_obj_set_style_text_color(grid_power_label, COLOR_WHITE, LV_PART_MAIN);
    }
    lv_label_set_text(grid_power_label, text);
    
    // === POSODOBI BATERIJO ===
    // SOC
    snprintf(text, sizeof(text), "%.0f%%", measurements->soc_percent);
    lv_label_set_text(battery_soc_label, text);
    
    // SOC barva glede na nivo
    if (measurements->soc_percent > 80) {
        lv_obj_set_style_text_color(battery_soc_label, COLOR_GREEN, LV_PART_MAIN);
        lv_obj_set_style_bg_color(soc_bar, COLOR_GREEN, LV_PART_INDICATOR);
    } else if (measurements->soc_percent > 30) {
        lv_obj_set_style_text_color(battery_soc_label, COLOR_YELLOW, LV_PART_MAIN);
        lv_obj_set_style_bg_color(soc_bar, COLOR_YELLOW, LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_text_color(battery_soc_label, COLOR_RED, LV_PART_MAIN);
        lv_obj_set_style_bg_color(soc_bar, COLOR_RED, LV_PART_INDICATOR);
    }
    
    lv_bar_set_value(soc_bar, (int)measurements->soc_percent, LV_ANIM_OFF);
    
    // Baterijska moc
    snprintf(text, sizeof(text), "Bat: %.1f kW", measurements->battery_charge_discharge_power);
    lv_label_set_text(battery_power_label, text);
    
    // Barva glede na smer
    if (measurements->battery_charge_discharge_power < -0.1f) {
        lv_obj_set_style_text_color(battery_power_label, COLOR_GREEN, LV_PART_MAIN); // Polnjenje
    } else if (measurements->battery_charge_discharge_power > 0.1f) {
        lv_obj_set_style_text_color(battery_power_label, COLOR_RED, LV_PART_MAIN);   // Praznjenje
    } else {
        lv_obj_set_style_text_color(battery_power_label, COLOR_ORANGE, LV_PART_MAIN); // Mirovanje
    }
    
    // === POSODOBI OSTALO ===
    // Napetost (povprecje faz)
    float avg_voltage = (measurements->phase_a_voltage + measurements->phase_b_voltage + measurements->phase_c_voltage) / 3.0f;
    snprintf(text, sizeof(text), "U: %.0f V", avg_voltage);
    lv_label_set_text(voltage_label, text);
    
    // Dnesna energija
    snprintf(text, sizeof(text), "Danes: PV %.1fkWh | Por %.1fkWh", 
             measurements->pv_yield_today, measurements->consumption_today);
    lv_label_set_text(energy_today_label, text);
    
    // Osveži display
    lv_refr_now(NULL);
    
    ESP_LOGD(TAG, "Display posodobljen: PV=%.1fkW, SOC=%.0f%%", 
             measurements->pv_output_power, measurements->soc_percent);
}

void display_update_connection_status(bool emma_connected, bool wifi_connected, float success_rate)
{
    char text[40];
    
    // EMMA povezava
    if (emma_connected && success_rate > 80.0f) {
        snprintf(text, sizeof(text), "EMMA: OK (%.0f%%)", success_rate);
        lv_obj_set_style_text_color(connection_label, COLOR_GREEN, LV_PART_MAIN);
    } else if (emma_connected && success_rate > 50.0f) {
        snprintf(text, sizeof(text), "EMMA: Slabo (%.0f%%)", success_rate);
        lv_obj_set_style_text_color(connection_label, COLOR_YELLOW, LV_PART_MAIN);
    } else {
        snprintf(text, sizeof(text), "EMMA: Disconnect");
        lv_obj_set_style_text_color(connection_label, COLOR_RED, LV_PART_MAIN);
    }
    lv_label_set_text(connection_label, text);
    
    lv_refr_now(NULL);
}

void display_update_system_status(const char* status_text)
{
    char text[40];
    snprintf(text, sizeof(text), "Sistem: %s", status_text);
    lv_label_set_text(status_label, text);
    
    // Barva glede na status
    if (strstr(status_text, "NORMAL") || strstr(status_text, "OK")) {
        lv_obj_set_style_text_color(status_label, COLOR_GREEN, LV_PART_MAIN);
    } else if (strstr(status_text, "EXCESS") || strstr(status_text, "POLNJEN")) {
        lv_obj_set_style_text_color(status_label, COLOR_BLUE, LV_PART_MAIN);
    } else if (strstr(status_text, "LOW") || strstr(status_text, "NIZKO")) {
        lv_obj_set_style_text_color(status_label, COLOR_RED, LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_color(status_label, COLOR_WHITE, LV_PART_MAIN);
    }
    
    lv_refr_now(NULL);
}

void display_show_error(const char* error_msg)
{
    // Prepiši naslov z napako
    lv_label_set_text(title_label, "NAPAKA SISTEMA");
    lv_obj_set_style_text_color(title_label, COLOR_RED, LV_PART_MAIN);
    
    // Prikaži napako v status labelu
    lv_label_set_text(status_label, error_msg);
    lv_obj_set_style_text_color(status_label, COLOR_RED, LV_PART_MAIN);
    
    lv_refr_now(NULL);
    
    ESP_LOGE(TAG, "Display napaka: %s", error_msg);
}

void display_clear_screen(void)
{
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BLACK, LV_PART_MAIN);
}