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

The adapter has no BOOT/EN lines, so it cannot flash. Images reach the DUT by
OTA (`POST /api/ota` on the DUT, admin password set with the test build's
`admin` command). A cold flash — bootloader, partition table, erase, recovery —
needs the operator to move the XIAO's USB-C to the Pi (FSD §2.2, bench cold
flash) and then `/api/flash` on that native-USB slot. Resets on a test build
are the console line `reboot`; NVS erase is `wipe` (FSD FR-19.5, FR-19.6).

Never set `CONFIG_PRINTSERVER_TEST_BUILD` in `sdkconfig.defaults`; CI fails a
production image that contains the test build's UDP log host.
