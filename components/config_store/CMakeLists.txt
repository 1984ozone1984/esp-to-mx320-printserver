idf_component_register(SRCS "config_store.c" "config_console.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES nvs_flash esp_driver_uart log freertos esp_system)
