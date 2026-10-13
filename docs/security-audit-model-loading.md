# Security audit — model loading threat model (audit phase 1)

**Status:** audit phase 1 — threat model and audit plan.
**Date:** 2026-10-13
**Tracking issue:** [#38](https://github.com/embeddedos-org/eNI/issues/38)
(formal security audit, eNI + eIPC combined). This document is phase 1
of that audit; it does not close #38, and no audit conclusion should
be read into it beyond the findings register below.

## Audit plan (preamble)

- **Scope, phase 1:** the model-loading path — `.eni_model` parsing,
  the ONNX refusal path, and the quantize tool's inputs and outputs
  (`tools/quantize_model.py` and its tests). Phase 2 extends to the
  eIPC tensor transport eNI's host link depends on.
- **Phases:** (1) this threat model, (2) manual code review of the
  in-scope paths against it, (3) targeted fuzzing of the parsers,
  (4) remediation and re-test.
- **Evidence standard:** every control claimed below must be backed
  by a test or a CI-visible artifact. A control that exists only as a
  code comment is recorded as *unverified*, not as a mitigation.

## Assets and trust boundaries

Assets:

- Model weights and structure (`.eni_model` v1/v2 files) — integrity
  is safety-relevant: eNI output can drive stimulation-adjacent
  decisions, so a tampered model is not a cosmetic problem.
- Neural signal data the loaded model processes — biometric,
  health-adjacent, confidential.
- The loader process itself — a parser compromise is code execution
  in a safety-critical component.

Trust boundaries:

| Boundary | Crossing |
|---|---|
| Model file → loader | Untrusted bytes (a model file can come from a researcher, a collaborator, a download) enter `read_eni_model()` and are parsed as trusted structure. This is the primary boundary. |
| Source model → quantize tool | ONNX and other source formats enter `tools/quantize_model.py`; quantized output crosses back as a `.eni_model` that the loader will trust. A poisoned input must not become a trusted output. |
| Loader → inference | Parsed structure becomes operator dispatch and tensor allocation. Anything the parser accepted is executed here. |

Rules at the boundary: model files are **data, never code**; every
length, count, and offset in the file is validated before use; any
format the loader does not fully support is refused, not partially
interpreted.

## STRIDE over the loader

| Threat | Vector in the loading path | Control in place (mitigation) |
|---|---|---|
| Spoofing | A model file presented as a vetted model with no provenance check. | Provenance is a known gap — recorded in the findings register; phase 2 scopes signing via the org's #162 envelope pattern. |
| Tampering | Corrupted or maliciously edited `.eni_model` (inflated tensor counts, out-of-range offsets) causing out-of-bounds reads/writes at load or inference. | `read_eni_model()` validates v1/v2 structure (header, entry/output shapes) before use — the fail-closed validation landed with the #40 fix; malformed files are rejected, not repaired. |
| Repudiation | A bad model loaded with no record of its source or digest. | Partial: quantize tooling records its inputs; a load-time digest log is a phase-2 item in the register. |
| Information disclosure | A crafted model causing the loader to read and leak adjacent memory into outputs or errors. | Bounds validation (above) is the primary control; error paths must not echo raw buffer contents — verified in phase 2 review. |
| Denial of service | A small file declaring enormous tensors, exhausting device memory at load. | Declared sizes are validated against the file's actual contents and format limits before allocation (the #40 fail-closed behavior: refuse rather than emit or accept zero-weight placeholders). |
| Elevation of privilege | Model-supplied bytes influencing code selection — operator dispatch, patch/JIT application, or any path where data becomes executable behavior. | See the untrusted-code section below; the standing rule is that no such path may exist, and any found is a critical finding. |

## Untrusted-code angle: model bytes are not code

Some model ecosystems blur data and code — the LLMJVM-PATCH pattern,
where a model artifact carries patched or generated code that the
runtime later applies or executes, treating code as data until the
moment it isn't. That pattern is in scope for this audit even though
eNI's formats are declarative:

- Any path where model-supplied bytes select code — an operator id
  indexing a dispatch table, a "plugin" or "custom op" field naming
  a library, a patch/delta section applied to firmware — crosses the
  data/code boundary and must be fail-closed: unknown ids refuse the
  load, never fall through to a default handler.
- Dispatch tables must be bounds-checked against a fixed, compiled-in
  operator set. A model can choose *among* compiled operators; it can
  never introduce one.
- Quantize outputs inherit this rule: the tool must not copy an
  executable or patch section from a source model into a `.eni_model`
  it emits.

**Phase-1 verdict:** no path was identified in the loader or the
quantize tool where model-supplied bytes are executed, applied as a
patch, or used to load code; operator selection is from the fixed
format-defined set. Evidence: the in-scope sources are
`tools/quantize_model.py` (parse, validate, quantize, emit) and the
format validation exercised by `tests/unit/test_quantize_validation.py`
— neither contains a code-loading or patch-application path. Phase 2
re-checks this verdict against the full firmware tree, not just the
tooling.

## Findings register (phase 1)

| # | Finding | Severity | Status |
|---|---|---|---|
| F1 | Model provenance: `.eni_model` files carry no signature; a vetted model and a tampered one are indistinguishable at load time. | High | Open — phase 2 scopes signing (org #162 envelope pattern) and a load-time digest log. |
| F2 | Untrusted-code verdict (above) is scoped to the tooling tree; the firmware-side dispatch paths are not yet reviewed. | Medium | Open — phase 2 review scope. |
| — | No critical findings in phase 1. The parser and quantize paths fail closed on malformed input (the #40 controls), and no data-to-code path was found in scope. | — | Phase 2 examines: eIPC tensor transport, firmware-side operator dispatch, error-path information leakage. |

## Cross-references

- [`security-audit-proposal.md`](security-audit-proposal.md) — the #38 proposal this phase executes against.
- `tools/quantize_model.py` — the quantize tool in scope.
- `tests/unit/test_quantize_validation.py` — the fail-closed validation evidence.
