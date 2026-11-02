/*
 * TC-LIFE-01 — FSD FR-5.1: the lifecycle decision function equals the §2.3
 * transition table for every listed row and returns "ignored" for every
 * unlisted (state × event) pair.
 */
#include <stdio.h>
#include "lifecycle_core.h"

typedef struct {
    const char *id;
    lc_state_t  from;
    lc_event_t  ev;
    int         guard_creds;   /* -1: any, 0/1: required has_credentials */
    lc_state_t  to;
    uint32_t    actions;
} row_t;

/* FSD §2.3, normative rows. FR-2.17 applies to every state but BOOT. */
static const row_t rows[] = {
    {"FR-2.10", LC_BOOT,          LC_EV_NVS_READ,         0, LC_PROVISIONING,  LC_ACT_START_PORTAL},
    {"FR-2.11", LC_BOOT,          LC_EV_NVS_READ,         1, LC_CONNECTING,    LC_ACT_START_STA},
    {"FR-2.12", LC_CONNECTING,    LC_EV_GOT_IP,          -1, LC_OPERATIONAL,   LC_ACT_START_SERVICES},
    {"FR-2.13", LC_CONNECTING,    LC_EV_CONNECT_FAILED,  -1, LC_CONNECTING,    LC_ACT_RETRY_BACKOFF},
    {"FR-2.14", LC_OPERATIONAL,   LC_EV_STA_DISCONNECTED,-1, LC_WIFI_RECOVERY, LC_ACT_ABORT_JOB},
    {"FR-2.15", LC_WIFI_RECOVERY, LC_EV_GOT_IP,          -1, LC_OPERATIONAL,   LC_ACT_START_SERVICES},
    {"FR-2.16", LC_WIFI_RECOVERY, LC_EV_CONNECT_FAILED,  -1, LC_WIFI_RECOVERY, LC_ACT_RETRY_BACKOFF},
    {"FR-2.17", LC_PROVISIONING,  LC_EV_BOOT_HELD_10S,   -1, LC_FACTORY_RESET, LC_ACT_ERASE_REBOOT},
    {"FR-2.17", LC_CONNECTING,    LC_EV_BOOT_HELD_10S,   -1, LC_FACTORY_RESET, LC_ACT_ERASE_REBOOT},
    {"FR-2.17", LC_OPERATIONAL,   LC_EV_BOOT_HELD_10S,   -1, LC_FACTORY_RESET, LC_ACT_ERASE_REBOOT},
    {"FR-2.17", LC_WIFI_RECOVERY, LC_EV_BOOT_HELD_10S,   -1, LC_FACTORY_RESET, LC_ACT_ERASE_REBOOT},
    {"FR-2.17", LC_FACTORY_RESET, LC_EV_BOOT_HELD_10S,   -1, LC_FACTORY_RESET, LC_ACT_ERASE_REBOOT},
    {"FR-2.20", LC_OPERATIONAL,   LC_EV_IP_LOST,         -1, LC_WIFI_RECOVERY, LC_ACT_ABORT_JOB},
    {"FR-2.18", LC_PROVISIONING,  LC_EV_PORTAL_JOIN_OK,  -1, LC_CONNECTING,    LC_ACT_PERSIST_STOP_AP},
};
#define NROWS (sizeof rows / sizeof rows[0])

static const row_t *find(lc_state_t s, lc_event_t e, int creds)
{
    for (unsigned i = 0; i < NROWS; i++)
        if (rows[i].from == s && rows[i].ev == e &&
            (rows[i].guard_creds < 0 || rows[i].guard_creds == creds))
            return &rows[i];
    return NULL;
}

int main(void)
{
    int fails = 0, checks = 0;
    for (int s = 0; s < LC_STATE_COUNT; s++)
        for (int e = 0; e < LC_EVENT_COUNT; e++)
            for (int c = 0; c <= 1; c++) {
                lc_guard_t g = { .has_credentials = c };
                lc_result_t r = lc_decide((lc_state_t)s, (lc_event_t)e, g);
                const row_t *row = find((lc_state_t)s, (lc_event_t)e, c);
                checks++;
                if (row) {
                    if (r.ignored || r.next != row->to || r.actions != row->actions) {
                        printf("FAIL %s: %s ev=%d creds=%d -> ignored=%d next=%s actions=0x%x "
                               "(want %s 0x%x)\n", row->id, lc_state_name((lc_state_t)s), e, c,
                               r.ignored, lc_state_name(r.next), (unsigned)r.actions,
                               lc_state_name(row->to), (unsigned)row->actions);
                        fails++;
                    }
                } else if (!r.ignored || r.next != (lc_state_t)s || r.actions != LC_ACT_NONE) {
                    /* prohibited: an unlisted pair changes state or acts */
                    printf("FAIL unlisted: %s ev=%d creds=%d -> ignored=%d next=%s actions=0x%x\n",
                           lc_state_name((lc_state_t)s), e, c, r.ignored,
                           lc_state_name(r.next), (unsigned)r.actions);
                    fails++;
                }
            }
    printf("TC-LIFE-01: %d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
