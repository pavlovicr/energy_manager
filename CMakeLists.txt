# The following lines of boilerplate have to be in your project's
# CMakeLists.txt. You can modify them to suit your needs, but keep the
# required variables and targets.

cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(energy_manager)

# Add component directories
set(EXTRA_COMPONENT_DIRS 
    components/energy
    components/emma  
    components/display
    components/wifi
    components/system
)

# Register main component
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES 
                    energy_manager 
                    emma_manager 
                    display_manager 
                    wifi_manager 
                    system_monitor 
                    emma_modbus
                    freertos
                    esp_timer
                    nvs_flash
                    esp_wifi
                    esp_netif
                    esp_event
                    esp_system)