/*
 * Lifecycle I/O shell (FSD §2.3, §5, §14): drives the pure decision function
 * from WiFi/IP events and executes its actions. Emits the §20 markers
 * `state: <S>`, `wifi: connecting`, `wifi: retry in <n> s`, `wifi: got ip <ip>`,
 * `wifi: lost`.
 *
 * L2 component: it knows no L1 component. Everything it cannot do itself
 * (services, portal, job abort, config erase) is a hook registered by main/.
 */
#pragma once
#include <stdbool.h>
#include "lifecycle_core.h"

typedef struct {
    bool        has_credentials;     /* guard of the BOOT/NVS-read row       */
    char        ssid[33];
    char        pass[64];
    const char *hostname;            /* DHCP hostname; NULL = IDF default    */

    /* Action hooks; any may be NULL (feature not in this phase). Called from
     * the lifecycle task. */
    void (*start_services)(void);    /* LC_ACT_START_SERVICES                */
    void (*start_portal)(void);      /* LC_ACT_START_PORTAL                  */
    void (*abort_job)(void);         /* LC_ACT_ABORT_JOB                     */
    void (*erase_config)(void);      /* LC_ACT_ERASE_REBOOT, before reboot   */
} lifecycle_config_t;

/* Starts the lifecycle task and feeds it the BOOT/NVS-read event.
 * esp_netif_init() and the default event loop must already exist. */
void lifecycle_start(const lifecycle_config_t *cfg);

lc_state_t lifecycle_state(void);
