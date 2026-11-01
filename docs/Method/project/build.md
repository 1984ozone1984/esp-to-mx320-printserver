# Build, flash and test commands

All builds run in the pinned container (`espressif/idf:v6.0.2`, `.devcontainer/`).

| Task | Command |
|---|---|
| Release build | `idf.py -B build -DSDKCONFIG=$PWD/build/sdkconfig build` |
| Test build (bench-only features, UDP log to the testbench) | `idf.py -B build-test -DSDKCONFIG=$PWD/build-test/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test.defaults" build` |
| Host tier | `cmake -S tests/host -B build-host && cmake --build build-host && ctest --test-dir build-host` |
| Target / bench tiers | `pytest` (needs `TESTBENCH_URL`, `COLDFLASH_DIR`; TestbenchDriver on `PYTHONPATH`, set by the devcontainer) |
| Journey only | `pytest -m journey` |

Flashing uses the offsets in `build/flash_args`, never a hand-written list. The
DUT's console is UART0 through the bench slot's USB-UART adapter; the XIAO's own
USB-C belongs to the printer (FSD §2.2).

Never set `CONFIG_PRINTSERVER_TEST_BUILD` in `sdkconfig.defaults`; CI fails a
production image that contains the test build's UDP log host.
