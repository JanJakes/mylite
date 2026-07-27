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
rejection, new candidate-only scenarios, missing candidate scenarios, and the
new scenario kinds. A weekly and manually configurable large-dataset job runs
the complete native and system suites at 100,000 rows by default. These runs
remain qualification artifacts rather than per-PR gates because hosted-runner
cost and variance are too high for a reliable large-dataset absolute threshold.

## Prepared DML Results

Paired Release builds used Clang 19.1.7 on the same host, with the candidate
compared against a baseline containing only the benchmark scenario
declarations. The ABBA run collected 18 samples per revision after five
warmups, using 7,500 operations per UPDATE and DELETE sample and 4,500 per
INSERT sample.

| Scenario | Baseline median | Retained-plan median | Change |
| --- | ---: | ---: | ---: |
| `runtime.wp_prepared_update` | 68.184 us | 67.290 us | -1.3% |
| `runtime.wp_prepared_insert` | 190.999 us | 170.205 us | -10.9% |
| `runtime.wp_prepared_delete` | 65.886 us | 56.873 us | -13.7% |

Profiling confirms 100 cache hits per 100 measured executions for each
scenario, with zero normalization, parse, or DML-plan builds. Parameterized
arithmetic and parameter conversions that materialize values during planning
remain on the full-planning path. UPDATE plan reuse removes repeated planning,
but that work is not a material share of this Clang-built scenario's
end-to-end latency.

## Reopen Results

The existing open-only `runtime.cold_open` scenario was run in paired Release
builds with 1,000 open/close operations, ten warmups, and seven samples.

| Scenario | Baseline median | Integrity-seal median | Change |
| --- | ---: | ---: | ---: |
| `runtime.cold_open` | 4.493 ms | 1.095 ms | -75.6% |
| `runtime.reopen_query` | 6.131 ms | 1.859 ms | -69.7% |

The fast path reads catalog state and the SQLite schema cookie. Unsealed files,
generation or cookie mismatches, migrations, catalog trigger changes, and
physical DDL run the complete catalog/physical-schema validator under
`BEGIN IMMEDIATE`. The seal is published only after successful validation.

## Automation Validation

The scheduled-job command was exercised locally at its 100,000-row default.
It produced 469 native CSV records, 62 complete paired summaries covering five
load phases and all 57 scenarios, and ten system rows covering five scenarios
on both engines. The CSV validator accepted all pairs; no checksum,
operation-count, worker-error, or row-count mismatch occurred.

## Final Qualification

The combined candidate was rebuilt from the final source with Clang 19.1.7 and
compared with the pre-change baseline on one pinned CPU using tmpfs databases.
All 16 paired ABBA scenarios passed their robust regression thresholds. The
principal combined-build results were:

| Scenario | Baseline median | Candidate median | Change |
| --- | ---: | ---: | ---: |
| `runtime.wp_prepared_insert` | 197.166 us | 172.180 us | -12.7% |
| `runtime.wp_prepared_delete` | 69.329 us | 60.416 us | -12.9% |
| `runtime.cold_open` | 4.699 ms | 1.148 ms | -75.6% |
| `runtime.reopen_query` | 5.111 ms | 1.499 ms | -70.7% |

The regular WordPress frontend/write requests, prepared SELECT/UPDATE, parser,
large `IN`, metadata, cache-saturation, and concurrent PROCESSLIST scenarios
remained within their measured noise allowances.

The final local qualification passed:

- 689 Release, 689 Debug, and 689 ASan/UBSan core tests;
- the focused TSan, allocator-fault, crash-recovery, and fuzz suites;
- 697 PHP/core integration tests;
- all 912 clang-tidy compile units and the formatting gate;
- production static, PHP extension, and shared-ABI builds;
- compatibility, MySQL expectation, performance, large-dataset, and release
  tooling tests;
- N-1 catalog creation, current migration, N-1 rejection, and current reopen.

The final ownership review confirmed that retained DML analysis is
statement-local, generation- and session-qualified, and released during
statement teardown. Mutable values and diagnostics are rebuilt per execution.
Integrity seals are published only in the catalog transaction that establishes
the validated state; stale seals force validation before commit, and failed
validation rolls back both catalog and physical-schema changes.

## Tasks

- [x] Add retained DML analysis ownership, matching, and teardown.
- [x] Rebuild INSERT execution values while reusing structural metadata.
- [x] Reuse safe UPDATE and DELETE plans with correctness fallbacks.
- [x] Add DML plan profiling counters and lifecycle tests.
- [x] Measure retained INSERT, UPDATE, and DELETE before and after.
- [x] Add catalog integrity seal state and migration.
- [x] Add and validate structural invalidation triggers.
- [x] Publish the seal atomically after trusted catalog changes or full validation.
- [x] Add clean reopen, tamper, migration, rollback, and external-DDL tests.
- [x] Measure open-only and reopen-plus-query costs.
- [x] Extend paired performance scenarios and tooling tests.
- [x] Add scheduled large-dataset evidence without noisy absolute gates.
- [x] Run focused, full, sanitizer, static-analysis, and workflow validation.
- [x] Review ownership, invalidation, diagnostics, compatibility, and cleanup.
