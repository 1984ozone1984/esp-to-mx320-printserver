/* Test-build console on UART0 (FSD FR-19.1, FR-19.3, FR-19.5, FR-19.6).
 * Compiled into every image, started only when CONFIG_PRINTSERVER_TEST_BUILD
 * is set: a release build never reads the console, so it ignores every line
 * (FR-19.2, FR-19.4, FR-19.7, FR-19.8). */
#include "config_store.h"

#include <ctype.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "console";

static char *skip_ws(char *p) { while (*p == ' ' || *p == '\t') p++; return p; }

static void restart_after_log(void)
{
    vTaskDelay(pdMS_TO_TICKS(200));   /* let the marker leave UART and UDP */
    esp_restart();
}

static void handle(char *line)
{
    char *cmd = skip_ws(line);
    if (strncmp(cmd, "cred ", 5) == 0) {
        char *ssid = skip_ws(cmd + 5);
        char *sp = strchr(ssid, ' ');
        const char *pass = "";
        if (sp) { *sp = '\0'; pass = skip_ws(sp + 1); }
        if (ssid[0] == '\0' || strlen(ssid) > 32 || strlen(pass) > 63) {
            ESP_LOGW(TAG, "cred: rejected");
            return;
        }
        if (config_wifi_set(ssid, pass) == ESP_OK) ESP_LOGI(TAG, "cred: stored");
    } else if (strncmp(cmd, "admin ", 6) == 0) {
        const char *pw = skip_ws(cmd + 6);
        size_t n = strlen(pw);
        if (n < 8 || n > 64) { ESP_LOGW(TAG, "admin: rejected"); return; }
        if (config_admin_set(pw) == ESP_OK) ESP_LOGI(TAG, "admin: stored");
    } else if (strcmp(cmd, "reboot") == 0) {
        ESP_LOGI(TAG, "reboot: requested");
        restart_after_log();
    } else if (strcmp(cmd, "wipe") == 0) {
        if (config_wipe_all() == ESP_OK) {
            ESP_LOGI(TAG, "wipe: done");
            restart_after_log();
        } else {
            ESP_LOGE(TAG, "wipe: failed");
        }
    } else if (cmd[0] != '\0') {
        ESP_LOGD(TAG, "unknown command");
    }
}

static void console_task(void *arg)
{
    (void)arg;
    char line[160];
    size_t len = 0;
    uint8_t c;
    for (;;) {
        if (uart_read_bytes(UART_NUM_0, &c, 1, portMAX_DELAY) != 1) continue;
        if (c == '\r' || c == '\n') {
            if (len) { line[len] = '\0'; handle(line); len = 0; }
        } else if (len < sizeof(line) - 1 && isprint(c)) {
            line[len++] = (char)c;
        } else if (len >= sizeof(line) - 1) {
            len = 0;   /* overlong line: drop it */
        }
    }
}

void config_console_start(void)
{
#if CONFIG_PRINTSERVER_TEST_BUILD
    if (!uart_is_driver_installed(UART_NUM_0)) {
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0));
    }
    xTaskCreate(console_task, "console", 4096, NULL, 3, NULL);
#endif
}
