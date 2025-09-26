# main/CMakeLists.txt - Energy Manager s Box3 displayom

# main/CMakeLists.txt - Energy Manager s Box3 displayom

idf_component_register(
    SRCS 
        "main.c"
        "modbus/modbus_tcp.c"      
        "modbus/emma_modbus.c"     
        "utils/wifi_manager.c"     
        "utils/load_control.c"     
        "display/display_manager.c"    # Dodano za Box3 display
    INCLUDE_DIRS 
        "."
        "modbus"                   
        "utils"                    
        "display"                  # Dodano za Box3 display
    REQUIRES 
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_timer
        lwip
        freertos
        log
        driver
        esp_system
        esp_common
        espressif__esp-box-3       # Pravilno ime iz managed_components
        espressif__esp_lvgl_port   # LVGL port
)