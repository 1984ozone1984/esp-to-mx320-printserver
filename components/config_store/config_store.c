/* NVS-backed configuration (FSD §19). Passwords are never logged (NFR-22.2). */
#include "config_store.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NS "mx320"

static const char *TAG = "config";
static uint32_t s_boot_count;

esp_err_t config_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs needs erase (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t open_rw(nvs_handle_t *h) { return nvs_open(NS, NVS_READWRITE, h); }

uint32_t config_boot_count_increment(void)
{
    nvs_handle_t h;
    uint32_t n = 0;
    if (open_rw(&h) == ESP_OK) {
        nvs_get_u32(h, "boot_count", &n);
        n++;
        nvs_set_u32(h, "boot_count", n);
        nvs_commit(h);
        nvs_close(h);
    }
    s_boot_count = n;
    return n;
}

uint32_t config_boot_count(void) { return s_boot_count; }

static bool get_str(const char *key, char *out, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t l = len;
    esp_err_t err = nvs_get_str(h, key, out, &l);
    nvs_close(h);
    return err == ESP_OK && out[0] != '\0';
}

static esp_err_t set_strs(const char *k1, const char *v1, const char *k2, const char *v2)
{
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, k1, v1);
    if (err == ESP_OK && k2) err = nvs_set_str(h, k2, v2);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

bool config_wifi_get(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    if (!get_str("wifi_ssid", ssid, ssid_len)) return false;
    if (!get_str("wifi_pass", pass, pass_len)) pass[0] = '\0';
    return true;
}

esp_err_t config_wifi_set(const char *ssid, const char *pass)
{
    return set_strs("wifi_ssid", ssid, "wifi_pass", pass);
}

bool config_admin_get(char *pass, size_t len) { return get_str("admin_pass", pass, len); }

esp_err_t config_admin_set(const char *pass) { return set_strs("admin_pass", pass, NULL, NULL); }

esp_err_t config_wipe_all(void)
{
    nvs_flash_deinit();
    return nvs_flash_erase();
}
