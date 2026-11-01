// Composition root (docs/Method/project/architecture.md): wires components,
// holds no logic. Phase 1 skeleton: proves the build/flash/observe pipeline
// and implements no FSD requirement yet.
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "sdkconfig.h"
#include "udp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (CONFIG_PRINTSERVER_UDP_LOG_HOST[0] != '\0') {
        udp_log_init(CONFIG_PRINTSERVER_UDP_LOG_HOST, 5555);
    }
    ESP_LOGI(TAG, "Init complete");
}
