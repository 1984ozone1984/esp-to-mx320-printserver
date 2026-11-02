"""Bench-tier fixtures. An unreachable bench or missing precondition is `not done`
(skipped with the reason), never `failed` (testing.md)."""
import pytest

from . import dut


@pytest.fixture(scope="session")
def bench():
    try:
        return dut.bench_info()
    except dut.BenchUnavailable as e:
        pytest.skip(f"not done: testbench unreachable ({e})")


@pytest.fixture
def baseline_ip(bench):
    """P-BASELINE: test build on the home WLAN, admin password set. Yields the IP."""
    try:
        ip = dut.baseline()
    except dut.BenchUnavailable as e:
        pytest.skip(f"not done: {e}")
    yield ip


@pytest.fixture
def coldflash_dir(request):
    d = request.config.getoption("--coldflash-dir")
    if not d:
        pytest.skip("not done: --coldflash-dir / COLDFLASH_DIR not given")
    return d


@pytest.fixture
def ota_image(request):
    import os
    p = os.environ.get("OTA_IMAGE")
    if not p:
        pytest.skip("not done: OTA_IMAGE (a CI firmware_*.bin of the test build) not given")
    with open(p, "rb") as f:
        return f.read()
