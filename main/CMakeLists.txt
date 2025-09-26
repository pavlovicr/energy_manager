# Main application component
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