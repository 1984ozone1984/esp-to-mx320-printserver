"""JRN-01 — the journey, the standard run (testing/test-plan.yaml), home-LAN variant.

Workflow shape: one test per step, run in file order; the order is the claim.
State passes between steps through `J`. A step whose predecessor did not
complete is skipped as `not done: blocked by JRN-01.x` — never failed, so a red
journey always names the step where it stops.

Steps whose feature belongs to a later FSD phase are xfail until it lands; an
XPASS is the landing signal and the marker is removed.

Recovery after partial execution: the journey always starts at JRN-01.1, a
bench cold flash with erase (FSD §2.2) followed by a power-cycle, which resets
NVS, both app slots and the running image. Nothing earlier needs undoing.

Inputs: --coldflash-dir / COLDFLASH_DIR (the image under test);
--expect-version / EXPECT_VERSION (defaults to the image's own version);
HOME_WIFI_SSID / HOME_WIFI_PASS; HA_MQTT_HOST / HA_MQTT_USER / HA_MQTT_PASS.
"""
import json
import os
import socket
import time

import pytest

from . import dut

pytestmark = pytest.mark.journey

PHASE2 = pytest.mark.xfail(reason="FSD Phase 2 (USB printer link) not yet in the firmware", strict=False)
PHASE3 = pytest.mark.xfail(reason="FSD Phase 3 (RAW port, discovery) not yet in the firmware", strict=False)
PHASE4 = pytest.mark.xfail(reason="FSD Phase 4 (portal, MQTT) not yet in the firmware", strict=False)

BOOT = r"boot: fw=(\S+) reset=(\S+) count=(\d+) flash=(\d+) psram=(\d+)"
TEST_BUILD_MARKER = b"192.168.8.70"      # FSD §19 udp_log_host of the test build (as CI checks)

J = {"done": set()}                      # journey state shared between steps


def after(step):
    """Skip (not done) unless the named step completed in this run."""
    if step not in J["done"]:
        pytest.skip(f"not done: blocked by {step}")


def _app_image(coldflash_dir):
    with open(os.path.join(coldflash_dir, "flash_args")) as f:
        for line in f:
            tok = line.split()
            if len(tok) == 2 and tok[0] == "0x20000":
                with open(os.path.join(coldflash_dir, os.path.basename(tok[1])), "rb") as img:
                    return img.read()
    raise AssertionError("flash_args names no image at 0x20000")


# --- JRN-01.1 ---------------------------------------------------------------

def test_jrn_01_1_flash_and_boot_unprovisioned(bench, coldflash_dir, request):
    """NVS-erased image boots: `boot: fw=<expected>` then `state: PROVISIONING` (NFR-2.4, FR-20.1)."""
    app = _app_image(coldflash_dir)
    J["test_build"] = TEST_BUILD_MARKER in app
    J["version"] = request.config.getoption("--expect-version") or dut.app_version(app)

    dut.operator("JRN-01.1: unplug the XIAO USB-C, then plug it into the Pi WHILE HOLDING BOOT; "
                 "release BOOT once plugged in. Click DONE when done.")
    dut.wait_cold_flash_slot()
    dut.cold_flash(coldflash_dir, erase=True)

    # Power-cycle in two prompts, so the XIAO is surely unpowered between them
    # (2026-11-03: a single "unplug, wait, replug" prompt left the chip running).
    dut.operator("JRN-01.1: unplug the XIAO USB-C from the Pi and LEAVE IT UNPLUGGED. "
                 "Click DONE while it is still unplugged.")
    t = dut.bench_now()   # after the flash's own resets: only the power-cycle boot counts
    dut.operator("JRN-01.1: plug the XIAO USB-C back into the Pi (do NOT hold BOOT). Click DONE when done.")
    b = dut.wait_line(BOOT, t, 20)
    assert b, "no `boot:` marker on the UART after the power-cycle"
    assert b[1].group(1) == J["version"], f"boot fw={b[1].group(1)}, expected {J['version']}"
    s = dut.wait_line(r"state: (PROVISIONING|CONNECTING)", b[0] - 0.001, 10)
    assert s, "no `state:` line after the boot marker"
    assert s[1].group(1) == "PROVISIONING", "erased image did not start PROVISIONING"
    J["boot_ts"] = b[0]
    J["done"].add("JRN-01.1")


# --- JRN-01.2 ---------------------------------------------------------------

@PHASE4
def test_jrn_01_2_provision_through_portal(bench):
    """FR-2.10, FR-12.1: bench joins MX320-Setup-<id>, submits home WLAN + admin; SoftAP gone ≤ 5 s."""
    after("JRN-01.1")
    ssid, pw = dut.home_wifi()
    # Bench WiFi API shapes (scan, enter-portal) are to be confirmed against the
    # testbench-wifi skill when Phase 4 starts; until then this step is xfail.
    scan = dut._bench("GET", "/api/wifi/scan", timeout=30)
    setup = [n for n in scan.get("networks", []) if str(n.get("ssid", "")).startswith("MX320-Setup-")]
    assert setup, "no MX320-Setup-<id> SoftAP in the bench scan"
    sap = setup[0]["ssid"]
    j = dut._bench("POST", "/api/enter-portal", {
        "ssid": sap,
        "form": {"wifi_ssid": ssid, "wifi_pass": pw, "admin_pass": dut.ADMIN_PASS},
    }, timeout=90)
    assert j.get("ok"), f"portal submission failed: {j}"
    t = dut.bench_now()
    assert dut.wait_line(r"portal: test-join ok", t - 30, 30), "no `portal: test-join ok`"
    time.sleep(5)
    scan = dut._bench("GET", "/api/wifi/scan", timeout=30)
    assert sap not in [n.get("ssid") for n in scan.get("networks", [])], "SoftAP still up 5 s after the test-join"
    J["provisioned_by"] = "portal"
    J["done"].add("JRN-01.2")


# --- JRN-01.3 ---------------------------------------------------------------

def test_jrn_01_3_operational_on_home_wlan(bench):
    """FR-2.11, FR-2.12: `wifi: got ip <ip>`; /api/status from the test host says OPERATIONAL."""
    after("JRN-01.1")
    t = dut.bench_now()
    if "JRN-01.2" not in J["done"]:
        # Stand-in until the portal (FSD Phase 4) lands: the test build's console
        # stores the same credentials (FR-19.1/19.3, P-BASELINE). A release image
        # has no console, so without the portal the journey stops here.
        if not J.get("test_build"):
            pytest.skip("not done: blocked by JRN-01.2 (release image, no portal yet)")
        ssid, pw = dut.home_wifi()
        t = dut.write(f"cred {ssid} {pw}")
        assert dut.wait_line(r"cred: stored", t, 5), "no `cred: stored`"
        t = dut.write(f"admin {dut.ADMIN_PASS}")
        assert dut.wait_line(r"admin: stored", t, 5), "no `admin: stored`"
        t = dut.write("reboot")
        J["provisioned_by"] = "console (stand-in for JRN-01.2)"
    g = dut.wait_line(r"wifi: got ip (\d+\.\d+\.\d+\.\d+)", t, 60)
    assert g, "no `wifi: got ip <ip>` within 60 s"
    ip = g[1].group(1)
    st = None
    for _ in range(10):                  # FR-2.12: services up ≤ 5 s after the IP
        try:
            st = dut.status(ip)
            break
        except Exception:
            time.sleep(1)
    assert st, f"GET http://{ip}/api/status never answered"
    assert st["state"] == "OPERATIONAL", f"state {st['state']}"
    assert st["fw"] == J["version"], f"/api/status fw={st['fw']}, expected {J['version']}"
    J["ip"] = ip
    J["done"].add("JRN-01.3")


# --- JRN-01.4 ---------------------------------------------------------------

@PHASE2
def test_jrn_01_4_printer_claimed(bench):
    """FR-8.1, FR-8.2, FR-8.9: printer.present, mdl = the MX320's 1284 MDL, status ready."""
    after("JRN-01.3")
    p = None
    for _ in range(15):
        p = dut.status(J["ip"]).get("printer") or {}
        if p.get("present"):
            break
        time.sleep(2)
    assert p and p.get("present") is True, f"printer not present: {p}"
    assert p.get("mdl") == "MX320 series", f"mdl {p.get('mdl')!r}"
    assert p.get("status") == "ready", f"printer status {p.get('status')!r}"
    J["mdl"] = p["mdl"]
    J["done"].add("JRN-01.4")


# --- JRN-01.5 ---------------------------------------------------------------

@PHASE3
def test_jrn_01_5_discoverable_via_mdns(bench):
    """FR-10.1, FR-10.2: _pdl-datastream._tcp on 9100 with usb_MDL; <hostname>.local -> DUT IP."""
    after("JRN-01.4")
    from zeroconf import ServiceBrowser, ServiceListener, Zeroconf

    st = dut.status(J["ip"])
    hostname = st.get("hostname") or st.get("config", {}).get("hostname")
    assert hostname, "/api/status names no hostname"
    found = {}

    class L(ServiceListener):
        def add_service(self, zc, type_, name):
            info = zc.get_service_info(type_, name, timeout=3000)
            if info:
                found[name] = info

        update_service = add_service

        def remove_service(self, zc, type_, name):
            pass

    zc = Zeroconf()
    try:
        ServiceBrowser(zc, "_pdl-datastream._tcp.local.", L())
        time.sleep(6)
        mine = [i for i in found.values() if J["ip"] in i.parsed_addresses()]
        assert mine, f"no _pdl-datastream._tcp instance at {J['ip']} (saw {list(found)})"
        info = mine[0]
        assert info.port == 9100, f"port {info.port}"
        mdl = (info.properties or {}).get(b"usb_MDL", b"").decode()
        assert mdl == J["mdl"], f"TXT usb_MDL {mdl!r}, claimed {J['mdl']!r}"
    finally:
        zc.close()
    resolved = socket.gethostbyname(f"{hostname}.local")
    assert resolved == J["ip"], f"{hostname}.local -> {resolved}, DUT is {J['ip']}"
    J["hostname"] = hostname
    J["done"].add("JRN-01.5")


# --- JRN-01.6 ---------------------------------------------------------------

JOB_FILE = os.path.join(os.path.dirname(__file__), "..", "data", "mx320-one-page.prn")


@PHASE3
def test_jrn_01_6_print_one_page(bench):
    """FR-9.1, FR-6.1, FR-6.4, FR-6.9: job to :9100 closed; `job: done bytes=<size>`; completed +1; page printed."""
    after("JRN-01.5")
    if not os.path.exists(JOB_FILE):
        pytest.skip("not done: mx320-job-file (tests/data/mx320-one-page.prn) not committed yet")
    with open(JOB_FILE, "rb") as f:
        job = f.read()
    before = dut.status(J["ip"])["job"]["completed"]
    t = dut.bench_now()
    with socket.create_connection((J["ip"], 9100), timeout=10) as s:
        s.sendall(job)
    d = dut.wait_line(r"job: done id=(\d+) bytes=(\d+)", t, 60)
    assert d, "no `job: done` marker"
    assert int(d[1].group(2)) == len(job), f"job: done bytes={d[1].group(2)}, file is {len(job)}"
    after_n = dut.status(J["ip"])["job"]["completed"]
    assert after_n == before + 1, f"job.completed {before} -> {after_n}"
    try:
        dut.operator("JRN-01.6: did the MX320 print the known test page? Click DONE if yes, CANCEL if not.")
    except dut.BenchUnavailable:
        pytest.fail("operator: the known page did not print")
    J["completed"] = after_n
    J["done"].add("JRN-01.6")


# --- JRN-01.7 ---------------------------------------------------------------

@PHASE4
def test_jrn_01_7_status_reaches_ha_broker(bench):
    """FR-13.1, FR-13.3: retained availability = online; state's job.completed = after JRN-01.6."""
    after("JRN-01.6")
    import paho.mqtt.client as mqtt

    host, user, pw = (os.environ.get(k) for k in ("HA_MQTT_HOST", "HA_MQTT_USER", "HA_MQTT_PASS"))
    if not host:
        pytest.skip("not done: HA_MQTT_HOST not in the environment")
    dev_id = dut.status(J["ip"])["id"]
    base = f"mx320/{dev_id}"
    got = {}
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if user:
        c.username_pw_set(user, pw)
    c.on_message = lambda cl, ud, m: got.__setitem__(m.topic, (m.retain, m.payload))
    c.connect(host, 1883, 30)
    c.subscribe([(f"{base}/availability", 1), (f"{base}/state", 1)])
    c.loop_start()
    try:
        end = time.time() + 10
        while time.time() < end and len(got) < 2:
            time.sleep(0.2)
    finally:
        c.loop_stop()
        c.disconnect()
    av = got.get(f"{base}/availability")
    assert av and av[0] and av[1] == b"online", f"availability: {av}"
    stp = got.get(f"{base}/state")
    assert stp and stp[0], f"no retained {base}/state"
    assert json.loads(stp[1])["job"]["completed"] == J["completed"], "state job.completed differs from JRN-01.6"
    J["done"].add("JRN-01.7")
