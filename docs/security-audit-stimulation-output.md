# Security audit — stimulation output threat model (audit phase 3)

**Status:** audit phase 3 — threat model.
**Date:** 2026-10-15
**Tracking issue:** [#38](https://github.com/embeddedos-org/eNI/issues/38)
(formal security audit, eNI + eIPC combined). This document is phase 3
of that audit; it does not close #38, and no audit conclusion should
be read into it beyond the findings register below.

## Audit plan (preamble)

- **Scope, phase 3:** the stimulation command path — the parameter
  set a caller supplies, the safety gate that checks it, the driver
  boundary that turns it into electrode output, and the host link
  that delivers commands. The parameter struct is
  `eni_stim_params_t` (`common/include/eni/stimulator.h`):
  `type`, `channel`, `amplitude` (float), `duration_ms`,
  `frequency_hz` (float), `pattern`. The safety gate is
  `eni_stim_safety_check()` (`common/src/stim_safety.c`, limits in
  `common/include/eni/stim_safety.h`).
- **Phases so far:** phase 1 — model loading
  (`docs/security-audit-model-loading.md`); phase 2 — the eIPC
  tensor transport (`docs/security-audit-tensor-transport.md`);
  this phase — stimulation output, the third attack surface #38
  names, and the one whose failure mode is physical.
- **Evidence standard:** every control claimed below must be backed
  by a test or a CI-visible artifact. A control that exists only as
  code is recorded as *code-verified, evidence pending*; a control
  with a test is *verified*; an absent control is stated as absent.

## Assets and trust boundaries

Assets:

- The stimulated tissue itself. Unlike phases 1–2, a failure here
  is not data loss or code execution — it is energy delivered to a
  person outside the intended envelope.
- The safety envelope as configuration: `eni_stim_safety_t`
  (configured `max_amplitude`, `max_duration_ms`,
  `min_interval_ms`, `max_daily_count`) clamped at init to the
  absolute ceilings (`ENI_STIM_ABSOLUTE_MAX_AMPLITUDE` 2.0 mA,
  `ENI_STIM_ABSOLUTE_MAX_DURATION_MS` 1,800,000 ms).
- The counters the envelope depends on (`last_stim_time_ms`,
  `daily_count`, `total_count`) — rate and daily limits are only
  as trustworthy as this state and the clock behind it.

Trust boundaries:

| Boundary | Crossing |
|---|---|
| Host link → command path | Stimulation parameters arrive from the host side (the same link phase 2 audited for tensor data). A compromised or confused host process is inside this boundary's threat model: parameters are claims, not instructions. |
| Caller → safety gate | `eni_stim_safety_check()` is the verify-the-claim step: absolute ceilings first, then configured limits, then rate and daily count. |
| Safety gate → driver | `eni_stimulator_ops_t.stimulate()` is a function-pointer table — drivers are pluggable. The gate protects output only on paths that actually call it. |
| Driver → electrodes | The output boundary. Nothing downstream of the driver re-checks the parameters. |

## The core threat

Phases 1 and 2 both reduced to the same shape: the failures that
matter *look* valid. Phase 3's instance is a **well-formed but
out-of-envelope stimulation command** — a structurally perfect
`eni_stim_params_t` whose values would deliver amplitude,
duration, or repetition the envelope exists to prevent.
Unauthorized stimulation is in #38's zero-criticals acceptance
list; this path is where it would happen.

## STRIDE over the command path

| Threat | Instance on this path | Control and status |
|---|---|---|
| Spoofing | A host-side caller issues commands without the session authority the host link established (phase 2's descriptor lesson, applied to commands) | Host-link authentication is phase 2 scope; no command-level authority field exists in `eni_stim_params_t` — *recorded as a composition question for the #38 review, not a finding here* |
| Tampering | Parameters modified between the safety check and the driver (shared mutable params struct) | Params are passed by const pointer to the check; whether every driver copies before use is *unverified* — evidence item 3.1 below |
| Repudiation | A stimulation occurred that no log records | `total_count` / `total_stimulations` exist as counters; an audit-log control is *not evidenced* in this layer |
| Information disclosure | Stimulation history (daily/total counts, last-stim time) reveals treatment patterns — health-adjacent data | Minimization is #38's privacy scope; no control claimed here |
| Denial of service | Command flooding exhausts the daily count, denying legitimate stimulation; or a stuck `active` state blocks output | Daily count is a safety control that doubles as a DoS surface — noted, accepted shape for a safety cap |
| Elevation of privilege | A caller exceeds the configured envelope by exploiting the check itself (see F3-1/F3-2) | Absolute ceilings in the check — *verified* for amplitude/duration; gaps in the findings register |

## Controls and evidence

Verified from `common/src/stim_safety.c` (read in full,
2026-10-15) and the test tree:

| Control | Code | Evidence |
|---|---|---|
| Amplitude ≤ absolute 2.0 mA and ≤ configured max | `eni_stim_safety_check` | `tests/test_stim_safety.c`, `tests/test_stim_safety_coverage.c`, `tests/test_stim_safety_extended.c` — *verified at unit level* |
| Duration ≤ absolute 1,800,000 ms and ≤ configured max | same | same — *verified at unit level* |
| Configured ceilings clamped to absolute at init | `eni_stim_safety_init` | same suites — *verified at unit level* |
| Minimum interval between stimulations | check + `eni_stim_safety_record` | same suites — *verified at unit level, subject to F3-2's clock assumption* |
| Daily count cap | check + record + `reset_daily` | same suites — *verified at unit level* |
| Gate is called before output | call sites in `common/src/feedback.c`, `framework/src/feedback.c`; end-to-end in `tests/integration/test_full_pipeline.c` | *Partial:* the feedback paths call the gate; whether **every** `stimulate()` path (including direct driver use) is gated is not evidenced — evidence item 3.1 |
| `frequency_hz` bound | **absent** from `eni_stim_safety_check` (no comparison touches the field) | — see F3-3 |
| `channel` / `pattern` / `type` validity | **absent** from the safety check (may exist driver-side) | *unverified* |

## Findings register (phase 3, initial)

- **F3-1 (fixed 2026-10-16): the check did not refuse NaN or
  negative amplitude.** Every amplitude comparison in
  `eni_stim_safety_check` is of the form `params->amplitude > max`.
  A NaN amplitude makes every such comparison false, and a
  negative amplitude is below every maximum — both therefore
  return `ENI_OK` from the gate. Whether downstream code is
  harmed depends on the driver (a NaN reaching a DAC conversion
  is undefined behaviour territory; negative amplitude may or
  may not be meaningful for biphasic patterns — the headers do
  not say). Stated as a code-verified fail-open edge, severity
  pending the phase-2-style code review: the gate should refuse
  non-finite and out-of-range-signed inputs explicitly, with a
  test, rather than inheriting float-comparison luck.
  **Resolution (2026-10-16):** `eni_stim_safety_check()` now
  refuses `!isfinite(amplitude)` and `amplitude < 0` explicitly,
  before any comparison runs, returning `ENI_ERR_PERMISSION`.
  Evidence: `tests/test_stim_safety.c` gains NaN, +Inf, −Inf and
  negative-amplitude refusal cases, plus a zero-amplitude case
  pinning that the gate does not over-refuse.
- **F3-2 (open): the rate limit assumes a monotonic clock.**
  `elapsed = current_time_ms - safety->last_stim_time_ms` is
  unsigned arithmetic; a clock reading that moves backwards
  (host-supplied time, a reset, a wrap of the source clock)
  makes `elapsed` enormous and the minimum-interval check
  passes immediately. The control is verified for honest clocks
  only. A test with a backwards step, and a stated clock-source
  requirement, are the missing evidence.
- **F3-3 (open): `frequency_hz` is unbounded at the safety
  layer.** Amplitude and duration have absolute ceilings;
  frequency has none — not in the check, not as an absolute
  constant in `stim_safety.h`. Charge delivery is a function of
  frequency as much as amplitude. Either the safety layer gains
  a frequency ceiling, or the audit records, with evidence,
  which layer owns it and what its limit is.
- **F2-1 status (from phase 2, updated 2026-10-16):** the
  session-side arena registry F2-1 calls for has landed on the
  eIPC side: `protocol/tensor_registry.go` in
  embeddedos-org/eIPC (commit `9ad8edab`, 2026-10-15, CI green)
  registers validated spans per `ShmName` and refuses
  overlapping registrations with `ErrTensorOverlap`; its
  table-driven tests are the cited evidence. This document
  remains the eNI-side record of that dependency.

## Evidence items for the next phase

- **3.1 Call-path audit:** enumerate every caller of every
  `stimulate()` implementation and show the safety gate precedes
  output on each, or record the ungated paths.
- **3.2 Edge-input tests:** NaN / negative / infinite amplitude,
  backwards clock step (F3-1, F3-2), each asserting refusal at
  the gate once the gate is hardened.
- **3.3 Frequency ownership:** the F3-3 decision, recorded with
  the owning layer's limit and its test.

## Cross-references

- Phase 1: `docs/security-audit-model-loading.md`
- Phase 2: `docs/security-audit-tensor-transport.md` (finding F2-1)
- Audit proposal and scope: `docs/security-audit-proposal.md`,
  `docs/audit-scope.md`
- Safety gate: `common/include/eni/stim_safety.h`,
  `common/src/stim_safety.c`; parameters and driver contract:
  `common/include/eni/stimulator.h`
- eIPC registry (F2-1 build half): `embeddedos-org/eIPC`
  `protocol/tensor_registry.go`
