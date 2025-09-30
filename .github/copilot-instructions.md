# ESP32 Energy Manager AI Development Guide

This document provides essential context for AI agents working with the ESP32 Energy Manager codebase.

## Project Overview

ESP32 Energy Manager is a modular embedded system that monitors and controls energy flow in smart home/building environments. Built on the ESP-IDF framework, it provides real-time energy monitoring, device control, and smart load management capabilities.

## Core Architecture

The codebase follows a clean modular architecture pattern:

- `core/` - Core system functionality
  - `energy_manager.[ch]` - Main energy management logic and state machine
  - `system_config.[ch]` - System configuration and persistence (uses NVS)
  - `app_tasks.[ch]` - FreeRTOS task management and coordination

- `communication/` - External interfaces
  - `modbus/` - Modbus TCP protocol for device communication
  - `wifi/` - WiFi network connectivity and management

- `control/` - System control logic
  - `load_control.[ch]` - Smart load switching and energy optimization

- `ui/` - User interface components
  - `display_manager.[ch]` - Display control using ESP-BOX 3 BSP

## Key Development Workflows

### Build & Flash
```bash
# Initial setup 
idf.py set-target esp32s3     # Project targets ESP32-S3
idf.py menuconfig             # Configure UART pins and Modbus settings

# Build and flash
idf.py -p [PORT] build flash monitor
```

### Component Initialization Order
The system components must be initialized in the following order (see `main.c`):
1. NVS flash storage 
2. System configuration
3. Core energy manager
4. Communication modules (WiFi before Modbus)
5. UI components
6. Application tasks

### Application Tasks
Four main FreeRTOS tasks handle the core functionality (`app_tasks.c`):
1. EMMA Communication - Handles Modbus device data exchange
2. Energy Analysis - Processes measurements and controls loads
3. Display Update - Refreshes UI at regular intervals
4. WiFi Monitor - Manages network connectivity

## Project Conventions

1. Error Handling
   - Use `ESP_ERROR_CHECK()` for critical initialization paths
   - Return `esp_err_t` from public APIs
   - Use ESP logging macros with appropriate tags (`ESP_LOGI`, `ESP_LOGW`, etc.)

2. Task Management
   - Core tasks defined in `app_tasks.c`
   - Stack sizes: 2048-4096 bytes based on task needs
   - High priority tasks must update watchdog timer
   - Always use task handles from `app_tasks.h`

3. State Management
   - System state tracked in `energy_manager.h` 
   - State transitions logged at INFO level
   - Input validation before state updates
   - Use state machine patterns for complex flows

## Dependencies

- ESP-IDF v5.0+ 
- External components (managed via `idf.py add-dependency`):
  - espressif/esp-box-3 - UI BSP
  - espressif/esp-modbus - Protocol stack
  - espressif/esp_lvgl_port - Graphics library

## Common Gotchas

1. Component Initialization
   - NVS flash MUST be initialized first
   - WiFi must be initialized before Modbus TCP
   - Display/UI is optional - handle absent hardware gracefully 

2. Task Safety
   - Main tasks must reset watchdog every 10 seconds
   - Protect shared resources with mutex locks
   - Prefer queue-based communication between tasks
   - Consider stack size when processing large Modbus responses

3. Communication
   - Each Modbus device needs unique slave address
   - RS485 connections require proper termination 
   - WiFi errors must be handled with reconnection logic

4. Memory Management
   - Use static allocation when possible
   - Check heap fragmentation during long-running operations
   - Release resources in error paths