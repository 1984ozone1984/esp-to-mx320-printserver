/*
 * Commissioning probe — debugging-agenda #3, #4, #5, #14. Not product code.
 *
 * Logs every device the USB host stack enumerates (through any hub), reads the
 * IEEE 1284 Device ID of each printer-class (07/01) interface, and polls its
 * port-status byte. A heartbeat proves the probe is alive at any moment.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "usb/usb_host.h"

#define MAX_DEVS        8
#define DEVID_MAX       1024
#define PS_POLL_MS      1000
#define PS_REPEAT_MS    30000

typedef enum { EV_NEW, EV_GONE } ev_kind_t;
typedef struct { ev_kind_t kind; uint8_t addr; usb_device_handle_t hdl; } ev_t;

typedef struct {
    bool used;
    uint8_t addr;
    usb_device_handle_t hdl;
    int printer_intf;       /* -1 = none */
    int printer_alt;
    int last_ps;            /* -1 = never read */
    TickType_t last_ps_log;
} dev_t_;

static usb_host_client_handle_t s_client;
static QueueHandle_t s_events;
static SemaphoreHandle_t s_xfer_done;
static dev_t_ s_devs[MAX_DEVS];
static volatile int s_dev_count;

static void client_cb(const usb_host_client_event_msg_t *msg, void *arg)
{
    ev_t ev = {0};
    if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        ev.kind = EV_NEW;
        ev.addr = msg->new_dev.address;
    } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ev.kind = EV_GONE;
        ev.hdl = msg->dev_gone.dev_hdl;
    } else {
        return;
    }
    xQueueSend(s_events, &ev, 0);
}

static void xfer_cb(usb_transfer_t *xfer)
{
    xSemaphoreGive(s_xfer_done);
}

/* Class-specific IN control request on an interface; returns bytes received or -1. */
static int class_in(usb_device_handle_t hdl, uint8_t breq, uint16_t wvalue, uint16_t windex,
                    uint8_t *out, int maxlen)
{
    usb_transfer_t *x;
    if (usb_host_transfer_alloc(sizeof(usb_setup_packet_t) + maxlen, 0, &x) != ESP_OK) {
        return -1;
    }
    usb_setup_packet_t *s = (usb_setup_packet_t *)x->data_buffer;
    s->bmRequestType = 0xA1;            /* IN | class | interface */
    s->bRequest = breq;
    s->wValue = wvalue;
    s->wIndex = windex;
    s->wLength = maxlen;
    x->num_bytes = sizeof(usb_setup_packet_t) + maxlen;
    x->device_handle = hdl;
    x->bEndpointAddress = 0;
    x->callback = xfer_cb;
    x->context = NULL;
    x->timeout_ms = 2000;
    int ret = -1;
    if (usb_host_transfer_submit_control(s_client, x) == ESP_OK
        && xSemaphoreTake(s_xfer_done, pdMS_TO_TICKS(3000)) == pdTRUE) {
        if (x->status == USB_TRANSFER_STATUS_COMPLETED) {
            ret = x->actual_num_bytes - (int)sizeof(usb_setup_packet_t);
            if (ret > 0) {
                memcpy(out, x->data_buffer + sizeof(usb_setup_packet_t), ret);
            }
        } else {
            printf("probe: xfer breq=%u status=%d\n", breq, (int)x->status);
        }
    }
    usb_host_transfer_free(x);
    return ret;
}

static void read_device_id(dev_t_ *d)
{
    static uint8_t buf[DEVID_MAX];
    /* GET_DEVICE_ID: wValue = config index (0), wIndex = (interface << 8) | alt */
    int n = class_in(d->hdl, 0, 0, (uint16_t)((d->printer_intf << 8) | d->printer_alt),
                     buf, sizeof(buf));
    if (n < 2) {
        printf("probe: deviceid addr=%u len=%d id=<none>\n", d->addr, n);
        return;
    }
    int len = (buf[0] << 8) | buf[1];   /* big-endian length incl. the 2 bytes */
    if (len > n) {
        len = n;
    }
    printf("probe: deviceid addr=%u len=%d id=%.*s\n", d->addr, len, len - 2, (char *)buf + 2);
}

static void poll_port_status(dev_t_ *d)
{
    uint8_t v;
    if (class_in(d->hdl, 1, 0, (uint16_t)d->printer_intf, &v, 1) != 1) {
        return;
    }
    TickType_t now = xTaskGetTickCount();
    if (v != d->last_ps || now - d->last_ps_log >= pdMS_TO_TICKS(PS_REPEAT_MS)) {
        printf("probe: portstatus addr=%u intf=%d value=0x%02x paper_empty=%d select=%d not_error=%d\n",
               d->addr, d->printer_intf, v, (v >> 5) & 1, (v >> 4) & 1, (v >> 3) & 1);
        d->last_ps = v;
        d->last_ps_log = now;
    }
}

static void on_new(uint8_t addr)
{
    dev_t_ *d = NULL;
    for (int i = 0; i < MAX_DEVS; i++) {
        if (!s_devs[i].used) { d = &s_devs[i]; break; }
    }
    if (!d) {
        printf("probe: attach addr=%u ignored (table full)\n", addr);
        return;
    }
    usb_device_handle_t hdl;
    esp_err_t err = usb_host_device_open(s_client, addr, &hdl);
    if (err != ESP_OK) {
        printf("probe: attach addr=%u open failed %s\n", addr, esp_err_to_name(err));
        return;
    }
    memset(d, 0, sizeof(*d));
    d->used = true;
    d->addr = addr;
    d->hdl = hdl;
    d->printer_intf = -1;
    d->last_ps = -1;
    s_dev_count++;

    const usb_device_desc_t *dd;
    usb_device_info_t info;
    usb_host_get_device_descriptor(hdl, &dd);
    usb_host_device_info(hdl, &info);
    printf("probe: attach addr=%u vid=%04x pid=%04x class=%02x speed=%s\n", addr,
           dd->idVendor, dd->idProduct, dd->bDeviceClass,
           info.speed == USB_SPEED_LOW ? "low" : info.speed == USB_SPEED_FULL ? "full" : "high");

    const usb_config_desc_t *cfg;
    if (usb_host_get_active_config_descriptor(hdl, &cfg) != ESP_OK) {
        printf("probe: attach addr=%u no config descriptor\n", addr);
        return;
    }
    int offset = 0;
    const usb_standard_desc_t *cur = (const usb_standard_desc_t *)cfg;
    while ((cur = usb_parse_next_descriptor_of_type(cur, cfg->wTotalLength,
                                                    USB_B_DESCRIPTOR_TYPE_INTERFACE, &offset)) != NULL) {
        const usb_intf_desc_t *in = (const usb_intf_desc_t *)cur;
        printf("probe: intf addr=%u num=%u alt=%u class=%02x sub=%02x proto=%02x\n", addr,
               in->bInterfaceNumber, in->bAlternateSetting, in->bInterfaceClass,
               in->bInterfaceSubClass, in->bInterfaceProtocol);
        if (in->bInterfaceClass == 0x07 && in->bInterfaceSubClass == 0x01 && d->printer_intf < 0) {
            d->printer_intf = in->bInterfaceNumber;
            d->printer_alt = in->bAlternateSetting;
        }
    }
    if (d->printer_intf >= 0) {
        read_device_id(d);
        poll_port_status(d);
    }
}

static void on_gone(usb_device_handle_t hdl)
{
    for (int i = 0; i < MAX_DEVS; i++) {
        if (s_devs[i].used && s_devs[i].hdl == hdl) {
            printf("probe: gone addr=%u\n", s_devs[i].addr);
            usb_host_device_close(s_client, hdl);
            s_devs[i].used = false;
            s_dev_count--;
            return;
        }
    }
}

static void lib_task(void *arg)
{
    usb_host_config_t cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&cfg));
    printf("probe: host installed\n");
    xTaskNotifyGive((TaskHandle_t)arg);
    while (1) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            printf("probe: host no clients\n");
        }
    }
}

static void client_task(void *arg)
{
    while (1) {
        usb_host_client_handle_events(s_client, portMAX_DELAY);
    }
}

static void heartbeat_task(void *arg)
{
    for (unsigned n = 1;; n++) {
        printf("probe: alive %u devices=%d\n", n, s_dev_count);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    printf("probe: usb-host fw=%s\n", esp_app_get_description()->version);
    s_events = xQueueCreate(16, sizeof(ev_t));
    s_xfer_done = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 5, NULL, 0);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    usb_host_client_config_t ccfg = {
        .is_synchronous = false,
        .max_num_event_msg = 8,
        .async = { .client_event_callback = client_cb, .callback_arg = NULL },
    };
    ESP_ERROR_CHECK(usb_host_client_register(&ccfg, &s_client));
    xTaskCreatePinnedToCore(client_task, "usb_cli", 4096, NULL, 5, NULL, 0);
    xTaskCreate(heartbeat_task, "alive", 2048, NULL, 1, NULL);

    /* Device work happens here, outside the client callback, so control
     * transfers can block while the client task keeps handling events. */
    while (1) {
        ev_t ev;
        if (xQueueReceive(s_events, &ev, pdMS_TO_TICKS(PS_POLL_MS)) == pdTRUE) {
            if (ev.kind == EV_NEW) {
                on_new(ev.addr);
            } else {
                on_gone(ev.hdl);
            }
            continue;
        }
        for (int i = 0; i < MAX_DEVS; i++) {
            if (s_devs[i].used && s_devs[i].printer_intf >= 0) {
                poll_port_status(&s_devs[i]);
            }
        }
    }
}
