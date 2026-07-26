# Runtime Performance Follow-up Plan

## Scope

This work closes the three residual opportunities from the large-dataset
qualification:

1. reuse native prepared INSERT, UPDATE, and DELETE analysis;
2. avoid repeating full catalog/physical-schema validation on every clean
   reopen without weakening corruption detection;
3. enforce the resulting performance contracts in paired regression
   automation.

Correctness remains the controlling constraint. Every optimization retains a
fallback to the existing execution or validation path when its reuse proof is
not satisfied.

## Prepared DML Analysis

A native `mylite_stmt` owns at most one DML analysis. The analysis is never
global and is never shared across handles. It is reusable only while all of the
following match:

- catalog generation;
- SQLite schema generation;
- planning-relevant session state;
- parameter count and parameter types;
- a statement-shape check proving that changing parameter values cannot change
  the retained structural plan.

UPDATE and DELETE retain their immutable planned sources, predicates, metadata,
and lowering shape. Direct UPDATE assignment parameters are converted for every
execution. Parameterized arithmetic and other value-sensitive expression
shapes retain the existing full-planning path.

INSERT retains table, column, key, foreign-key, and target-column analysis.
Rows, defaults, value conversion, generated auto-increment values, duplicate-key
assignments, warnings, and bindings are rebuilt for every execution. This keeps
MySQL conversion and diagnostics current while removing repeated catalog and
name-resolution work.

DDL, external schema changes, planning-relevant session changes, or a parameter
type change destroy the retained analysis before replanning. Statement reset
does not clear valid analysis. Statement finalization releases all retained
metadata references.

## Durable Reopen Validation

Catalog schema version 38 adds a durable integrity seal to the singleton
catalog state:

- the catalog generation covered by the last successful validation;
- the SQLite schema cookie covered by the last successful validation.

Catalog triggers invalidate the seal for structural INSERT, DELETE, and UPDATE
operations. Physical DDL changes SQLite's schema cookie. Dropping or changing
an invalidation trigger also changes that cookie. A fast reopen is allowed only
when both persisted seal values match current durable state.

MyLite catalog mutations advance the seal in the same transaction that commits
their catalog generation and physical DDL. Migration and any unsealed or
mismatched file run the complete existing integrity validator, then publish a
new seal only after validation succeeds. A failed validation never seals the
file.

The validator also verifies the invalidation triggers. Direct catalog tampering
through SQLite therefore either invalidates the seal or changes the schema
cookie, preserving reopen rejection coverage.

## Regression Automation

The existing paired baseline/candidate runner remains authoritative. Its
manifest gains:

- retained prepared UPDATE;
- retained prepared INSERT and DELETE;
- a lifecycle reopen scenario that reports open time separately from query
  time.

Raw samples, robust statistics, revisions, and environment metadata remain
artifacts. Tool tests verify manifest parsing, threshold failures, noisy-run
rejection, and the new scenario kinds. Large-dataset runs remain scheduled
qualification artifacts rather than per-PR gates because hosted-runner cost and
variance are too high for a reliable million-row PR threshold.

## Prepared DML Results

Paired Release builds from the same checkout and host used 5,000 iterations
for UPDATE and 3,000 for INSERT and DELETE, with two warmups and seven samples.
Only the benchmark scenario declarations were added to the baseline build.

| Scenario | Baseline median | Retained-plan median | Change |
| --- | ---: | ---: | ---: |
| `runtime.wp_prepared_update` | 72.204 us | 61.218 us | -15.2% |
| `runtime.wp_prepared_insert` | 203.434 us | 172.191 us | -15.4% |
| `runtime.wp_prepared_delete` | 69.088 us | 56.457 us | -18.3% |

Profiling confirms 100 cache hits per 100 measured executions for each
scenario, with zero normalization, parse, or DML-plan builds. Parameterized
arithmetic and parameter conversions that materialize values during planning
remain on the full-planning path.

## Tasks

- [x] Add retained DML analysis ownership, matching, and teardown.
- [x] Rebuild INSERT execution values while reusing structural metadata.
- [x] Reuse safe UPDATE and DELETE plans with correctness fallbacks.
- [x] Add DML plan profiling counters and lifecycle tests.
- [x] Measure retained INSERT, UPDATE, and DELETE before and after.
- [ ] Add catalog integrity seal state and migration.
- [ ] Add and validate structural invalidation triggers.
- [ ] Publish the seal atomically after trusted catalog changes or full validation.
- [ ] Add clean reopen, tamper, migration, rollback, and external-DDL tests.
- [ ] Measure open-only and reopen-plus-query costs.
- [ ] Extend paired performance scenarios and tooling tests.
- [ ] Add scheduled large-dataset evidence without noisy absolute gates.
- [ ] Run focused, full, sanitizer, static-analysis, and workflow validation.
- [ ] Review ownership, invalidation, diagnostics, compatibility, and cleanup.
