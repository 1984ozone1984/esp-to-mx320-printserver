"""FSD Phase 1, Wave 1 — standard bench tests (testing/test-plan.yaml).

Each test is xfail until its feature lands; an XPASS is the landing signal and
the marker is then removed. Times are bench-clock timestamps of console lines.
"""
import hashlib
import json
import re
import time

import pytest

from . import dut

FEATURE_ABSENT = pytest.mark.xfail(reason="Phase 1 feature not yet in the firmware", strict=False)
BOOT = r"boot: fw=(\S+) reset=(\S+) count=(\d+) flash=(\d+) psram=(\d+)"


@FEATURE_ABSENT
def test_tc_plat_01_boot_marker(baseline_ip):
    """NFR-2.3, NFR-2.4, FR-20.1: boot marker on the UART after `reboot`."""
    t = dut.write("reboot")
    hit = dut.wait_line(BOOT, t, 5)
    assert hit, "no `boot:` marker on the UART within 5 s of `reboot`"
    _, m = hit
    assert m.group(2) == "SW"
    assert (m.group(4), m.group(5)) == ("8", "8"), f"flash={m.group(4)} psram={m.group(5)}"


@FEATURE_ABSENT
@pytest.mark.slow
def test_tc_plat_02_chip_read_during_cold_flash(bench, coldflash_dir):
    """NFR-2.1, NFR-2.2: chip read on the native-USB slot during a bench cold flash."""
    dut.operator("TC-PLAT-02: unplug the XIAO USB-C, then plug it into the Pi WHILE HOLDING BOOT; "
                 "release BOOT once plugged in. Click DONE when done.")
    dut.wait_cold_flash_slot()
    info = dut.chip_info()
    assert info.get("ok"), info
    assert info["chip"].startswith("ESP32-S3"), info["chip"]
    assert info["flash_size"] == "8MB", info["flash_size"]
    assert info["mac"] == "dc:da:0c:57:57:a0", info["mac"]
    dut.cold_flash(coldflash_dir, erase=True)


@FEATURE_ABSENT
def test_tc_con_01_cred(bench):
    """FR-19.1: `cred` stores credentials; after reboot CONNECTING then got ip."""
    ssid, pw = dut.home_wifi()
    t = dut.write("wipe")
    assert dut.wait_line(r"state: PROVISIONING", t, 20), "precondition: `wipe` did not reach PROVISIONING"
    t = dut.write(f"cred {ssid} {pw}")
    assert dut.wait_line(r"cred: stored", t, 5), "no `cred: stored`"
    t = dut.write("reboot")
    c = dut.wait_line(r"state: (CONNECTING|PROVISIONING)", t, 15)
    assert c and c[1].group(1) == "CONNECTING", "after reboot: not CONNECTING"
    assert dut.wait_line(r"wifi: got ip", c[0], 60), "no `wifi: got ip`"


@FEATURE_ABSENT
def test_tc_con_02_admin(baseline_ip, ota_image):
    """FR-19.3: `admin` stores the OTA password; OTA with it answers 200."""
    t = dut.write("admin testpass2")
    assert dut.wait_line(r"admin: stored", t, 5), "no `admin: stored`"
    code, body = dut.ota(baseline_ip, ota_image, auth=("admin", "testpass2"))
    assert code == 200, f"OTA with the stored password answered {code}: {body[:200]!r}"
    dut.wait_line(r"wifi: got ip", t, 60)   # let it come back before cleanup


@FEATURE_ABSENT
def test_tc_con_03_reboot(baseline_ip):
    """FR-19.5: `reboot` → `reboot: requested`, then within 5 s boot reset=SW count=n+1."""
    n = dut.status(baseline_ip)["boot_count"]
    t = dut.write("reboot")
    r = dut.wait_line(r"reboot: requested", t, 5)
    assert r, "no `reboot: requested`"
    b = dut.wait_line(BOOT, r[0], 5)
    assert b, "no `boot:` within 5 s of `reboot: requested`"
    assert b[1].group(2) == "SW", f"reset reason {b[1].group(2)}"
    assert int(b[1].group(3)) == n + 1, f"count {b[1].group(3)}, want {n + 1}"


@FEATURE_ABSENT
def test_tc_con_04_wipe(baseline_ip):
    """FR-19.6: `wipe` → `wipe: done`, within 5 s boot count=1 and PROVISIONING."""
    t = dut.write("wipe")
    w = dut.wait_line(r"wipe: done", t, 10)
    assert w, "no `wipe: done`"
    b = dut.wait_line(BOOT, w[0], 5)
    assert b and b[1].group(3) == "1", f"after wipe: {b[1].group(0) if b else 'no boot marker'}"
    s = dut.wait_line(r"state: (PROVISIONING|CONNECTING)", w[0], 5)
    assert s and s[1].group(1) == "PROVISIONING", "after wipe: not PROVISIONING"


@FEATURE_ABSENT
def test_tc_life_02_connecting_within_2s(baseline_ip):
    """FR-2.11: credentials stored · reset → CONNECTING ≤ 2 s after the boot marker."""
    t = dut.write("reboot")
    b = dut.wait_line(BOOT, t, 10)
    assert b, "no `boot:` marker"
    c = dut.wait_line(r"state: CONNECTING", b[0] - 0.001, 5)
    assert c, "no `state: CONNECTING`"
    assert c[0] - b[0] <= 2.0, f"CONNECTING {c[0] - b[0]:.2f} s after boot"
    assert not dut.lines_matching(r"SoftAP|state: PROVISIONING", b[0] - 0.001), "SoftAP started"


@FEATURE_ABSENT
def test_tc_life_03_http_within_5s_of_ip(baseline_ip):
    """FR-2.12 (Phase 1 case): /api/status 200 ≤ 5 s after `wifi: got ip`, never before.
    Host and bench clocks are assumed NTP-synced (≤ 0.5 s)."""
    ip = baseline_ip
    t = dut.write("reboot")
    deadline = time.time() + 60
    first_ok = None
    time.sleep(0.5)
    while time.time() < deadline and first_ok is None:
        try:
            code, _ = dut.http(ip, "/api/status", timeout=1)
            if code == 200:
                first_ok = time.time()
        except Exception:
            pass
        time.sleep(0.1)
    assert first_ok, "/api/status never answered after the reboot"
    g = dut.wait_line(r"wifi: got ip", t, 5)
    assert g, "no `wifi: got ip`"
    assert first_ok >= g[0] - 0.5, "HTTP answered before `wifi: got ip`"
    assert first_ok - g[0] <= 5.5, f"HTTP up {first_ok - g[0]:.2f} s after the IP"
    assert dut.wait_line(r"state: OPERATIONAL", t, 5), "no `state: OPERATIONAL`"


@FEATURE_ABSENT
def test_tc_stat_01_version_matches(baseline_ip):
    """FR-18.1 (banner + /api/status) and the Phase 1 status fields."""
    t = dut.write("reboot")
    b = dut.wait_line(BOOT, t, 10)
    assert b, "no `boot:` marker"
    g = dut.wait_line(r"wifi: got ip", b[0], 60)
    assert g
    time.sleep(1)
    st = dut.status(baseline_ip)
    for k in ("fw", "boot_count", "reset_reason", "state", "heap_free"):
        assert k in st, f"/api/status lacks {k}"
    assert st["fw"] == b[1].group(1), f"banner {b[1].group(1)} vs status {st['fw']}"


@FEATURE_ABSENT
def test_tc_ota_01_authenticated_ota(baseline_ip, ota_image):
    """FR-11.3: valid image + auth → 200, reboot ≤ 5 s after the reply, running the other slot."""
    ip = baseline_ip
    before = json.loads(dut.http(ip, "/api/slots")[1])
    t = dut.bench_now()
    code, body = dut.ota(ip, ota_image)
    replied = time.time()
    assert code == 200, f"OTA answered {code}: {body[:200]!r}"
    b = dut.wait_line(BOOT, t, 15)
    assert b, "no reboot after OTA"
    assert b[0] - replied <= 5.5, f"reboot {b[0] - replied:.2f} s after the reply"
    g = dut.wait_line(r"wifi: got ip (\S+)", b[0], 60)
    assert g
    ip = g[1].group(1)
    assert dut.status(ip)["fw"] == dut.app_version(ota_image)
    after = json.loads(dut.http(ip, "/api/slots")[1])
    assert after["running"] != before["running"], "still running the same slot"


@FEATURE_ABSENT
@pytest.mark.slow
def test_tc_ota_02_slots(bench, coldflash_dir, ota_image):
    """FR-11.10 (a) after a cold flash with erase, (b) after an OTA."""
    dut.operator("TC-OTA-02: unplug the XIAO USB-C, then plug it into the Pi WHILE HOLDING BOOT; "
                 "release BOOT once plugged in. Click DONE when done.")
    dut.wait_cold_flash_slot()
    parts = dut.cold_flash(coldflash_dir, erase=True)
    app = next(d for off, n, d in parts if off == "0x20000")
    ip = dut.baseline()
    t0 = time.time()
    code, body = dut.http(ip, "/api/slots", timeout=6)
    assert code == 200 and time.time() - t0 <= 5
    a = json.loads(body)
    assert a["running"] == "ota_0"
    assert a["ota_0_sha256"] == hashlib.sha256(app + b"\xff" * (0x300000 - len(app))).hexdigest()
    assert a["ota_1_sha256"] == hashlib.sha256(b"\xff" * 0x300000).hexdigest()
    t = dut.bench_now()
    assert dut.ota(ip, ota_image)[0] == 200
    g = dut.wait_line(r"wifi: got ip (\S+)", t, 60)
    assert g
    b = json.loads(dut.http(g[1].group(1), "/api/slots", timeout=6)[1])
    assert b["running"] == "ota_1"
    assert b["ota_1_sha256"] != a["ota_1_sha256"]
    assert b["ota_0_sha256"] == a["ota_0_sha256"]


@FEATURE_ABSENT
def test_tc_log_01_udp_log(baseline_ip):
    """FR-20.2: markers emitted while OPERATIONAL with the link up also reach /api/udplog."""
    dut._bench("DELETE", "/api/udplog")
    t = dut.write("admin testpass1")          # produces `admin: stored` while OPERATIONAL
    assert dut.wait_line(r"admin: stored", t, 5)
    time.sleep(2)
    lines = dut._bench("GET", "/api/udplog").get("lines", [])
    texts = [l.get("text", l.get("line", "")) for l in lines]
    assert any("admin: stored" in x for x in texts), "marker missing from the UDP log"
