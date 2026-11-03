/* HTTP API on port 80 (FSD §11). Phase 1: GET /api/status (Phase 1 fields),
 * GET /api/slots (FR-11.10), POST /api/ota (FR-11.3, auth §22). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *fw;
    const char *reset_reason;
    const char *state;
    uint32_t    boot_count;
} http_status_t;

typedef struct {
    void (*get_status)(http_status_t *out);           /* filled by main/   */
    bool (*get_admin_pass)(char *buf, size_t len);    /* false: none set   */
} http_api_config_t;

/* Idempotent: a second call while the server runs does nothing. */
void http_api_start(const http_api_config_t *cfg);
