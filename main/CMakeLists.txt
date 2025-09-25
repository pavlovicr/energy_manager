# main/CMakeLists.txt - Energy Manager main komponenta
# Ta datoteka mora biti v main/ direktoriju

# Začnemo z osnovnimi datotekami, ki jih imate
# Dodajte nove datoteke ko jih ustvarite

idf_component_register(
    SRCS 
        "main.c"
         "modbus/modbus_tcp.c"      # Dodajte ko ustvarite
         "modbus/emma_modbus.c"     
         "utils/wifi_manager.c"     
        # "utils/load_control.c"     # Dodajte ko ustvarite
    INCLUDE_DIRS 
        "."
         "modbus"                   
         "utils"                    
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
)