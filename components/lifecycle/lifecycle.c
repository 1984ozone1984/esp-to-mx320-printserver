/* Lifecycle I/O shell (FSD §2.3, §5, §14, §20 markers).
 *
 * One task owns the state. WiFi/IP event handlers and the retry timer only
 * post messages to it; the task asks lc_decide() what to do and does it. */
#include "lifecycle.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "lifecycle";

typedef enum {
    MSG_EVENT,        /* a §2.3 event for lc_decide()               */
    MSG_STA_STARTED,  /* driver up: make the first connect attempt  */
    MSG_RETRY_DUE,    /* backoff wait over: next connect attempt    */
} msg_kind_t;

typedef struct {
    msg_kind_t kind;
    lc_event_t event;
} msg_t;

static lifecycle_config_t s_cfg;
static QueueHandle_t      s_q;
static esp_timer_handle_t s_retry_timer;
static volatile lc_state_t s_state = LC_BOOT;
static uint32_t           s_attempt;      /* backoff index, reset on got ip */

static void post(msg_kind_t kind, lc_event_t ev)
{
    msg_t m = { .kind = kind, .event = ev };
    xQueueSend(s_q, &m, 0);
}

/* ---- event sources ------------------------------------------------------ */

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    if (id == WIFI_EVENT_STA_START) {
        post(MSG_STA_STARTED, 0);
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_state == LC_OPERATIONAL) {
            ESP_LOGI(TAG, "wifi: lost");
            post(MSG_EVENT, LC_EV_STA_DISCONNECTED);
        }
        /* The association that just ended (or never formed) is a failed
         * attempt: CONNECTING/WIFI_RECOVERY retry with backoff (FR-2.13/2.16). */
        post(MSG_EVENT, LC_EV_CONNECT_FAILED);
    }
}

static void on_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    if (id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "wifi: got ip " IPSTR, IP2STR(&e->ip_info.ip));
        post(MSG_EVENT, LC_EV_GOT_IP);
    } else if (id == IP_EVENT_STA_LOST_IP) {
        post(MSG_EVENT, LC_EV_IP_LOST);
    }
}

static void on_retry_timer(void *arg)
{
    (void)arg;
    post(MSG_RETRY_DUE, 0);
}

/* ---- actions ------------------------------------------------------------ */

static void connect_attempt(void)
{
    ESP_LOGI(TAG, "wifi: connecting");
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect: %s", esp_err_to_name(err));
        post(MSG_EVENT, LC_EV_CONNECT_FAILED);
    }
}

static void start_sta(void)
{
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (s_cfg.hostname) esp_netif_set_hostname(netif, s_cfg.hostname);

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_LOST_IP, on_ip, NULL, NULL));

    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid, s_cfg.ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, s_cfg.pass, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = s_cfg.pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    /* The driver keeps no copy of credentials in its own NVS namespace:
     * config_store is the only place they live (§19, FR-19.6 wipe). */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);   /* architecture.md, serves NFR-14.1 */
}

static void retry_backoff(void)
{
    uint32_t n = lc_backoff_s(s_attempt);
    if (s_attempt < 32) s_attempt++;
    ESP_LOGI(TAG, "wifi: retry in %u s", (unsigned)n);
    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, (uint64_t)n * 1000000ULL);
}

static void run_actions(uint32_t act)
{
    if (act & LC_ACT_START_PORTAL && s_cfg.start_portal) s_cfg.start_portal();
    if (act & LC_ACT_START_STA) start_sta();
    if (act & LC_ACT_ABORT_JOB && s_cfg.abort_job) s_cfg.abort_job();
    if (act & LC_ACT_RETRY_BACKOFF) retry_backoff();
    if (act & LC_ACT_START_SERVICES) {
        s_attempt = 0;
        esp_timer_stop(s_retry_timer);
        if (s_cfg.start_services) s_cfg.start_services();
    }
    if (act & LC_ACT_ERASE_REBOOT) {
        if (s_cfg.erase_config) s_cfg.erase_config();
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    }
}

static void lifecycle_task(void *arg)
{
    (void)arg;
    msg_t m;
    for (;;) {
        if (xQueueReceive(s_q, &m, portMAX_DELAY) != pdTRUE) continue;
        switch (m.kind) {
        case MSG_STA_STARTED:
            if (s_state == LC_CONNECTING) connect_attempt();
            break;
        case MSG_RETRY_DUE:
            if (s_state == LC_CONNECTING || s_state == LC_WIFI_RECOVERY) connect_attempt();
            break;
        case MSG_EVENT: {
            lc_guard_t g = { .has_credentials = s_cfg.has_credentials };
            lc_result_t r = lc_decide(s_state, m.event, g);
            if (r.ignored) break;
            if (r.next != s_state) {
                s_state = r.next;
                ESP_LOGI(TAG, "state: %s", lc_state_name(r.next));
            }
            run_actions(r.actions);
            break;
        }
        }
    }
}

/* ---- API ---------------------------------------------------------------- */

void lifecycle_start(const lifecycle_config_t *cfg)
{
    s_cfg = *cfg;
    s_q = xQueueCreate(16, sizeof(msg_t));
    const esp_timer_create_args_t ta = { .callback = on_retry_timer, .name = "wifi_retry" };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &s_retry_timer));
    xTaskCreate(lifecycle_task, "lifecycle", 4096, NULL, 5, NULL);
    post(MSG_EVENT, LC_EV_NVS_READ);
}

lc_state_t lifecycle_state(void) { return s_state; }
