#include "energy_manager.h"
#include "communication/modbus/emma_modbus.h"
#include "control/load_control.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char* TAG = "ENERGY_MGR";

// Globalne strukture
static energy_manager_t g_energy_mgr;
static emma_client_t g_emma_client;
static bool g_initialized = false;
static energy_data_t energy_data = {0};

// Helper funkcije
static energy_state_t analyze_energy_situation(const emma_measurements_t *measurements);
static void execute_energy_control(energy_state_t state);
static const char* get_state_string(energy_state_t state);

energy_state_t energy_manager_get_current_state(void) {
    return g_energy_manager.current_state;  // ali kako se imenuje tvoja globalna spremenljivka
}

esp_err_t energy_manager_init(void)
{
    esp_err_t energy_manager_init(void) {
    // Initialize energy data
    energy_data.state = ENERGY_STATE_INITIALIZING;
    return ESP_OK; 
    }
    ESP_LOGI(TAG, "Initializing Energy Manager Core");
    
    memset(&g_energy_mgr, 0, sizeof(energy_manager_t));
    
    // Privzeta konfiguracija
    g_energy_mgr.config.excess_power_threshold_kw = 1.0f;
    g_energy_mgr.config.battery_high_soc_threshold = 90.0f;
    g_energy_mgr.config.battery_low_soc_threshold = 20.0f;
    g_energy_mgr.config.grid_feed_limit_kw = 8.0f;
    g_energy_mgr.config.load_control_enabled = true;
    g_energy_mgr.config.grid_limit_active = true;
    
    // Inicializacija EMMA klienta
    esp_err_t ret = emma_init(&g_emma_client, EMMA_DEFAULT_IP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize EMMA client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    g_energy_mgr.current_state = ENERGY_STATE_INITIALIZING;
    g_energy_mgr.last_update_time = esp_timer_get_time();
    g_initialized = true;
    
    ESP_LOGI(TAG, "Energy Manager Core initialized");
    return ESP_OK;
}

esp_err_t energy_manager_connect_emma(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = emma_connect(&g_emma_client);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Connected to EMMA successfully");
        g_energy_mgr.emma_connected = true;
    } else {
        ESP_LOGW(TAG, "EMMA connection failed, will retry later");
        g_energy_mgr.emma_connected = false;
    }
    
    return ret;
}

esp_err_t energy_manager_update_measurements(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!g_energy_mgr.emma_connected) {
        // Poskusi ponovno povezavo
        energy_manager_connect_emma();
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = emma_read_all_measurements(&g_emma_client);
    if (ret == ESP_OK) {
        // Kopiraj najnovejše meritve
        g_energy_mgr.latest_data = g_emma_client.measurements;
        g_energy_mgr.data_valid = g_emma_client.measurements.data_valid;
        g_energy_mgr.last_update_time = esp_timer_get_time();
        g_energy_mgr.stats.successful_reads++;
        
        ESP_LOGD(TAG, "EMMA measurements updated");
    } else {
        g_energy_mgr.data_valid = false;
        g_energy_mgr.stats.failed_reads++;
        ESP_LOGW(TAG, "Failed to read EMMA measurements");
        
        // Po več neuspešnih poskusih prekini povezavo
        if (g_energy_mgr.stats.failed_reads % 5 == 0) {
            emma_disconnect(&g_emma_client);
            g_energy_mgr.emma_connected = false;
        }
    }
    
    return ret;
}

energy_state_t energy_manager_analyze_and_control(void)
{
    if (!g_initialized || !g_energy_mgr.data_valid) {
        return ENERGY_STATE_NO_DATA;
    }
    
    // Analiziraj trenutno energetsko situacijo
    energy_state_t new_state = analyze_energy_situation(&g_energy_mgr.latest_data);
    
    // Izvedi kontrolne ukrepe če se je stanje spremenilo
    if (new_state != g_energy_mgr.current_state) {
        ESP_LOGI(TAG, "Energy state changed: %s -> %s", 
                 get_state_string(g_energy_mgr.current_state),
                 get_state_string(new_state));
        
        execute_energy_control(new_state);
        g_energy_mgr.current_state = new_state;
        g_energy_mgr.stats.state_changes++;
    }
    
    return new_state;
}

const emma_measurements_t* energy_manager_get_latest_data(void)
{
    if (!g_initialized || !g_energy_mgr.data_valid) {
        return NULL;
    }
    return &g_energy_mgr.latest_data;
}

energy_statistics_t energy_manager_get_statistics(void)
{
    return g_energy_mgr.stats;
}

const char* energy_manager_get_status_string(void)
{
    if (!g_initialized) {
        return "NOT_INITIALIZED";
    }
    return get_state_string(g_energy_mgr.current_state);
}

float energy_manager_get_success_rate(void)
{
    if (!g_initialized) {
        return 0.0f;
    }
    
    uint32_t total = g_energy_mgr.stats.successful_reads + g_energy_mgr.stats.failed_reads;
    if (total == 0) {
        return 0.0f;
    }
    
    return (float)g_energy_mgr.stats.successful_reads / total * 100.0f;
}

bool energy_manager_is_emma_connected(void)
{
    return g_initialized && g_energy_mgr.emma_connected;
}

// Helper funkcije
static energy_state_t analyze_energy_situation(const emma_measurements_t *measurements)
{
    if (!measurements || !measurements->data_valid) {
        return ENERGY_STATE_NO_DATA;
    }
    
    // Preveri nizko baterijo
    if (measurements->soc_percent < g_energy_mgr.config.battery_low_soc_threshold) {
        return ENERGY_STATE_BATTERY_LOW;
    }
    
    // Preveri previsoko napajanje v omrežje
    if (g_energy_mgr.config.grid_limit_active && 
        measurements->feed_in_power > g_energy_mgr.config.grid_feed_limit_kw) {
        return ENERGY_STATE_GRID_LIMITING;
    }
    
    // Preveri presežek energije
    float excess_power = measurements->pv_output_power - measurements->load_power;
    bool battery_full_enough = measurements->soc_percent > g_energy_mgr.config.battery_high_soc_threshold;
    
    if (excess_power > g_energy_mgr.config.excess_power_threshold_kw && battery_full_enough) {
        return ENERGY_STATE_EXCESS_AVAILABLE;
    }
    
    return ENERGY_STATE_NORMAL;
}

static void execute_energy_control(energy_state_t state)
{
    if (!g_energy_mgr.config.load_control_enabled) {
        ESP_LOGD(TAG, "Load control disabled - no action taken");
        return;
    }
    
    switch (state) {
        case ENERGY_STATE_EXCESS_AVAILABLE:
            ESP_LOGI(TAG, "Enabling loads due to excess power");
            load_control_enable_flexible_loads();
            break;
            
        case ENERGY_STATE_BATTERY_LOW:
            ESP_LOGI(TAG, "Reducing loads due to low battery");
            load_control_disable_non_essential_loads();
            break;
            
        case ENERGY_STATE_GRID_LIMITING:
            ESP_LOGI(TAG, "Enabling loads to reduce grid feed-in");
            load_control_enable_high_power_loads();
            break;
            
        case ENERGY_STATE_NORMAL:
        default:
            ESP_LOGD(TAG, "Normal operation - maintaining current state");
            load_control_maintain_normal_state();
            break;
    }
}

static const char* get_state_string(energy_state_t state)
{
    switch (state) {
        case ENERGY_STATE_INITIALIZING: return "INITIALIZING";
        case ENERGY_STATE_NO_DATA: return "NO_DATA";
        case ENERGY_STATE_NORMAL: return "NORMAL";
        case ENERGY_STATE_EXCESS_AVAILABLE: return "EXCESS_POWER";
        case ENERGY_STATE_BATTERY_LOW: return "BATTERY_LOW";
        case ENERGY_STATE_GRID_LIMITING: return "GRID_LIMITING";
        default: return "UNKNOWN";
    }
}