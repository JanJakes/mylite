# Retained-Write Attribution

## Status

Specified; implementation pending.

## Summary

The final mixed large-dataset seed at one million fact rows takes 174.18
seconds in MyLite and 47.45 seconds in the bundled SQLite comparator, a 3.67x
ratio and a 126.73-second absolute gap. The workload is linear, retained-plan
hits eliminate repeated planning, and the existing 100K profile attributes
12.49 of 14.80 seconds to calls through `sqlite3_step()`. That profile does not
separate native SQLite work from MyLite's physical schema, collations,
generated foreign-key guards, or public execution lifecycle.

This phase adds a controlled four-layer seed benchmark and the profiling
instrumentation needed to attribute the residual before any optimization is
attempted. Every layer executes the same logical rows in the same phase order
and verifies the same final state.

## Existing Evidence

- [Extended large-dataset qualification](../../performance/large-dataset-extended-qualification-2026-07.md)
- [Repeated-write and import qualification](../../performance/repeated-write-import-qualification-2026-07.md)
- `mylite_large_dataset_benchmark`
- MyLite's opt-in runtime profile in
  `packages/libmylite/src/runtime/mylite_profile.c`

The mixed seed contains retained inserts for accounts, items, item-tag
bridges, upsert targets, composite parents, and foreign-key fan-out support
tables. The one-million-row fixture performs 4,030,399 logical writes.

## Attribution Layers

The benchmark creates a fresh database per layer and never changes durability,
transaction size, row order, values, indexes, or constraints merely to improve
a result.

### Layer 1: native SQLite schema

The existing bundled SQLite schema and retained SQLite statements are the
baseline. Native SQLite collations and native SQLite foreign keys implement
the comparator's schema behavior.

### Layer 2: MyLite physical schema

MyLite creates the complete schema through its ordinary DDL path. The
benchmark then executes retained plain physical `INSERT ... VALUES` programs
directly against MyLite's underlying SQLite connection.

This layer retains:

- MyLite physical table and index definitions;
- MyLite physical types and custom collations;
- the same file/VFS and transaction boundary as the other MyLite-backed
  layers.

It excludes MyLite-generated guard predicates and the public statement
lifecycle. It is diagnostic only and is never a supported application entry
point.

### Layer 3: generated guarded SQLite

MyLite creates the schema and generates retained physical INSERT programs
through the ordinary planner. The benchmark captures the exact generated SQL
and executes it directly with the same converted physical values.

This layer adds generated foreign-key `EXISTS` predicates and any generated
SQLite scalar expressions to Layer 2 while still excluding public statement
reset, diagnostics, catalog synchronization, result ownership, and
transaction bookkeeping.

The harness must prove that captured programs belong to the expected physical
table, use the expected parameter count, and produce the same row/checksum
state. It must not hand-author a supposedly equivalent guard.

### Layer 4: full MyLite

The existing public retained-statement seed is the end-to-end layer. It uses
`mylite_prepare()`, reset, bind, step, affected-row checks, MyLite transaction
state, diagnostics, write metadata, catalog generation checks, and result
ownership.

## Program Discovery

A separate untimed discovery database is created with the same deterministic
DDL order. A SQLite statement trace records the exact physical INSERT program
executed by the first valid retained MyLite statement for each seed table.

For every discovered program the benchmark records:

- logical and physical table identity;
- SQL text hash and parameter count;
- whether the program contains generated guard predicates;
- the corresponding plain physical INSERT derived from the generated target
  and column list;
- successful preparation against each same-layout attribution database.

Physical names must match across the discovery and measurement databases. A
mismatch fails the run; the benchmark does not guess or rewrite catalog IDs.

## Workload and Sampling

The complete seed is split into the existing timed phases:

1. accounts;
2. items;
3. item tags;
4. support tables.

Support-table output additionally reports upsert/composite-parent, fan-out,
RESTRICT, and SET NULL subphase counters so one small path cannot be hidden by
the aggregate.

Qualification runs use:

- 100K and 1M fact rows;
- five samples per layer and size;
- a balanced forward/reverse layer order, rotated between samples so no layer
  owns a fixed warmup position;
- one explicit transaction per complete seed, matching the existing mixed
  load;
- CPU affinity where available;
- fresh files and a warm filesystem cache policy recorded with the evidence.

Instrumentation and sampled-profile runs are separate from the timing run.
Counter collection may perturb execution and therefore cannot replace the
uninstrumented paired wall-time result.

## Counters

Each layer and phase records:

- wall and process CPU nanoseconds;
- logical writes and affected rows;
- SQLite statement executions;
- `SQLITE_STMTSTATUS_VM_STEP`, `FULLSCAN_STEP`, `SORT`, `AUTOINDEX`,
  `REPREPARE`, `RUN`, `FILTER_HIT`, and `FILTER_MISS`;
- SQLite allocation-count and allocation-byte deltas where the bundled status
  APIs expose them;
- MyLite instrumented allocation count and bytes;
- custom collation callback count and elapsed nanoseconds;
- custom SQLite scalar-function callback count and elapsed nanoseconds;
- generated guarded-statement execution count and VM steps;
- retained DML plan creations/hits and execution-statement cache hits/misses.

The opt-in profile wrapper samples statement-status counters immediately after
each profiled `sqlite3_step()` and resets the per-statement counters after
recording. Profiling-only callback trampolines preserve the original SQLite
callback application data and destructor behavior. When no generated scalar
function executes, the recorded scalar count is explicitly zero.

Every counter is monotonic within a profile and checked for overflow. Metadata
and user-program VM steps are reported separately.

## Sampled Profiles

The harness records call-graph samples for the 100K counter workload at each
layer. Profiles must identify the binary revision, symbols, sampling command,
kernel perf policy, sample count, and unresolved-symbol count.

At minimum, the report groups samples into:

- SQLite b-tree, VDBE, pager, and index maintenance;
- MyLite collation comparison;
- generated-guard parent lookup;
- MyLite value conversion and binding;
- MyLite retained-statement lifecycle and diagnostics;
- allocation and memory movement.

Unavailable or permission-denied profiling is reported as a qualification
blocker, not silently replaced with wall-time inference.

## Correctness

Every layer must match the full MyLite seed for:

- row count and deterministic checksum per table;
- total logical and affected writes;
- parent/child foreign-key validity;
- unique and primary-key state;
- NULL distribution and deterministic text payloads;
- transaction commit and reopen verification.

The direct diagnostic layers are disposable and never update MyLite catalog
timestamps. Verification therefore reads physical user tables directly for
all layers and separately verifies the full MyLite database through the public
API.

## Attribution

For each phase, size, and sample the report calculates:

- physical-schema delta: Layer 2 minus Layer 1;
- generated-program delta: Layer 3 minus Layer 2;
- public-MyLite delta: Layer 4 minus Layer 3;
- total residual: Layer 4 minus Layer 1.

The deltas are diagnostic interactions, not independent constants. The report
must accompany every wall-time delta with VM steps, callback time, allocation
counts, and sampled-profile evidence. It must explain at least 90% of the
one-million-row absolute residual through measured layer deltas and
counter/profile evidence before an optimization is proposed.

## Optimization Gate

Only a demonstrated dominant avoidable cost may be changed. Physical indexes,
collation semantics, foreign-key correctness, diagnostics, schema-generation
checks, and transaction behavior are not removable benchmark overhead.

After any change:

- rerun the four layers at 100K and 1M;
- use paired ABBA measurements against the pre-change revision;
- preserve linear scaling and every correctness check;
- report both absolute time and ratio;
- retain the attribution counters so improvement cannot hide displaced work.

## Architecture and Dependencies

The attribution client and profile fields are developer tooling. They add no
public ABI, shipping dependency, catalog state, or `.mylite` file-format
field. Profiling remains opt-in through `MYLITE_ENABLE_PROFILING`.

Generated SQL capture uses SQLite's public trace and statement-inspection APIs.
No SQLite fork patch is required. The shipped nonprofiling library does not pay
callback or statement-status instrumentation overhead.

## Test Plan

Before qualification:

1. Add profile tests for statement-status accumulation/reset, metadata
   separation, collation/scalar callback counts, elapsed time, inactive
   profiles, and counter overflow handling.
2. Add a small four-layer smoke that covers every seed phase and validates
   generated-program identity, parameter counts, row counts, checksums, guard
   counts, and VM-step ordering.
3. Add negative tests for missing programs, physical-name drift, incomplete
   layer matrices, wrong checksums, and profiling-disabled invocation.
4. Run the full native suite, sanitizers, formatting, static analysis, ABI,
   install, compatibility, and production-size gates.
5. Run five-sample ABBA timing, counter, and sampled-profile passes at 100K and
   1M, publish raw artifacts, and write the quantitative attribution report.
