/* HTTP API I/O shell (FSD §11, §22). */
#include "http_api.h"
#include "http_api_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bootloader_common.h"
#include "esp_app_format.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "http";

#define APP_SLOT_MAX 0x300000u   /* §23 app image ceiling */

static http_api_config_t s_cfg;
static httpd_handle_t    s_server;

static esp_err_t send_json(httpd_req_t *r, const char *json)
{
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_sendstr(r, json);
}

static esp_err_t send_err(httpd_req_t *r, const char *status, const char *msg)
{
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "text/plain");
    return httpd_resp_sendstr(r, msg);
}

/* ---- GET /api/status ---------------------------------------------------- */

static esp_err_t status_get(httpd_req_t *r)
{
    http_status_t st = { 0 };
    if (s_cfg.get_status) s_cfg.get_status(&st);
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"fw\":\"%s\",\"state\":\"%s\",\"boot_count\":%u,"
             "\"reset_reason\":\"%s\",\"heap_free\":%u}",
             st.fw ? st.fw : "", st.state ? st.state : "", (unsigned)st.boot_count,
             st.reset_reason ? st.reset_reason : "", (unsigned)esp_get_free_heap_size());
    return send_json(r, buf);
}

/* ---- GET /api/slots (FR-11.10) ------------------------------------------ */

static bool slot_sha256(esp_partition_subtype_t sub, char hex[65])
{
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, sub, NULL);
    uint8_t d[32];
    /* Type "data" makes the helper hash all `size` bytes read from flash,
     * not just the app image inside them (FR-11.10: no cached value). */
    if (!p || bootloader_common_get_sha256_of_partition(p->address, p->size,
                                                        1 /* PART_TYPE_DATA */, d) != ESP_OK)
        return false;
    for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", d[i]);
    return true;
}

static esp_err_t slots_get(httpd_req_t *r)
{
    char h0[65], h1[65];
    if (!slot_sha256(ESP_PARTITION_SUBTYPE_APP_OTA_0, h0) ||
        !slot_sha256(ESP_PARTITION_SUBTYPE_APP_OTA_1, h1))
        return send_err(r, "500 Internal Server Error", "slot read failed");
    const esp_partition_t *run = esp_ota_get_running_partition();
    char buf[220];
    snprintf(buf, sizeof(buf),
             "{\"running\":\"%s\",\"ota_0_sha256\":\"%s\",\"ota_1_sha256\":\"%s\"}",
             run ? run->label : "?", h0, h1);
    return send_json(r, buf);
}

/* ---- POST /api/ota (FR-11.3, FR-11.4, FR-11.6) -------------------------- */

typedef struct {
    const esp_partition_t *part;
    esp_ota_handle_t       h;
    bool                   begun;
    uint8_t                head[sizeof(esp_image_header_t)];
    size_t                 head_len;
    size_t                 total;
    const char            *why;           /* rejection reason, 400 */
} ota_ctx_t;

static bool ota_sink(const uint8_t *d, size_t n, void *arg)
{
    ota_ctx_t *c = arg;
    c->total += n;
    if (c->total > APP_SLOT_MAX) { c->why = "image larger than 3 MB"; return false; }

    if (!c->begun) {
        size_t take = sizeof(c->head) - c->head_len;
        if (take > n) take = n;
        memcpy(c->head + c->head_len, d, take);
        c->head_len += take;
        d += take;
        n -= take;
        if (c->head_len < sizeof(c->head)) return true;

        const esp_image_header_t *ih = (const esp_image_header_t *)c->head;
        if (ih->magic != ESP_IMAGE_HEADER_MAGIC) { c->why = "bad image magic"; return false; }
        if (ih->chip_id != ESP_CHIP_ID_ESP32S3) { c->why = "wrong chip id"; return false; }
        if (esp_ota_begin(c->part, OTA_WITH_SEQUENTIAL_WRITES, &c->h) != ESP_OK) {
            c->why = "ota begin failed";
            return false;
        }
        c->begun = true;
        if (esp_ota_write(c->h, c->head, c->head_len) != ESP_OK) { c->why = "flash write failed"; return false; }
    }
    if (n && esp_ota_write(c->h, d, n) != ESP_OK) { c->why = "flash write failed"; return false; }
    return true;
}

static bool authorized(httpd_req_t *r)
{
    char hdr[160], pass[65];
    if (httpd_req_get_hdr_value_str(r, "Authorization", hdr, sizeof(hdr)) != ESP_OK) return false;
    if (!s_cfg.get_admin_pass || !s_cfg.get_admin_pass(pass, sizeof(pass))) return false;
    bool ok = http_basic_auth_ok(hdr, "admin", pass);
    memset(pass, 0, sizeof(pass));
    return ok;
}

static esp_err_t ota_post(httpd_req_t *r)
{
    if (!authorized(r)) {
        httpd_resp_set_hdr(r, "WWW-Authenticate", "Basic realm=\"mx320\"");
        return send_err(r, "401 Unauthorized", "unauthorized");
    }

    char ct[128], boundary[MP_BOUNDARY_MAX + 1];
    if (httpd_req_get_hdr_value_str(r, "Content-Type", ct, sizeof(ct)) != ESP_OK ||
        !http_multipart_boundary(ct, boundary, sizeof(boundary)))
        return send_err(r, "400 Bad Request", "expected multipart/form-data with field 'image'");

    ota_ctx_t c = { .part = esp_ota_get_next_update_partition(NULL) };
    mp_parser_t *mp = malloc(sizeof(*mp));
    uint8_t *buf = malloc(4096);
    if (!c.part || !mp || !buf) {
        free(mp); free(buf);
        return send_err(r, "500 Internal Server Error", "no memory or no update slot");
    }
    mp_init(mp, boundary, "image", ota_sink, &c);

    ESP_LOGI(TAG, "ota: receiving %u bytes into %s", (unsigned)r->content_len, c.part->label);
    size_t left = r->content_len;
    mp_status_t st = MP_MORE;
    while (left > 0 && st == MP_MORE) {
        int got = httpd_req_recv(r, (char *)buf, left < 4096 ? left : 4096);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0) { c.why = "connection lost"; st = MP_ERROR; break; }
        left -= (size_t)got;
        st = mp_feed(mp, buf, (size_t)got);
    }
    free(mp);
    free(buf);

    esp_err_t err = ESP_FAIL;
    if (st == MP_DONE && c.begun) {
        err = esp_ota_end(c.h);          /* verifies the whole image */
        c.begun = false;
        if (err == ESP_OK) err = esp_ota_set_boot_partition(c.part);
        if (err != ESP_OK) c.why = "image verification failed";
    } else if (!c.why) {
        c.why = c.head_len < sizeof(c.head) ? "image missing or too short" : "malformed multipart body";
    }
    if (c.begun) esp_ota_abort(c.h);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ota: rejected (%s)", c.why);
        return send_err(r, "400 Bad Request", c.why);
    }
    ESP_LOGI(TAG, "ota: %u bytes written to %s, rebooting", (unsigned)c.total, c.part->label);
    send_err(r, "200 OK", "ok, rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));      /* let the reply leave (FR-11.3: reboot <= 5 s) */
    esp_restart();
    return ESP_OK;
}

/* ---- start -------------------------------------------------------------- */

void http_api_start(const http_api_config_t *cfg)
{
    if (s_server) return;
    s_cfg = *cfg;
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.stack_size = 8192;
    hc.lru_purge_enable = true;
    if (httpd_start(&s_server, &hc) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        s_server = NULL;
        return;
    }
    static const httpd_uri_t uris[] = {
        { .uri = "/api/status", .method = HTTP_GET,  .handler = status_get },
        { .uri = "/api/slots",  .method = HTTP_GET,  .handler = slots_get  },
        { .uri = "/api/ota",    .method = HTTP_POST, .handler = ota_post   },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
        httpd_register_uri_handler(s_server, &uris[i]);
    ESP_LOGI(TAG, "http: listening on :80");
}
