# MX320 WiFi Print Server — User Manual

How to install and run the MX320 WiFi Print Server.

**Status: nothing is deployable yet.** No firmware exists. The chapters below
describe the intended procedures and become real as each phase ships (FSD §3).

## Contents

1. What you need
2. Wiring
3. First setup (captive portal)
4. Adding the printer on a Mac
5. Adding the printer on Windows
6. Status page and Home Assistant
7. Updating firmware
8. Troubleshooting
9. Recovery

## 1. What you need

- Canon PIXMA MX320, Seeed Studio XIAO ESP32-S3, an active (self-powered) USB
  hub, USB cables.
- A 2.4 GHz WLAN.
- On a Mac: an MX320 driver (Gutenprint "Canon PIXMA MX320"). On Windows: the
  Canon MX320 driver from Canon's support site.

## 2. Wiring

XIAO USB-C → hub upstream port; MX320 → hub downstream port; hub power supply
→ mains. The VBUS/power arrangement is confirmed during commissioning; this
chapter will carry the exact wiring once it is.

## 3. First setup (captive portal)

Pending (Phase 4).

## 4. Adding the printer on a Mac

Pending (Phase 3). Intended path: System Settings → Printers → Add → the
printer appears via Bonjour; choose the MX320 driver.

## 5. Adding the printer on Windows

Pending (Phase 3). Intended path: Add printer → TCP/IP → `mx320-<id>.local` or
the IP, port 9100, protocol RAW → Canon MX320 driver; disable bidirectional
support on the port.

## 6. Status page and Home Assistant

Pending (Phases 1–4). `http://mx320-<id>.local/`.

## 7. Updating firmware

Pending (Phase 1).

## 8. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|

## 9. Recovery

Pending. Factory reset: hold the XIAO BOOT button for 10 s.

---

**Contract:** `docs/Functionality/FSD.md` · **Build rules:** `docs/Method/`
