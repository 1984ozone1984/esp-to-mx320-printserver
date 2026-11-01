# Gate — Ready for shipment (closes Phase 3, Build)

**Standing exclusion.** Evidence is committed artefacts in this repository, CI results, and live testbench answers only — never memory, transcripts, `~/.claude`, or the author's account of what was done. Something not found in the repository is not there; that is a finding.

**Verdict format.** `OPEN`, or `SHUT` followed by one line per finding: the requirement row, the evidence looked for, and what was found instead. Return a verdict, never a fix — do not edit files.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Every requirement met | plan `tests:` × FSD IDs | Every Must/Should ID in the FSD is in ≥ 1 `verifies:`; every such test `status: successful` at a commit ≥ the FSD's last change |
| 2 | Journey green | JRN-01.1–JRN-01.7 | all `successful`, same commit |
| 3 | Reconcile empty both ways | plan `impl:` vs `tests/` | No declared test with empty `impl`; no executable test no `impl` points at |
| 4 | Acceptance AT-1…AT-4 done | plan | Operator-judged entries `successful` with evidence |
| 5 | User manual current | `docs/UserDocumentation/User-Manual.md` | Status line not "nothing is deployable"; no "Pending" chapter |
| 6 | Release image clean | CI | Production build without `192.168.8.70`, no test endpoints (FR-12.7) |

## Mechanical checks

```bash
grep -c 'status: successful' testing/test-plan.yaml
grep -n 'impl: null' testing/test-plan.yaml          # expect no output
grep -n 'Pending' docs/UserDocumentation/User-Manual.md   # expect no output
```

## Judgement checks

- For three recovery requirements (FR-2.15, FR-6.6, FR-13.5), did the test assert the prohibited outcomes (no reboot, no FIN on abort, no stale state)? Quote the assertion.

## Traps

- A test "successful" against the real MX320 for a byte-exact contract while `usb-printer-sink` is `no` — the byte comparison never happened.
