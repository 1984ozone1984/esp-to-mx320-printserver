/* Persistent configuration (FSD §19) and the test build's console commands
 * (FR-19.1, FR-19.3, FR-19.5, FR-19.6). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t config_store_init(void);          /* nvs_flash_init (+ repair) */

/* boot_count: incremented once per boot, returns the new value (§19). */
uint32_t  config_boot_count_increment(void);
uint32_t  config_boot_count(void);

bool      config_wifi_get(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
esp_err_t config_wifi_set(const char *ssid, const char *pass);

bool      config_admin_get(char *pass, size_t len);
esp_err_t config_admin_set(const char *pass);

/* Erase the whole nvs partition (FR-19.6). */
esp_err_t config_wipe_all(void);

/* Test build only: start the UART0 console task (cred/admin/reboot/wipe). */
void      config_console_start(void);
