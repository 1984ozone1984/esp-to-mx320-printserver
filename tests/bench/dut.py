"""Bench helpers for the MX320 print server (FSD §2.2, §25.0; testing.md P-BASELINE).

The DUT console is the UART adapter slot (found by USB id, or DUT_SLOT). There is no
BOOT/EN wiring: resets are the test build's `reboot` line, NVS erase is `wipe`.
Markers are matched on the bench's recorded serial buffer (not a concurrent
monitor, which duplicates lines), and their times are the bench's timestamps.
"""
import json
import os
import re
import struct
import time
import urllib.error
import urllib.request

BENCH = os.environ.get("TESTBENCH_URL", "http://192.168.8.70:8080")
# Slot labels are not stable across a bench reboot (2026-11-03: the UART
# adapter moved SLOT3 -> SLOT2, the XIAO's USB-C SLOT4 -> SLOT3), so slots are
# found by USB identity unless the environment names them.
CONSOLE_VIDPID = "0403:6001"      # FT232R USB-UART adapter (console)
COLD_FLASH_VIDPID = "303a:1001"   # XIAO native USB, only while its USB-C is on the Pi
_ENV_CONSOLE = os.environ.get("DUT_SLOT")
_ENV_COLD = os.environ.get("COLD_FLASH_SLOT")
ADMIN_PASS = os.environ.get("MX320_TEST_ADMIN", "testpass1")


class BenchUnavailable(Exception):
    """An unmet precondition: the test is `not done`, never `failed`."""


def _bench(method, path, body=None, timeout=15):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(BENCH + path, data=data, method=method,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read())
    except (urllib.error.URLError, OSError) as e:
        raise BenchUnavailable(f"{method} {path}: {e}") from e


def bench_info():
    return _bench("GET", "/api/info", timeout=5)


def _slot_by_vidpid(vidpid):
    for sl in _bench("GET", "/api/devices", timeout=30)["slots"]:
        if sl.get("present") and any(d.get("vid_pid") == vidpid for d in sl.get("usb_devices") or []):
            return sl["label"]
    return None


_console = None


def console_slot():
    global _console
    if _ENV_CONSOLE:
        return _ENV_CONSOLE
    if _console is None:
        _console = _slot_by_vidpid(CONSOLE_VIDPID)
        if _console is None:
            raise BenchUnavailable(f"no slot with the UART adapter ({CONSOLE_VIDPID})")
    return _console


def cold_flash_slot():
    """The XIAO's native-USB slot, or None while its USB-C is not on the Pi."""
    return _ENV_COLD or _slot_by_vidpid(COLD_FLASH_VIDPID)


# --- console -----------------------------------------------------------------

def console_lines(since=0.0):
    """Recorded lines of the console slot after bench time `since`."""
    r = _bench("GET", f"/api/serial/output?slot={console_slot()}&lines=500&since={since}")
    return r.get("lines", [])


def bench_now():
    """Bench-clock reference: the newest recorded line's time (or 0)."""
    lines = console_lines()
    return max((l["ts"] for l in lines), default=0.0)


def write(cmd):
    """Write one console line; returns the bench-clock time to search from."""
    t0 = bench_now()
    # The slot's RFC2217 port is single-client and intermittently busy; the bench
    # retries for up to ~19 s per call (test-plan bench-serial-write limitation).
    for attempt in range(3):
        r = _bench("POST", "/api/serial/write", {"slot": console_slot(), "text": cmd}, timeout=45)
        if r.get("ok"):
            return t0
        if "cannot reach the proxy" not in str(r.get("error", "")):
            break
        time.sleep(2)
    raise BenchUnavailable(f"serial write failed: {r}")


def wait_line(pattern, since, timeout=15.0):
    """First recorded line after `since` matching `pattern` -> (ts, match) or None."""
    rx = re.compile(pattern)
    end = time.monotonic() + timeout
    seen = set()
    while time.monotonic() < end:
        for l in console_lines(since):
            key = (l["ts"], l["text"])
            if key in seen:
                continue
            seen.add(key)
            m = rx.search(l["text"])
            if m:
                return l["ts"], m
        time.sleep(0.3)
    return None


def lines_matching(pattern, since):
    rx = re.compile(pattern)
    return [(l["ts"], rx.search(l["text"])) for l in console_lines(since) if rx.search(l["text"])]


# --- device HTTP (home WLAN, reached directly from the test host) -------------

def http(ip, path, method="GET", body=None, auth=None, timeout=10, ctype=None):
    req = urllib.request.Request(f"http://{ip}{path}", data=body, method=method)
    if ctype:
        req.add_header("Content-Type", ctype)
    if auth:
        import base64
        req.add_header("Authorization", "Basic " + base64.b64encode(
            f"{auth[0]}:{auth[1]}".encode()).decode())
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def status(ip):
    code, body = http(ip, "/api/status")
    assert code == 200, f"/api/status answered {code}"
    return json.loads(body)


# --- baseline (testing.md P-BASELINE) ----------------------------------------

def home_wifi():
    ssid, pw = os.environ.get("HOME_WIFI_SSID"), os.environ.get("HOME_WIFI_PASS")
    if not ssid or not pw:
        raise BenchUnavailable("HOME_WIFI_SSID / HOME_WIFI_PASS not in the environment")
    return ssid, pw


def baseline(timeout=60.0):
    """wipe → cred (home WLAN) → admin → reboot → OPERATIONAL. Returns the DUT IP."""
    ssid, pw = home_wifi()
    t = write("wipe")
    if not wait_line(r"state: PROVISIONING", t, 20):
        raise BenchUnavailable("baseline: no `state: PROVISIONING` after `wipe` "
                               "(firmware without FR-19.6, or console not answering)")
    t = write(f"cred {ssid} {pw}")
    assert wait_line(r"cred: stored", t, 5), "baseline: no `cred: stored`"
    t = write(f"admin {ADMIN_PASS}")
    assert wait_line(r"admin: stored", t, 5), "baseline: no `admin: stored`"
    t = write("reboot")
    hit = wait_line(r"wifi: got ip (\d+\.\d+\.\d+\.\d+)", t, timeout)
    assert hit, "baseline: no `wifi: got ip` after reboot"
    return hit[1].group(1)


# --- images ------------------------------------------------------------------

def app_version(image: bytes) -> str:
    """esp_app_desc_t.version from an app image (desc at 0x20, version at +0x10)."""
    magic = struct.unpack_from("<I", image, 0x20)[0]
    assert magic == 0xABCD5432, "not an ESP-IDF app image"
    return image[0x30:0x50].split(b"\0", 1)[0].decode()


def ota(ip, image: bytes, auth=("admin", ADMIN_PASS), timeout=120):
    """POST /api/ota as multipart field `image` (FSD FR-11.3, curl -F image=@…)."""
    boundary = "mx320ota" + os.urandom(8).hex()
    body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"image\"; "
            f"filename=\"fw.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n").encode() \
        + image + f"\r\n--{boundary}--\r\n".encode()
    return http(ip, "/api/ota", "POST", body, auth=auth, timeout=timeout,
                ctype=f"multipart/form-data; boundary={boundary}")


# --- bench cold flash (FSD §2.2) ------------------------------------------------

def operator(message, timeout=300):
    r = _bench("POST", "/api/human-interaction", {"message": message, "timeout": timeout},
               timeout=timeout + 10)
    if not r.get("confirmed"):
        raise BenchUnavailable(f"operator declined: {message}")


def wait_cold_flash_slot(timeout=60):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        sl = cold_flash_slot()
        if sl and (not _ENV_COLD or any(
                x["label"] == sl and x.get("present")
                for x in _bench("GET", "/api/devices", timeout=30)["slots"])):
            return sl
        time.sleep(1)
    raise BenchUnavailable("the XIAO's native USB did not appear (USB-C not on the Pi?)")


def chip_info():
    return _bench("POST", "/api/chip/info", {"slot": cold_flash_slot()}, timeout=90)


def cold_flash(coldflash_dir, erase=True):
    """Flash the images named in flash_args at their offsets through the native-USB
    slot; erase = the app padded with 0xFF to the full ota_0 slot, and 0xFF images
    over nvs and ota_1 (FSD 0.1.29 §2.2, §23)."""
    import uuid
    parts = []
    with open(os.path.join(coldflash_dir, "flash_args")) as f:
        for line in f:
            tok = line.split()
            if len(tok) == 2 and tok[0].startswith("0x"):
                name = os.path.basename(tok[1])
                with open(os.path.join(coldflash_dir, name), "rb") as img:
                    parts.append((tok[0], name, img.read()))
    if erase:
        parts = [(off, n, d + b"\xff" * (0x300000 - len(d)) if off == "0x20000" else d)
                 for off, n, d in parts]
        parts.append(("0x9000", "nvs_erase.bin", b"\xff" * 0x6000))
        parts.append(("0x320000", "ota1_erase.bin", b"\xff" * 0x300000))
    boundary = "mx320cf" + uuid.uuid4().hex
    body = b""
    for k, v in (("slot", cold_flash_slot()), ("chip", "esp32s3"), ("baud", "460800")):
        body += (f"--{boundary}\r\nContent-Disposition: form-data; name=\"{k}\"\r\n\r\n{v}\r\n").encode()
    for off, name, data in parts:
        body += (f"--{boundary}\r\nContent-Disposition: form-data; name=\"bin@{off}\"; "
                 f"filename=\"{name}\"\r\nContent-Type: application/octet-stream\r\n\r\n").encode()
        body += data + b"\r\n"
    body += f"--{boundary}--\r\n".encode()
    req = urllib.request.Request(BENCH + "/api/flash", data=body, method="POST",
                                 headers={"Content-Type": f"multipart/form-data; boundary={boundary}"})
    with urllib.request.urlopen(req, timeout=600) as r:
        res = json.loads(r.read())
    assert res.get("ok"), f"cold flash failed: {res.get('error')}"
    return parts
