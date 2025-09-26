#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "esp_err.h"
#include "emma_modbus.h"  // Za emma_measurements_t strukturo

// Display funkcije za EMMA sistem
void display_init(void);
void display_create_emma_screen(void);
void display_update_emma_measurements(const emma_measurements_t *measurements);
void display_update_connection_status(bool emma_connected, bool wifi_connected, float success_rate);
void display_update_system_status(const char* status_text);
void display_show_error(const char* error_msg);
void display_clear_screen(void);

#endif // DISPLAY_MANAGER_H