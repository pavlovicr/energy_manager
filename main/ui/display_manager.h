#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "esp_err.h"
#include "communication/modbus/emma_modbus.h"
#include <stdint.h>
#include <stdbool.h>

// Status struktura za display
typedef struct {
    bool available;         // Ali je display hardware dostopen
    bool initialized;       // Ali je uspešno inicializiran
    uint32_t update_count;  // Število uspešnih posodobitev
    uint32_t error_count;   // Število napak
} display_status_t;

// API funkcije
esp_err_t display_manager_init(void);
esp_err_t display_manager_create_emma_screen(void);
esp_err_t display_manager_update_emma_measurements(const emma_measurements_t *measurements);
esp_err_t display_manager_update_connection_status(bool emma_connected, bool wifi_connected, float success_rate);
esp_err_t display_manager_update_system_status(const char* status_text);

// Dodatne funkcije
esp_err_t display_manager_show_error(const char* error_msg);
esp_err_t display_manager_clear_screen(void);
esp_err_t display_manager_test(void);

// Status funkcije
display_status_t display_manager_get_status(void);
bool display_manager_is_available(void);
void display_manager_print_status(void);

#endif // DISPLAY_MANAGER_H