idf_component_register(SRCS "lifecycle_core.c" "lifecycle.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_wifi esp_netif esp_event esp_timer esp_system log freertos)
