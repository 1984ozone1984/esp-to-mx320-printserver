/* Device identity and boot facts (FSD §18, §20 boot marker, Appendix B). */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* Firmware version: the app description's version (git describe, §18). */
const char *device_fw_version(void);

/* Reset reason name without the ESP_RST_ prefix (Appendix B). */
const char *device_reset_reason(void);

/* <id>: lowercase hex of the last 3 bytes of the WiFi STA MAC (7 bytes incl. NUL). */
void device_id(char out[7]);

/* Default hostname `mx320-<id>` (13 bytes incl. NUL). */
void device_default_hostname(char out[13]);

/* Emit `boot: fw=<ver> reset=<reason> count=<n> flash=<MB> psram=<MB>` (FR-20.1). */
void device_log_boot_marker(uint32_t boot_count);
