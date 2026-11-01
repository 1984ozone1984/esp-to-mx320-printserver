# Gate — Load defined (closes Phase 0, Definition)

**Standing exclusion.** Evidence is committed artefacts in this repository, CI results, and live testbench answers only — never memory, transcripts, `~/.claude`, or the author's account of what was done. Something not found in the repository is not there; that is a finding.

**Verdict format.** `OPEN`, or `SHUT` followed by one line per finding: the requirement row, the evidence looked for, and what was found instead. Return a verdict, never a fix — do not edit files.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Every Must/Should carries a verification contract with a "must not" | `docs/Functionality/FSD.md` | Each `**FR-x.y** [Must|Should]` / `**NFR-x.y**` and each FR-2.1x state-table row has a compact contract row (with a non-empty *Must NOT happen* cell or an explicit "—" only where nothing can be prohibited) or a YAML `verification:` block with `prohibited_outcomes` |
| 2 | Requirements atomic and falsifiable | same | No requirement joins two obligations; each names a stimulus, an observation and (where timed) a deadline |
| 3 | Provenance tagged | same | Every requirement line carries `[user]`, `[derived]`, `[pack:esp32]`, `[tooling]` or `[proposed]` |
| 4 | No unaccepted value left as approved | same, Appendix C | Every `[proposed]` value appears in an `open_decisions` entry or the approval_status names it |
| 5 | Stateful system has a complete state model | FSD §2.3 | States table, normative transition table, unhandled-event default, prohibited transitions |
| 6 | Security profile precedes security requirements | FSD §22 | Named profile, accepted risks listed, no "encrypted or obfuscated" criterion |
| 7 | Build-read numbers stated | FSD §2.2, §23 | Flash 8 MB, PSRAM, console pins, partition offsets, image ceiling, IDF version |
| 8 | Phases enterable in order | FSD §3 | Every requirement in a phase's exit is buildable from that phase and earlier ones |
| 9 | Three planes committed | `docs/00-Overview.md`, `docs/Method/`, `docs/UserDocumentation/User-Manual.md` | All tracked by git |
| 10 | No architecture-critical `(assumed)` unresolved | FSD §4 Assumptions | A1–A3 are each tied to a debugging-agenda item that resolves them |

## Mechanical checks

```bash
F=docs/Functionality/FSD.md
git ls-files docs/00-Overview.md docs/Method docs/UserDocumentation | wc -l      # > 0
grep -c 'shall' $F
grep -nE '^\s*- \*\*N?FR-|^\| N?FR-' $F | grep -inwE 'appropriate|graceful|user-friendly|as needed|reasonable|sufficient|robust|seamless|acceptable|normal operation|best effort'  # expect no output
grep -nE '\{\{|TODO|<placeholder' $F                                              # expect no output
# every declared ID has a contract row or YAML block:
comm -23 <(grep -oE '\*\*N?FR-[0-9]+\.[0-9]+\*\*' $F | tr -d '*' | sort -u) \
         <( (grep -oE '^\| N?FR-[0-9]+\.[0-9]+ \|' $F | grep -oE 'N?FR-[0-9.]+[0-9]'; grep -oE '^id: N?FR-[0-9.]+' $F | cut -d' ' -f2) | sort -u)   # expect no output
```

## Judgement checks

- Pick five requirements at random, including one from §6 and one from §2.3. For each, could a competent stranger build a rig from the contract alone that returns pass or fail? Quote the contract.
- Walk §3 phase by phase: name, for each exit requirement, the phase that supplies what it rests on.

## Traps

- NFR-2.1–2.7 are in a table, not `**NFR-2.x**` lines; the mechanical diff does not see them. Check their contract table by eye.
- FR-2.10–FR-2.19 live in the state table and the compact table under it; FR-2.19 appears only in the compact table.
- The `udp_log_host` default in §19 is still `[proposed]` (not part of OD-1).
- A contract row whose stimulus needs a capability from a later phase (printer, RAW port, MQTT, provisioning) makes its phase exit unenterable — check the exit lists against §3 scopes, not just the IDs.
- A contract that observes a log line must use a marker FR-20.1 defines; a contract that observes a status field must use an Appendix B key (`job.completed`, not `jobs_completed`).
- One requirement covering several tasks, endpoints or verbs (GET *and* POST) needs a stimulus for each, or a split.
- A requirement naming several rejection conditions or triggers ("bad magic, wrong chip, > 3 MB"; "state, printing or counters") needs a stimulus for each named one — not a substitute ("truncated").
- A precondition such as "credentials stored" must be reachable with the scope of the phase whose exit lists the requirement.
- A deadline expressed through another parameter ("after keepalive expiry") is only falsifiable if that parameter has a stated value.
