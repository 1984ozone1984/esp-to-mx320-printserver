/* Device identity and boot facts (FSD §18, §20, Appendix B). */
#include "device_info.h"

#include <stdio.h>
#include "esp_app_desc.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_psram.h"
#include "esp_system.h"

static const char *TAG = "main";

const char *device_fw_version(void)
{
    return esp_app_get_description()->version;
}

const char *device_reset_reason(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:    return "POWERON";
    case ESP_RST_EXT:        return "EXT";
    case ESP_RST_SW:         return "SW";
    case ESP_RST_PANIC:      return "PANIC";
    case ESP_RST_INT_WDT:    return "INT_WDT";
    case ESP_RST_TASK_WDT:   return "TASK_WDT";
    case ESP_RST_WDT:        return "WDT";
    case ESP_RST_DEEPSLEEP:  return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:   return "BROWNOUT";
    case ESP_RST_SDIO:       return "SDIO";
    case ESP_RST_USB:        return "USB";
    case ESP_RST_JTAG:       return "JTAG";
    case ESP_RST_EFUSE:      return "EFUSE";
    case ESP_RST_PWR_GLITCH: return "PWR_GLITCH";
    case ESP_RST_CPU_LOCKUP: return "CPU_LOCKUP";
    default:                 return "UNKNOWN";
    }
}

void device_id(char out[7])
{
    uint8_t mac[6] = { 0 };
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, 7, "%02x%02x%02x", mac[3], mac[4], mac[5]);
}

void device_default_hostname(char out[13])
{
    char id[7];
    device_id(id);
    snprintf(out, 13, "mx320-%s", id);
}

void device_log_boot_marker(uint32_t boot_count)
{
    uint32_t flash = 0;
    esp_flash_get_size(NULL, &flash);
    size_t psram = esp_psram_is_initialized() ? esp_psram_get_size() : 0;
    ESP_LOGI(TAG, "boot: fw=%s reset=%s count=%u flash=%u psram=%u",
             device_fw_version(), device_reset_reason(), (unsigned)boot_count,
             (unsigned)(flash >> 20), (unsigned)((psram + (1u << 19)) >> 20));
}
