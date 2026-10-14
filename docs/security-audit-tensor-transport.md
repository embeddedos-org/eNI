# Security audit — tensor transport threat model (audit phase 2)

**Status:** audit phase 2 — threat model for the eIPC tensor
transport that carries eNI's neural data.
**Date:** 2026-10-14
**Tracking issue:** [#38](https://github.com/embeddedos-org/eNI/issues/38)
(formal security audit, eNI + eIPC combined). Phase 1
(`security-audit-model-loading.md`) covered model loading; this is
phase 2 of the same audit. It does not close #38.

## Scope

eNI's host link moves tensors between processes over eIPC. The
transport's security surface is the **tensor descriptor**
(`embeddedos-org/eIPC`, `protocol/tensor.go`): a self-describing
record of an arena in shared memory — shape, strides, dtype, byte
offset, total length, and the capabilities it claims — bound to
`core.Message` by `AttachTensor`/`ExtractTensor` (landed 2026-10-13).
Phase 2 audits that descriptor path: what a malicious or corrupted
descriptor can make a receiver do, and what evidence must exist
before the transport is trusted with neural data.

Out of scope: the Frame codec itself (already fuzzed —
`FuzzFrameDecode`), and any transport below eIPC (shm, sockets),
which the descriptor must not assume is trustworthy.

## Assets and trust boundaries

- **The arena bytes.** Tensor payloads are neural data — inputs may
  be health-adjacent signals, outputs may drive actuators. Integrity
  and confidentiality both matter; a descriptor that points outside
  its arena is a memory-safety bug, not a data bug.
- **The descriptor is untrusted input.** It crosses a process
  boundary, so every field in it is attacker-controlled until
  validated. The bridge-contract rule from the Track-1 design
  applies: descriptors carry arena **handles and declared
  capabilities** across the boundary — raw pointers never cross,
  and a capability named in a descriptor is a *claim*, not a grant.
- **The capability set.** `Message` carries a single primary
  capability claim; the descriptor carries its own capability list.
  The two must agree (`ExtractTensor` rejects a mismatch) and both
  must be checked against what the receiver actually granted.

## STRIDE over the descriptor path

| Threat | Vector | Required control |
|---|---|---|
| Spoofing | A descriptor asserting a capability it was never granted — the CWE-290 shape (an asserted identity/capability accepted without proof), the same pattern as the LiteLLM MCP fabricated-Bearer incident (CVE-2026-59822) | `Validate` rejects capability strings outside the granted set; `ExtractTensor` rejects message/descriptor capability mismatch; attach refuses a descriptor with no declared capability (fail-closed) |
| Tampering | Overlapping arenas: two descriptors claiming the same shm span with different shapes, so one writer's tensor is another reader's | Descriptor validation must reject spans that overlap an already-registered arena for the session (registry-side evidence, see below) |
| Tampering | Footprint overflow: shape × strides whose byte size wraps int64, making a huge tensor validate as a small one | Checked arithmetic in footprint computation; overflow is a validation error, never a wrap |
| Tampering | Unaligned `Offset` for the dtype, smuggling misaligned access into receivers that assume alignment | `Validate` enforces dtype alignment on `Offset` and on the computed footprint |
| Information disclosure | Over-long `TotalLength` reading past the arena into adjacent shm | Span must fit the arena exactly as registered; decode re-validates after parsing (`ValidateAfterDecode`) |
| Denial of service | Hostile dimensions (ndim at the encoding max, zero-stride broadcasts) inflating receiver allocations | Bounded ndim and element counts at decode; hostile-input unit tests already pin the truncated/oversized cases |
| Elevation of privilege | A data-plane descriptor influencing dispatch (operator selection, code paths) — the phase-1 untrusted-code angle applied to transport | Descriptors are data only; no field may select code. Any future dispatch use needs its own audit phase |

## Evidence this phase must produce

Phase 2 closes when eIPC's CI carries, and this register can cite:

1. A native fuzz target over descriptor decode + validate
   (`FuzzTensorDescriptor`): any descriptor that decodes and
   validates must round-trip; no panics, ever.
2. Table-driven property tests for the four bridge-contract cases:
   overlapping arenas, footprint integer overflow, unaligned
   offset, and a capability string outside the granted set — all
   rejected, fail-closed.
3. The overlap control's session-side half: where arena spans are
   registered (the transport's session/arena bookkeeping), a test
   that a second registration overlapping a live span is refused.
   If no such registry exists yet, that absence is a finding, not
   an assumption.

Items 1–2 are the eIPC build item paired with this document
(2026-10-14 daily run); item 3 is recorded here as the phase-2
follow-up.

## Findings register (phase 2, initial)

- **F2-1 (open):** descriptor-level controls exist (validate,
  capability match, fail-closed attach), but arena-overlap is a
  *session* property — a single descriptor cannot see the other
  spans it might overlap. Until a registration-side check exists,
  overlap resistance rests on the receiver's arena bookkeeping,
  which has no test evidence yet. Tracked as evidence item 3 above.
- **F2-2 (informational):** the phase-1 lesson repeats on transport:
  the failures that matter are the ones that *look* valid — a
  well-formed descriptor claiming too much. The controls above are
  all of the "verify the claim" kind, and the fuzz + property
  evidence is what keeps them honest.

## Cross-references

- Phase 1: `docs/security-audit-model-loading.md`
- eIPC descriptor + binding: `embeddedos-org/eIPC`
  `protocol/tensor.go`, `protocol/tensor_message.go`,
  `docs/track1/tensor-transport.md`
- Track-1 capability manifest (the vocabulary descriptors claim
  against): `embeddedos-org/eosllm` `docs/track1/capability-manifest.md`
