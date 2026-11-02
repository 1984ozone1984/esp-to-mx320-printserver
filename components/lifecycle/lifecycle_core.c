/* Lifecycle decision function — pure core (FSD §2.3, FR-5.1).
 * Every row of the normative transition table is one case below; any pair
 * not listed is "ignored": state unchanged, no action. */
#include "lifecycle_core.h"

static lc_result_t go(lc_state_t next, uint32_t actions)
{
    return (lc_result_t){ .ignored = false, .next = next, .actions = actions };
}

lc_result_t lc_decide(lc_state_t state, lc_event_t event, lc_guard_t guard)
{
    /* FR-2.17: any state but BOOT, BOOT held >= 10 s. */
    if (event == LC_EV_BOOT_HELD_10S && state != LC_BOOT && state < LC_STATE_COUNT)
        return go(LC_FACTORY_RESET, LC_ACT_ERASE_REBOOT);

    switch (state) {
    case LC_BOOT:
        if (event == LC_EV_NVS_READ)
            return guard.has_credentials
                ? go(LC_CONNECTING, LC_ACT_START_STA)          /* FR-2.11 */
                : go(LC_PROVISIONING, LC_ACT_START_PORTAL);    /* FR-2.10 */
        break;
    case LC_PROVISIONING:
        if (event == LC_EV_PORTAL_JOIN_OK)
            return go(LC_CONNECTING, LC_ACT_PERSIST_STOP_AP);  /* FR-2.18 */
        break;
    case LC_CONNECTING:
        if (event == LC_EV_GOT_IP)
            return go(LC_OPERATIONAL, LC_ACT_START_SERVICES);  /* FR-2.12 */
        if (event == LC_EV_CONNECT_FAILED)
            return go(LC_CONNECTING, LC_ACT_RETRY_BACKOFF);    /* FR-2.13 */
        break;
    case LC_OPERATIONAL:
        if (event == LC_EV_STA_DISCONNECTED)
            return go(LC_WIFI_RECOVERY, LC_ACT_ABORT_JOB);     /* FR-2.14 */
        if (event == LC_EV_IP_LOST)
            return go(LC_WIFI_RECOVERY, LC_ACT_ABORT_JOB);     /* FR-2.20 */
        break;
    case LC_WIFI_RECOVERY:
        if (event == LC_EV_GOT_IP)
            return go(LC_OPERATIONAL, LC_ACT_START_SERVICES);  /* FR-2.15 */
        if (event == LC_EV_CONNECT_FAILED)
            return go(LC_WIFI_RECOVERY, LC_ACT_RETRY_BACKOFF); /* FR-2.16 */
        break;
    default:
        break;
    }
    return (lc_result_t){ .ignored = true, .next = state, .actions = LC_ACT_NONE };
}

const char *lc_state_name(lc_state_t state)
{
    static const char *const names[LC_STATE_COUNT] = {
        "BOOT", "PROVISIONING", "CONNECTING", "OPERATIONAL", "WIFI_RECOVERY", "FACTORY_RESET",
    };
    return (unsigned)state < LC_STATE_COUNT ? names[state] : "?";
}

uint32_t lc_backoff_s(uint32_t attempt)
{
    /* FR-2.13: 1, 2, 4, … capped at 60 s; attempt counts from 0. */
    return attempt >= 6 ? 60u : (1u << attempt);
}
