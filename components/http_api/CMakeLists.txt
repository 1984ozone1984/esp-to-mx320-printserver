idf_component_register(SRCS "http_api_core.c" "http_api.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_http_server app_update bootloader_support esp_partition
                                     esp_app_format esp_system log freertos)
