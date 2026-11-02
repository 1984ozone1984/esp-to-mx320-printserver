/* Lifecycle decision function — pure core (FSD §2.3, FR-5.1).
 * Stub: every pair is "ignored" until the table is implemented. */
#include "lifecycle_core.h"

lc_result_t lc_decide(lc_state_t state, lc_event_t event, lc_guard_t guard)
{
    (void)event;
    (void)guard;
    return (lc_result_t){ .ignored = true, .next = state, .actions = LC_ACT_NONE };
}

const char *lc_state_name(lc_state_t state)
{
    static const char *const names[LC_STATE_COUNT] = {
        "BOOT", "PROVISIONING", "CONNECTING", "OPERATIONAL", "WIFI_RECOVERY", "FACTORY_RESET",
    };
    return (unsigned)state < LC_STATE_COUNT ? names[state] : "?";
}
