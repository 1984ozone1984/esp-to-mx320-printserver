# MX320 WiFi Print Server — Debugging agenda

Project-side parts that have never been observed doing their job on this bench.
`/commission` burns this list down. Each item matches a capability in
`testing/test-plan.yaml` marked `available: unproven` (or `no`); proving it flips
the capability to `yes`. Measurements belong on the capability, not in the plan's
tests. The testbench itself is not on this list.

| # | Part | Unproven because | Proven by | Capability | Closed |
|---|---|---|---|---|---|
| 1 | XIAO ESP32-S3 on a bench slot through a USB-UART adapter | This unit has never been on this bench, and never through UART0 | The skeleton is flashed through `/api/flash` from a CI artefact's `flash_args`, and `Init complete` is read on the UART console after a reset | `dut-on-slot` | |
| 2 | BOOT/EN lines (bench GPIO18/17 → XIAO GPIO0/EN) | The XIAO's EN is not on its castellations; the wiring does not exist yet | `/api/serial/gpio-test` reports `boot_wired` and `en_wired`; a GPIO-driven reset produces a fresh boot on the UART | `dut-boot-en` | |
| 3 | Active hub powered with the ESP as its upstream host | The XIAO cannot source VBUS (FSD R1); whether the hub enables downstream ports this way is unknown | A downstream device is powered and the ESP's USB host logs its attach | `usb-host-hub-power` | |
| 4 | MX320 enumerating as a printer behind the hub | Class/subclass and ESP-IDF hub support are assumed (FSD A1, R2) | The ESP reads a class 07/01 interface and a 1284 Device ID from the MX320 | `printer-mx320` | |
| 5 | MX320 port-status bits | Canon may not maintain them (FSD R3, OD-3) | The PaperEmpty bit changes when paper is removed (operator-confirmed) | `printer-port-status` | |
| 6 | USB printer-class sink | Does not exist (FSD OD-4) | It enumerates as a printer to the ESP, records bulk-OUT bytes whose SHA-256 matches a sent file, and pauses on command | `usb-printer-sink` | |
| 7 | Printer detach / re-attach on demand | No switchable port; only an operator can unplug | An operator-prompted unplug and re-plug is seen by the ESP each time | `printer-detach` | |
| 8 | Mac and Windows clients with MX320 drivers | Driver availability on current macOS is unconfirmed (FSD R4) | Each prints a page on the MX320 over a direct USB cable; one job is saved to file as `mx320-job-file` | `lan-clients`, `mx320-job-file` | |
| 9 | Operator at the bench | Nobody has answered a bench prompt for this project | A portal prompt is answered yes and no, and both answers reach the caller | `operator` | |
| 10 | Home Assistant MQTT broker | Not yet reached from the devcontainer or the home WLAN | A test publish from the devcontainer appears in HA's MQTT listener | `ha-broker` | |
| 11 | Home WLAN credentials in the devcontainer, shared L2 with the DUT | Derived from the home-LAN journey; not yet set up | `HOME_WIFI_SSID`/`HOME_WIFI_PASS` present in the container env; an mDNS browse from the devcontainer sees another LAN device | `home-wlan` | |
