#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Status struktura za task monitoring
typedef struct {
    bool emma_task_running;
    bool analysis_task_running;
    bool display_task_running;
    bool wifi_monitor_running;
    uint32_t total_tasks;
    uint32_t free_heap;
} task_status_t;

// API funkcije
esp_err_t app_tasks_start(void);
esp_err_t app_tasks_stop(void);
task_status_t app_tasks_get_status(void);

#endif // APP_TASKS_H