idf_component_register(SRCS "device_info.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_app_format spi_flash esp_psram esp_hw_support esp_system log)
