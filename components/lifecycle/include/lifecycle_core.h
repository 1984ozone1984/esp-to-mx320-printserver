/*
 * Lifecycle decision function — pure core, no ESP-IDF includes.
 * FSD §2.3 (state model) and FR-5.1: (state, event, guard) -> (next, actions),
 * "ignored" for every pair the table does not list.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LC_BOOT = 0,
    LC_PROVISIONING,
    LC_CONNECTING,
    LC_OPERATIONAL,
    LC_WIFI_RECOVERY,
    LC_FACTORY_RESET,
    LC_STATE_COUNT
} lc_state_t;

typedef enum {
    LC_EV_NVS_READ = 0,      /* NVS read at boot; guard: has_credentials        */
    LC_EV_GOT_IP,            /* STA got an IPv4 address                          */
    LC_EV_CONNECT_FAILED,    /* STA connect attempt failed or timed out          */
    LC_EV_STA_DISCONNECTED,  /* STA link lost                                    */
    LC_EV_IP_LOST,           /* IPv4 lost while the link stays up                */
    LC_EV_BOOT_HELD_10S,     /* BOOT button held >= 10 s                         */
    LC_EV_PORTAL_JOIN_OK,    /* valid portal form and STA test-join succeeded    */
    LC_EVENT_COUNT
} lc_event_t;

/* Actions are a bitmask; a transition may request several. */
enum {
    LC_ACT_NONE            = 0,
    LC_ACT_START_PORTAL    = 1u << 0, /* setup SoftAP, DNS catch-all, portal  */
    LC_ACT_START_STA       = 1u << 1,
    LC_ACT_START_SERVICES  = 1u << 2, /* RAW (if printer), mDNS, HTTP, MQTT   */
    LC_ACT_RETRY_BACKOFF   = 1u << 3, /* 1, 2, 4 … 60 s, unbounded            */
    LC_ACT_ABORT_JOB       = 1u << 4,
    LC_ACT_ERASE_REBOOT    = 1u << 5, /* erase user config, reboot on release */
    LC_ACT_PERSIST_STOP_AP = 1u << 6, /* persist config, stop SoftAP          */
};

typedef struct {
    bool has_credentials;    /* guard for LC_EV_NVS_READ */
} lc_guard_t;

typedef struct {
    bool       ignored;      /* true: unlisted pair, state unchanged */
    lc_state_t next;
    uint32_t   actions;
} lc_result_t;

lc_result_t lc_decide(lc_state_t state, lc_event_t event, lc_guard_t guard);

const char *lc_state_name(lc_state_t state);   /* "BOOT", "PROVISIONING", … */

/* FR-2.13 / FR-2.16 retry wait for attempt n (0-based): 1, 2, 4, 8, 16, 32, 60, 60 … s */
uint32_t lc_backoff_s(uint32_t attempt);
