<!-- generated: eos-ai-scaffold -->
# Tasks

Working ledger for `eNI`. The planner writes entries; each owning role
updates its own row. Roles are in [AGENTS.md](./AGENTS.md), the workflow in
[ORCHESTRATION.md](./ORCHESTRATION.md), the gate in [VERIFY.md](./VERIFY.md).

Status is one of: `todo`, `in-progress`, `blocked`, `review`, `done`.

## Active

| ID | Task | Owner | Mode | Status | Depends on |
|----|------|-------|------|--------|------------|
| —  | No active tasks. | — | — | — | — |

## Completed

| ID | Task | Owner | Verified by | Evidence |
|----|------|-------|-------------|----------|
| T-001 | CI ran zero tests and reported success | testing | reviewer | `ci.yml` configured with `-DBUILD_TESTS=ON`, but this project's option is `ENI_BUILD_TESTS`, so no test binary was built; `ctest` exits 0 on an empty test set (verified: exit code 0). The step also ended in `|| true`, which would have discarded a real failure as well. Fixed the flag, removed `|| true`, added `--no-tests=error`. CI now builds and runs 40 tests, all passing. |
| T-002 | Fix a memory leak in the provider test | testing | reviewer | `tests/test_providers.c:29` called `eni_provider_init()`, which allocates provider-private state in `simulator.c:29`, and never called `eni_provider_shutdown()`. LeakSanitizer: 12 bytes direct leak. Shutdown now runs on both the passing and the failing path. 40/40 eNI tests pass under `-fsanitize=address,undefined`. |

---

## Task template

```markdown
### T-000 — <short title>

Owner: <role>
Mode: <see MODES.md>
Status: todo
Depends on: <task ids, or none>

Goal
: <one sentence: what is true afterwards that is not true now>

Acceptance criteria
: - <observable, checkable statement>
  - <observable, checkable statement>

Files in scope
: <paths the owner is expected to touch>

Out of scope
: <what this task deliberately does not change>

Risks
: <what could break, and what would reveal it>

Verification
: | Check | Command | Result |
  |-------|---------|--------|
  | <name> | `<command>` | `NOT RUN` |
```

## Verification commands for this repository

These commands were derived from the manifests at the repository root. Confirm one works before relying on it; a listed script may still be a stub.

| Check | Command | Default state |
|-------|---------|---------------|
| Build | `cmake --build build -j` | `NOT RUN` |
| Unit tests | `ctest --test-dir build --output-on-failure` | `NOT RUN` |

## Rules

- One task per unit of work that can be verified on its own.
- Acceptance criteria are written before work starts and are not edited to match
  what was built. If they were wrong, say so and rewrite them explicitly.
- A task reaches `done` only when the definition of done in
  [ORCHESTRATION.md](./ORCHESTRATION.md) is met and the verification commands
  were actually run.
- `blocked` requires a note naming what it is blocked on and who can unblock it.
