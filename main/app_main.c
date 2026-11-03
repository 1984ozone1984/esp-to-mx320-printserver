// Composition root (docs/Method/project/architecture.md): wires components,
// holds no logic. FSD Phase 1: boot marker, NVS config + test-build console,
// lifecycle with WiFi STA.
#include "config_store.h"
#include "device_info.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lifecycle.h"
#include "sdkconfig.h"
#include "udp_log.h"

static const char *TAG = "main";
static char s_hostname[13];

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (CONFIG_PRINTSERVER_UDP_LOG_HOST[0] != '\0') {
        udp_log_init(CONFIG_PRINTSERVER_UDP_LOG_HOST, 5555);
    }

    ESP_ERROR_CHECK(config_store_init());
    device_log_boot_marker(config_boot_count_increment());
    config_console_start();

    lifecycle_config_t lc = { 0 };
    lc.has_credentials = config_wifi_get(lc.ssid, sizeof(lc.ssid), lc.pass, sizeof(lc.pass));
    device_default_hostname(s_hostname);
    lc.hostname = s_hostname;
    lifecycle_start(&lc);

    ESP_LOGI(TAG, "Init complete");
}
