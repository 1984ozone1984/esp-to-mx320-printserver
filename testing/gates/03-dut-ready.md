# Gate — DUT ready (closes Phase 2, Commissioning)

**Standing exclusion.** Evidence is committed artefacts in this repository, CI results, and live testbench answers only — never memory, transcripts, `~/.claude`, or the author's account of what was done. Something not found in the repository is not there; that is a finding.

**Verdict format.** `OPEN`, or `SHUT` followed by one line per finding: the requirement row, the evidence looked for, and what was found instead. Return a verdict, never a fix — do not edit files.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Testbench record current | `testing/test-plan.yaml` `testbench:` | `last_seen` dated in this phase; URL answers `/api/info` now |
| 2 | DUT record matches the FSD's unit | `dut:` | slot, MAC, chip ESP32-S3, flash 8 MB, PSRAM 8 MB from a dated `/api/chip/info` observation (FSD NFR-2.1–2.3) |
| 3 | Forward path proven | `dut-on-slot.observed` | A named CI run → artefact → flashed at its `flash_args` offsets → `Init complete` read on the UART |
| 4 | Project-side capabilities resolved | `capabilities:` | No `unproven` left; each `no` states its consequence; `printer-port-status` outcome reflected in the FSD (OD-3 closed) |
| 5 | Agenda empty of open items (incl. #12 release verification) | `testing/debugging-agenda.md` | Every row has a dated *Closed* entry |
| 6 | Assumptions A1–A3 and risks R1–R2 resolved in the FSD | FSD §4 | Each confirmed, or the FSD amended |

## Mechanical checks

```bash
curl -sf -m 10 $TESTBENCH_URL/api/info
grep -c 'available: unproven' testing/test-plan.yaml          # expect 0
awk -F'|' 'NR>8 && $8 ~ /^ *$/' testing/debugging-agenda.md   # expect no open rows
```

## Judgement checks

- Does the forward-path evidence name a specific CI run and the offsets actually in that run's `flash_args`? Quote it.
- Was the MX320 identity (`printer-mx320`) read by the ESP itself, not by another host?

## Traps

- The XIAO's USB-C now belongs to the printer: a slot showing a native-USB `303a:1001` device means the DUT is wired the old way (Phase 0 finding), not that the forward path works.
- A capability "proven" by reading an endpoint's name, not by observing it work.
