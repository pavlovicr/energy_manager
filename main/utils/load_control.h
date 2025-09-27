#ifndef LOAD_CONTROL_H
#define LOAD_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Configuration
#define LOAD_MAX_DEVICES 16  // Dodano definicija za maksimalno število naprav

// Load priority levels
typedef enum {
    LOAD_PRIORITY_ESSENTIAL = 0,      // Always on (refrigerator, lights)
    LOAD_PRIORITY_NORMAL = 1,         // Normal household loads
    LOAD_PRIORITY_FLEXIBLE = 2,       // Can be delayed (dishwasher, washing machine)
    LOAD_PRIORITY_OPPORTUNISTIC = 3,  // Only when excess power (water heater, heat pump)
} load_priority_t;