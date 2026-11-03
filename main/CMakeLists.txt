idf_component_register(SRCS "app_main.c"
                       PRIV_REQUIRES udp_log config_store device_info lifecycle esp_netif esp_event log)
