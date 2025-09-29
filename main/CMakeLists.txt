# main/CMakeLists.txt - Modularna Energy Manager konfiguracija

idf_component_register(
    SRCS 
        # Glavna aplikacija
        "main.c"
        
        # Core moduli
        "core/energy_manager.c"
        "core/system_config.c"
        "core/app_tasks.c"
        
        # Communication moduli
        "communication/modbus/emma_modbus.c"
        "communication/modbus/modbus_tcp.c"
        "communication/wifi/wifi_manager.c"
        
        # Control moduli
        "control/load_control.c"
        
        # UI moduli
        "ui/display_manager.c"
        
    INCLUDE_DIRS 
        "."                              # Glavna main mapa
        "core"                           # Core komponente
        "communication"                  # Komunikacijske komponente  
        "communication/modbus"           # Modbus specifično
        "communication/wifi"             # WiFi specifično
        "control"                        # Kontrolne komponente
        "ui"                            # UI komponente
        
    REQUIRES 
        # ESP-IDF core komponente
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
        espressif__esp-modbus
        mb_example_common
        
        # Box3 BSP komponente (opcijsko)
        espressif__esp-box-3
        espressif__esp_lvgl_port
        
    #OPTIONAL_REQUIRES
        # Opcijske komponente ki ne prekinejo build-a če manjkajo
    #    esp_lcd
    #    button
       
)