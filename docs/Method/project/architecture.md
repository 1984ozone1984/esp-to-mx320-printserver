# MX320 WiFi Print Server — Project bindings (HOW)

How the code mirrors the FSD §2.4 layers. Rules here are not observable from
outside the device; the requirements they serve are in
[`../../Functionality/FSD.md`](../../Functionality/FSD.md).

## Source layout

ESP-IDF project; one component per FSD component, named by layer:

| Layer | Component directory | FSD chapter |
|---|---|---|
| L2 | `components/lifecycle` | §5, §2.3 |
| L2 | `components/job_forwarder` | §6 |
| L2 | `components/printer_status` | §7 |
| L1 | `components/usb_printer` | §8 |
| L1 | `components/raw_port` | §9 |
| L1 | `components/discovery` | §10 |
| L1 | `components/http_api` | §11 |
| L1 | `components/captive_portal` | §12 |
| L1 | `components/mqtt_publisher` | §13 |
| D | `components/config_store` | §19 |
| D | `components/udp_log` | §20 |
| D | `components/device_info` | §18, §20 boot marker, App. B `reset_reason` |
| — | `main/` | composition root only: wires components, no logic |

## Dependency rules

- L2 never includes an L1 header; L1 never includes an L0 wrapper of another L1
  component. Upward needs are inverted through callbacks registered in `main/`.
- Every component splits a **pure core** (`*_core.c`, no ESP-IDF includes) from
  its I/O shell. Pure cores: lifecycle transition function, status mapping,
  1284 Device-ID parser, mDNS TXT builder, status JSON serializer, HA discovery
  builder, config validators. Host-tier tests compile only the cores.
- `PRIV_REQUIRES` lists only components the code includes today.

## Implementation decisions

- WiFi power-save is `WIFI_PS_NONE` (serves FSD NFR-14.1).
- The job forwarder reads TCP into a fixed 64 KiB ring in internal RAM and
  writes to bulk-OUT in transfers of ≤ 16 KiB; it reads TCP only when the ring
  has space (FSD FR-6.5).
- Bulk-IN is drained continuously and discarded (FSD §1 non-goals).
- The watchdog fault-injection endpoint `/api/test/hang` and the UART `cred`
  and `admin` console commands exist only under `CONFIG_PRINTSERVER_TEST_BUILD`; the release
  `sdkconfig.defaults` leaves it unset (FSD FR-19.1/19.2, NFR-17.1). No WiFi
  credential is ever compiled into an image — CI artefacts of this public repo
  are downloadable.
- `dependencies.lock` is committed; `managed_components/` is not.

## Tools

- Build container: `espressif/idf:v6.0.2`.
- Testbench: `http://192.168.8.70:8080`; UDP log port 5555; console via the
  USB-UART adapter slot (SLOT3, RX/TX only); images by OTA, cold flash only
  through the XIAO's USB-C moved to the Pi (FSD §2.2).
