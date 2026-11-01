# Gate — AI harnessed (closes Phase 1, Harness)

**Standing exclusion.** Evidence is committed artefacts in this repository, CI results, and live testbench answers only — never memory, transcripts, `~/.claude`, or the author's account of what was done. Something not found in the repository is not there; that is a finding.

**Verdict format.** `OPEN`, or `SHUT` followed by one line per finding: the requirement row, the evidence looked for, and what was found instead. Return a verdict, never a fix — do not edit files.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Test plan exists with bench capabilities `yes`/`no` as the bench declares and project-side ones `unproven` (or `no` with consequence) | `testing/test-plan.yaml` | `bench-*` entries; `dut-on-slot`, `dut-boot-en`, `usb-host-hub-power`, `printer-mx320`, `printer-port-status`, `printer-detach`, `mx320-job-file`, `lan-clients`, `home-wlan`, `ha-broker`, `operator`, `release-verification` = unproven; `usb-printer-sink` = no with consequence |
| 2 | Journey seeded, one test per step | same | JRN-01.1 … JRN-01.7, `shape: workflow`, `kind: standard`, `wave: 1`, each `verifies:` naming IDs that exist in the FSD, `impl: null`, `status: not done` |
| 3 | Debugging agenda matches the unproven/no project capabilities | `testing/debugging-agenda.md` | 12 rows; every project-side capability named by at least one row |
| 4 | Testing standard is one file with rules + shared procedures | `docs/Method/standards/testing.md` | "Rules that hold for every run", P-PROVISION, P-SINK-JOB |
| 5 | Firmware contains only the skeleton + UDP log | `main/`, `components/` | Only `components/udp_log`; `main/app_main.c` calls netif/event init, optional `udp_log_init`, logs `Init complete`. No WiFi, USB host, HTTP, MQTT, mDNS, NVS or OTA code or REQUIRES yet (those are /build's waves) |
| 6 | Build constraints match FSD §23 | `sdkconfig.defaults`, `partitions.csv`, `CMakeLists.txt` | 8 MB flash, octal PSRAM, UART console, no secondary console, partition offsets 0x9000/0xF000/0x11000/0x20000/0x320000, cmake ≥ 3.22 |
| 7 | CI: build on push, release on tag, verify job gating release | `.github/workflows/build.yml`, `ci.yml` | `verify` on `[self-hosted, testbench]`; `release` needs `verify`; production image must not contain `192.168.8.70` |
| 8 | CI has run green once on main | GitHub `1984ozone1984/esp-to-mx320-printserver` | Command below |
| 9 | Devcontainer with toolchain and runner installed, registration awaiting the owner | `.devcontainer/` | Dockerfile FROM `espressif/idf:v6.0.2`; `runner/register.sh` and `start.sh` name this repo; nothing auto-registers |
| 10 | DUT untouched in Phase 1 | test plan | `dut.slot: null`, `dut.mac: null`; no capability carrying evidence that could only come from the board |

## Mechanical checks

```bash
test -f testing/test-plan.yaml && test -f testing/debugging-agenda.md
ls .github/workflows/build.yml .github/workflows/ci.yml
grep -q 'runs-on: \[self-hosted, testbench\]' .github/workflows/build.yml
grep -A2 '^  release:' .github/workflows/build.yml | grep -q verify
gh run list -R 1984ozone1984/esp-to-mx320-printserver -L 5 --branch main --json name,conclusion   # both workflows success at least once
grep -rlnE 'esp_wifi|usb_host|esp_http|mqtt|mdns|nvs_flash|esp_ota' main components && exit 1 || true
grep -q 'Init complete' main/app_main.c
grep -q 'helloworld' -r .devcontainer && exit 1 || true
```

## Judgement checks

- Open a fresh `/build` session with only the repo. Can it state the project's position (phase, wave, next act) without asking anything? Quote what it said.
- Does every `verifies:` ID in the plan exist in the FSD? List any that do not.

## Traps

- The devcontainer and runner scripts were derived from a sibling project; a leftover repo name would register the runner to the wrong repository.
- `.gitignore` must anchor `/build/` so it never swallows a directory named `build` elsewhere.
- `tests/host` holds only a tier-runs smoke test. A host test asserting product behaviour belongs to /build's waves.
- `release-verification` declared `yes` because the runner is *installed*: until a tag has run the verify job on the registered runner it is unproven.
- The verify job exits 5 (no journey tests collected) until JRN tests are written — correct, a release must not publish; do not mistake it for a CI fault.
