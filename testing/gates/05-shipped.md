# Gate — Shipped (closes the release)

**Standing exclusion.** Evidence is committed artefacts in this repository, CI results, and live testbench answers only — never memory, transcripts, `~/.claude`, or the author's account of what was done. Something not found in the repository is not there; that is a finding.

**Verdict format.** `OPEN`, or `SHUT` followed by one line per finding: the requirement row, the evidence looked for, and what was found instead. Return a verdict, never a fix — do not edit files.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Release published from a tagged commit | GitHub releases | `vX.Y.Z` release with `firmware_vX.Y.Z.bin` and `coldflash/` |
| 2 | Built in the pinned container | the tag's build run | `espressif/idf:v6.0.2` |
| 3 | Journey green on the released bytes | the tag's `verify` job | success, JRN-01.x passed against `artefact/coldflash` |

## Mechanical checks

```bash
gh release list -R 1984ozone1984/esp-to-mx320-printserver -L 1
gh run list -R 1984ozone1984/esp-to-mx320-printserver --workflow build.yml -L 3 --json headBranch,conclusion,event
```

## Traps

- A release whose verify job was skipped (no runner) but published anyway — `release` must need `verify`.
