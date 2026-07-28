# July 2026 Whole-Project Follow-up Remediation Plan

## Objective

This plan addresses every actionable finding from the follow-up whole-project
review of commit `453bb5cfef53c322891fa3c27822d8f3ba065d43`.

The work is complete only when every finding ID in the coverage matrix has:

1. a focused reproducer that fails on the audited commit;
2. an independently authored specification or architecture contract;
3. an implementation that fixes the root cause rather than one observed shape;
4. focused and broader regression coverage;
5. MySQL 8.4.9 differential evidence where behavior is compatibility-sensitive;
6. sanitizer, concurrency, fault, performance, or packaging evidence as applicable;
7. updated compatibility and architecture documentation; and
8. a final review showing that no finding was lost, weakened, or silently deferred.

Project licensing remains an explicit release blocker, but its selection is
still deferred. It stays in this plan so it cannot be mistaken for completed
work.

## Delivery Rules

- Correctness, durability, and MySQL 8.4.9 compatibility take priority over
  performance and source-code reduction.
- Add or update a specification before changing public behavior.
- Reproduce each defect before or in the same commit as its fix.
- Use deterministic barriers and failpoints for concurrency and storage tests;
  do not use sleeps to approximate an ordering.
- Keep public ABI additions additive and document ownership, lifetime,
  nullability, invalidation, threading, and diagnostics.
- Treat ASan, UBSan, LSan, TSan, fuzz, crash, and fault-injection failures as
  correctness failures.
- Establish a measured baseline before performance or size changes. Retain an
  optimization only when repeated paired measurements demonstrate a benefit.
- Preserve exact raw samples and environment metadata for performance work.
- Land focused, independently reviewable commits in the phase order below.
- Do not mark a compatibility row green merely because syntax is accepted or a
  placeholder is tested.
- Do not mark a task complete until its implementation, tests, documentation,
  and required broader gates all pass.

## Execution Order

The phases are dependency ordered:

1. Phase 0 freezes reproducers and evidence.
2. Phase 1 is the immediate critical path because it closes durable corruption
   and stale-write semantics.
3. Phase 2 follows because prepared-statement lifecycle changes affect native
   and PHP state ownership.
4. Phases 3, 4, and 5 may proceed in parallel only with disjoint source
   ownership and independent integration reviews.
5. Phase 6 begins after the relevant correctness fixes so measurements do not
   optimize behavior that will be replaced.
6. Phase 7 may advance alongside correctness work, but no corrected claim is
   published until its implementation evidence is final.
7. Phase 8 follows behavioral stabilization; architecture extraction must not
   obscure correctness diffs or invalidate their before/after reproducers.
8. Phase 9 can prepare non-license infrastructure earlier, but release
   publication and redistribution remain blocked until all technical gates and
   the license decision are complete.
9. Phase 10 runs once against one immutable candidate commit. Evidence from
   different candidate commits does not compose into a release qualification.

Within each phase, use separate commits for the failing reproducer and
specification, root-cause implementation, adapter/integration work,
documentation, and final gate updates whenever those units remain independently
buildable and reviewable.

## Phase 0: Reproduction And Baseline Freeze

### Finding register and evidence

- [ ] Record the audited commit, compiler/tool versions, SQLite and zlib
  versions, PHP version, MySQL 8.4.9 image digest, and current CI run.
- [ ] Preserve deterministic source-level reproducers for `COR-01`, `COR-02`,
  `PREP-01`, `PREP-02`, `API-01`, `API-02`, `API-03`, `API-04`, `SEC-01`,
  `SEC-02`, `SEC-03`, `SEM-01`, `SEM-02`, and `SQL-01`.
- [ ] Convert temporary reproducers into first-party tests without checking in
  generated databases, binaries, logs, or absolute local paths.
- [ ] Capture current matched MySQL 8.4.9 outputs for every compatibility
  defect before implementation.
- [ ] Capture current performance samples for durable writes, retained writes,
  `LOAD DATA`, buffered results, and large spatial validation.
- [ ] Capture current production artifact sizes, exports, dependencies,
  package contents, SBOM, provenance, and reproducibility hashes at HEAD.
- [ ] Add a machine-readable finding manifest mapping every ID below to its
  specification, implementation area, native test, MySQL fixture where
  applicable, CI tier, and documentation.
- [ ] Add a validator test that fails if any finding is missing a required
  evidence edge or is marked closed while its checklist remains incomplete.

### Baseline gates

- [ ] Run the complete current Release, Debug, ASan/UBSan, LSan, TSan, fuzz,
  PHP, application, MySQL, coverage, and performance workflows.
- [ ] Record expected failures only for newly added reproducers. Do not weaken
  or skip an existing test to establish the baseline.
- [ ] Confirm the worktree is clean and all temporary audit artifacts and
  review agents are gone before implementation begins.

## Phase 1: Writer-Stable Metadata And Storage Integrity

### `COR-01`: stale metadata-dependent write plans

- [x] Specify the writer-stable planning contract for persistent DDL, direct
  DML, native prepared DML, SQL-level prepared DML, and autocommit-disabled
  first writes.
- [x] Decide on one central protocol:
  - acquire the persistent writer transaction before metadata synchronization
    and planning; or
  - bind plans to a durable catalog/schema generation and replan under the
    writer lock when it differs.
- [x] Prove that the selected protocol does not deadlock read transactions,
  nested catalog operations, explicit transactions, or statement reset.
- [x] Apply the protocol to every persistent single-action ALTER path,
  including `MODIFY`, `CHANGE`, `FORCE`, `ORDER BY`, CHECK, primary-key,
  index, rename, and physical-rebuild variants.
- [x] Apply it to INSERT, REPLACE, INSERT SET, INSERT SELECT, `LOAD DATA`,
  UPDATE, DELETE, joined DML, duplicate-key handling, and prepared-plan reuse.
- [x] Ensure temporary-table and multi-action ALTER paths retain their already
  valid locking behavior.
- [x] Recheck auto-increment reservation, foreign-key validation, duplicate-key
  planning, generated expressions, defaults, and conversion modes after any
  replan.
- [x] Make structural DDL run full physical/catalog validation before
  publishing a new integrity seal.
- [x] Prevent a mutation from sealing a state derived from a plan older than
  the writer-stable generation.

### `COR-01` tests

- [x] Add a trace-hook barrier where handle B commits `ADD COLUMN` after handle
  A plans `MODIFY COLUMN` but before A's writer lock.
- [x] Assert exact catalog columns, physical columns, indexes, constraints,
  seal values, reopen behavior, and post-reopen reads/writes.
- [x] Cover stale type conversion, index changes, nullability, defaults,
  generated columns, foreign keys, and duplicate-key targets for every DML
  family.
- [x] Cover direct and retained prepared plans, statement reset, explicit
  transactions, autocommit off, and the first deferred write.
- [x] Run the concurrency matrix under TSan and repeated multi-process stress.
- [x] Add a release-gate test that fails if planning can precede the
  writer-stable catalog snapshot for a metadata-dependent write.

### `COR-02`: concurrent migration convergence

- [x] Move durable schema-version discovery under the migration writer lock.
- [x] Define whether the complete migration chain uses one transaction or
  rereads and converges after each serialized step.
- [x] Make every migration safely accept a winner's already-committed result.
- [x] Add two-handle and two-process barriers for N-1 to current migration.
- [x] Require both openers to succeed against one validated current-format
  file, with no partial migration, spurious corruption error, or stale handle.
- [x] Repeat migration tests under crash and write/sync/truncate failpoints.

### `STOR-01`: integrity-seal corruption contract

- [x] Specify whether MyLite promises detection of relational/catalog
  inconsistency only, raw payload corruption, or broader page-level bit rot.
- [x] Choose between full open-time structural validation, a deterministic
  catalog/schema digest, or another measured design that cannot trust only
  generation and SQLite schema-cookie values.
- [x] Ensure the fast-open optimization never accepts a raw catalog mutation
  that changes physical/catalog correspondence.
- [x] Add valid-file structured mutations that preserve SQLite b-tree
  integrity and generation/schema-cookie values.
- [x] Require deterministic open-time corruption diagnostics rather than a
  later internal row-operation error.
- [x] Expand the database-open fuzzer beyond 65,536 bytes with valid current
  catalog seeds and post-open catalog/query operations.

### `STOR-02` and `STOR-03`: initialization recovery and fault coverage

- [x] Define recoverability for an `INITIALIZING` file after creator death.
- [x] Implement exclusive validation/recovery, or safe quarantine/removal only
  when the exact owned inode is proven to contain no committed user state.
- [x] Test process death at every initialization transaction, payload sync,
  catalog commit, and lifecycle-byte publication boundary.
- [x] Extend VFS fault coverage from CREATE TABLE to rebuilding ALTER,
  DROP/RENAME, index DDL, migrations, truncate, delete, and close.
- [ ] Run hot-journal and process-death recovery on POSIX and Windows.
- [x] Assert exact pre-operation or post-operation state, no temporary physical
  objects, valid seals, and successful SQLite integrity checks after reopen.

### Phase 1 closure

- [x] Run all catalog, DDL, DML, transaction, concurrency, migration,
  recovery, failpoint, TSan, ASan/UBSan, and application suites.
- [x] Independently review every plan-generation comparison and seal-publication
  site.
- [x] Update storage, transaction, prepared-plan, and file-format documentation.

Linux qualification evidence for the Phase 1 implementation at `a099ea52e`:

- Native: 692/692 tests passed, including four concurrency tests and the
  complete crash/fault-injection recovery model.
- ASan/UBSan: 692/692 tests passed; TSan: 4/4 concurrency tests passed.
- Repository-wide `clang-format` and `clang-tidy` gates passed.
- WordPress: 29,248 tests and 3,440,328 assertions passed, with 86 upstream
  PHPUnit deprecation warnings and 94 skips.
- Drupal: 559 tests and 2,340 assertions passed, with 8 skips.
- Laravel: 2 tests and 12 assertions passed.
- Doctrine DBAL and ORM: 2 tests and 15 assertions passed.
- MediaWiki: 35 tests and 852 assertions passed, with 1 skip.

The remaining unchecked Phase 1 item requires the existing Windows CI job to
execute the hot-journal and initialization process-death paths for this commit.
The corresponding POSIX paths passed locally.

## Phase 2: Prepared Statements, Results, Diagnostics, And PHP Parity

### `PREP-01`: lazy prepare without execution-side effects

- [x] Specify parse-time, analyze-time, prepare-time, first-step, reset, and
  finalize state transitions for every statement family.
- [x] Ensure parameterless table SELECT preparation does not start a read
  transaction, capture a snapshot, install an active cursor, block another
  command, or block another handle's writer.
- [x] Defer SQLite execution preparation when needed to preserve schema and
  session correctness without violating lazy behavior.
- [x] Correct the existing native test that currently requires prepare-time
  transaction activation.
- [x] Cover constant/table SELECTs with and without parameters, intervening
  same-handle commands, concurrent writers, schema changes, and diagnostics.
- [x] Verify the behavior against MySQL 8.4.9 through mysqli and PDO.

`PREP-01` implementation evidence at `e340b6da8`:

- The native cursor lifecycle test covers zero-parameter and parameterized
  constant/table SELECTs, reset before first step, same-handle commands,
  cross-handle DML and DDL, metadata refresh, incompatible DDL diagnostics,
  explicit transactions, `autocommit=0`, and never-executed finalize.
- The profiling test proves prepare performs one parse and analysis but zero
  SELECT lowerings; first step records a retained-plan hit and the sole
  lowering.
- All 692 native tests passed. The broad incremental invocation passed 690 and
  reported two newly registered SQLite ownership executables as not built;
  building those registered targets and rerunning them passed 2/2.
- Focused Debug, Release, ASan/UBSan, and deterministic allocator-failpoint
  tests pass.
- The reviewed public-header mirror and production shared-library symbol
  manifest match, and the production static library remains within its
  15,000,000-byte limit at 12,346,056 bytes.
- The MyLite mysqli and PDO adapter regressions pass, and the pinned MySQL
  8.4.9 mysqli/PDO expectation fixture passes with the recorded transaction,
  writer, schema-change, and diagnostic behavior.

### `PREP-02`: row-producing classification and reset

- [x] Replace inconsistent static and dynamic row-return classifiers with one
  typed statement-result capability.
- [x] Make reset release every execution-owned materialized row, cursor,
  metadata, completion, warning, and binding state while retaining only
  explicitly reusable analysis.
- [x] Re-execute prepared `SHOW`, `EXPLAIN`, DESCRIBE, metadata statements,
  and other row-producing utilities after intervening DDL/session changes.
- [x] Add mutations between executions so stale and fresh output are
  distinguishable.
- [x] Cover repeated reset/finalize under ASan/UBSan and allocation failpoints.

`PREP-02` implementation evidence at `1dd11000e`, `6e0b820c1`, and
`e17af78c4`:

- One AST-derived capability now controls native prepared dispatch, query
  completion semantics, result ownership, and reset for streaming,
  materialized, utility, and dynamic statement families.
- The native cursor regression covers `SHOW TABLES`, `DESCRIBE`, table and
  query `EXPLAIN`, `SHOW VARIABLES`, buffered SELECT, reset before execution,
  completed and partial reset, schema/data/session mutation, dropped-object
  failure, recreation, and repeated reset/finalize.
- Focused Release, Debug, and ASan/UBSan cursor suites passed. The allocator
  sweep covers materialized re-execution failure, repeated reset, recovery,
  and finalize; it also preserves `MYLITE_NOMEM` through SHOW result setup.
- All seven mysqli/PDO extension suites passed with same-object replay
  assertions. The pinned MySQL 8.4.9 mysqli/PDO fixture independently observed
  fresh SHOW, DESCRIBE, EXPLAIN, and session-variable results.
- The production shared-library ABI matches both public manifests. The
  production static archive is 12,347,862 bytes against the 15,000,000-byte
  gate.
- All 692 native tests passed, including the complete crash/fault-injection
  recovery model. The repository-wide clang-tidy gate passed for 915
  first-party compilation units with warnings treated as errors.

### `API-01`: mysqli pending-result state machine

- [x] Specify connection states for pending `real_query`, stored results,
  unbuffered results, partial fetch, exhaustion, free, reset, and errors.
- [x] Reject disallowed commands with MySQL error 2014/SQLSTATE/message instead
  of silently finalizing a pending result.
- [x] Preserve the legal behavior of ordinary buffered `query()` results.
- [x] Cover procedural and object APIs, direct and prepared statements,
  commit/autocommit, strict exception mode, and cleanup ordering.

`API-01` implementation evidence at `44daa2f5f`, with release-review
completion at `0277ee818`:

- The explicit `READY`, `DIRECT_PENDING`, `DIRECT_UNBUFFERED`, and
  `PREPARED_UNBUFFERED` states use identity-checked owners and reject
  unrelated commands before any pending cursor can be cleared.
- The MySQL 8.4.9 fixture and MyLite adapter regression cover report-off and
  strict diagnostics, direct acquisition, buffered and unbuffered reads,
  partial/final/EOF fetches, materialized utility results, prepared
  store/get/metadata/fetch behavior, same- and different-statement execution,
  zero-row results, commit/autocommit rejection, rollback integrity, and
  free/reset/close/destructor recovery. It also covers ping, stat, refresh,
  server debug-info, and kill operations with MySQL's return/exception
  distinction.
- All eight mysqli/PDO developer tests pass. All nine PHP extension tests pass
  under ASan/UBSan with the matching Clang runtime preloaded for the host PHP
  executable. All 701 Release tests pass, and focused clang-tidy plus
  repository formatting gates pass.

### `API-02` and `API-05`: statement-owned diagnostics and warnings

- [x] Add additive native ABI accessors for statement error code, SQLSTATE,
  message, warning count, and indexed warning records.
- [x] Define snapshot ownership and lifetime independently from later
  connection or statement activity.
- [x] Store PDO native error code/message on each statement and make
  `errorInfo()` select the correct statement or connection record.
- [x] Implement mysqli prepared warning counts and real warning iteration
  rather than placeholder code/message values.
- [x] Test two simultaneously live failing statements, an intervening success,
  multirow warning chains, reset, re-execution, and connection close.

`API-02` and `API-05` implementation evidence at `1445aee86`, with
release-review completion at `d5005f606`:

- The additive native ABI exposes independent statement diagnostics, total and
  retained warning counts, and caller-owned indexed warning copies. Direct
  results deep-own their warning snapshots, including counted-only warnings.
- PDO stores native error code and copied message on the handle whose operation
  failed. mysqli copies connection and statement warning lists and gives each
  returned warning object an independently owned FIFO chain.
- The MySQL 8.4.9 fixture, native API regression, deterministic allocator
  failpoint sweep, and all mysqli/PDO extension tests cover simultaneous live
  failures, intervening success, direct and prepared multi-warning chains,
  capping, reset, re-execution, invalid access, and close lifetime.
- All 693 Release tests pass. Focused Debug, Release, and ASan/UBSan native
  tests, all ten developer and ASan/UBSan PHP extension tests, exact ABI gates,
  formatting, focused clang-tidy, and the production size gate pass.

### `API-03` and `API-04`: PHP scalar types and server identity

- [ ] Specify native-protocol conversion for signed/unsigned integer
  boundaries, BIT, FLOAT/DOUBLE, DECIMAL, overflowing unsigned BIGINT, NULL,
  binary data, and text.
- [ ] Return representable integral values as PHP integers and approximate
  numerics as doubles for PDO and prepared mysqli results.
- [ ] Preserve DECIMAL, overflowing integers, text, and binary values as
  strings where MySQL does.
- [ ] Honor `PDO::ATTR_STRINGIFY_FETCHES`.
- [ ] Keep text-protocol mysqli direct-query behavior unchanged.
- [ ] Return MyLite's package version for the PDO client version and the MySQL
  compatibility identity for the server version.
- [ ] Assert server identity equals `SELECT VERSION()` and remove integration
  harness overrides that mask the defect.

### `API-06` and `API-07`: metadata, observables, and framework integration

- [ ] Implement PDO `getColumnMeta()` and complete `describe` metadata from
  native result descriptors.
- [ ] Cover empty results, integer, unsigned, decimal, text, blob, nullable,
  primary, unique, expression, spatial, temporal, and aggregate columns.
- [ ] Define and implement buffered PDO SELECT `rowCount()` compatibility.
- [ ] Expose a stable public connection ID and make mysqli `thread_id` match
  `CONNECTION_ID()` across multiple handles.
- [ ] Replace Laravel and Doctrine test-only bridge wiring with supported
  connector/driver packages, or explicitly scope those baselines as adapters
  rather than drop-in integrations.
- [ ] Test fresh framework applications through normal configuration,
  migrations, hydration, metadata, exception conversion, and transactions.

### `SEC-03`: length-aware database paths

- [ ] Add a length-aware native database-open API with explicit ownership,
  embedded-NUL, empty-path, and `:memory:` semantics.
- [ ] Preserve the existing NUL-terminated API as a documented convenience
  wrapper.
- [ ] Reject embedded NUL bytes in core PHP, mysqli socket/path resolution, and
  PDO DSN handling before any filesystem access.
- [ ] Test NUL placement at the beginning, middle, and end of paths, including
  authorized-prefix bypass shapes, `:memory:`, empty values, and non-ASCII
  paths.
- [ ] Verify that rejected paths create, open, truncate, or delete no file.
- [ ] Run the path matrix under POSIX and Windows, ASan/UBSan, and VFS
  failpoints.

### Phase 2 closure

- [ ] Run core cursor/prepared/diagnostic suites under Release, Debug,
  ASan/UBSan, LSan, and allocation failpoints.
- [ ] Run all PHP adapter suites and differential probes against MySQL 8.4.9.
- [ ] Run WordPress, Drupal, Laravel, Doctrine, and MediaWiki baselines.
- [ ] Review ABI snapshots, ownership documentation, and adapter state
  transitions independently.

## Phase 3: Spatial Safety, Correctness, And Complexity

### `SEC-01`: bounded geometry nesting

- [ ] Determine MySQL 8.4.9 behavior and diagnostics for deeply nested WKB,
  WKT, and GeoJSON.
- [ ] Define one maximum geometry nesting/work budget shared by parsing,
  decoding, validation, conversion, traversal, and destruction.
- [ ] Prefer iterative traversal where untrusted depth can reach the C stack.
- [ ] Enforce the limit at WKB, WKT, GeoJSON, geometry decode, validation,
  copy, transformation, and cleanup entry points.
- [ ] Ensure rejection and cleanup themselves cannot recurse beyond the limit.
- [ ] Add below/at/above-limit tests and large-input ASan/UBSan regressions.
- [ ] Expand geometry fuzzing to reach full SQL/WKT/WKB paths and inputs above
  the previous 65,536-byte ceiling.

### `SEC-02`: scalable and cancellable spatial validation

- [ ] Record `ST_IsValid()` scaling at 8K, 16K, 32K, and 64K vertices.
- [ ] Replace quadratic segment and ring comparison with a sweep-line,
  spatial-index, or equivalently scalable algorithm.
- [ ] Add statement cancellation/deadline checks to long first-party spatial
  loops.
- [ ] Define compatibility-safe complexity and memory limits for pathological
  geometry.
- [ ] Add performance thresholds based on slopes and operation counts, not
  noisy absolute hosted-runner time.

### `SEM-01`: robust topology and metric behavior

- [ ] Replace the fixed absolute spatial epsilon with robust/adaptive
  orientation and segment predicates.
- [ ] Never clamp a positive metric distance to zero solely because it is
  small.
- [ ] Differentially test below/at/above `1e-12` for point-line, line-line,
  polygon boundary, intersection, disjointness, containment, and distance.
- [ ] Repeat cases after translation and scaling to catch coordinate-magnitude
  dependence.
- [ ] Review every topology predicate that derives truth from distance zero.

### Phase 3 closure

- [ ] Run all spatial native and MySQL expectation suites, large-input fuzzing,
  ASan/UBSan, and structured performance tests.
- [ ] Update spatial specifications and compatibility rows to match measured
  values, metadata, limits, and diagnostics.

## Phase 4: SQL Front-end Failure Handling And Resource Bounds

### `SQL-01`: fatal retry status propagation

- [ ] Propagate retry-context initialization and callback `NOMEM` instead of
  restoring the original syntax status.
- [ ] Define precedence for fatal infrastructure errors over recoverable parse
  errors.
- [ ] Add allocator failpoints at every retry allocation and callback.
- [ ] Require `MYLITE_SQL_PARSE_NOMEM`/`MYLITE_NOMEM`, stable diagnostics, and
  leak-free cleanup.

### `SQL-02`: bounded recovery work and memory

- [ ] Measure peak allocations, bytes, lexer passes, callbacks, and runtime for
  flat and shallow malformed input through at least 65,536 tokens and 1 MiB.
- [ ] Define parser work, token, depth, and memory budgets proportional to
  accepted input size.
- [ ] Avoid allocating full per-token indexes unless a retry requires them.
- [ ] Stop retries deterministically when the budget is exhausted and return
  the specified diagnostic.
- [ ] Add scaling and fuzz regression gates.

### `SQL-03`, `SQL-04`, and `SQL-05`: spans, diagnostics, and nesting limits

- [ ] Rebase and validate width, length, precision, scale, and every other
  embedded AST payload span, not only each node's primary span.
- [ ] Assert full-source snapshot invariants for retry-produced ASTs.
- [ ] Remove unchecked `size_t` to `int` diagnostic precision casts and cap
  copied token text to the diagnostic buffer budget.
- [ ] Differentially verify complete syntax-error code, SQLSTATE, and message
  wording against MySQL 8.4.9.
- [ ] Sweep valid nested parentheses and `IF()` expressions across the Lemon
  stack boundary.
- [ ] Document the supported limit or use a growable parser stack with an
  explicit byte ceiling.

### `ARCH-03`: reduce the retry recognizer

- [ ] Inventory every retry as a grammar transform, token transform, utility
  placeholder, or unsupported compatibility fallback.
- [ ] Move grammar/token transforms behind typed parser interfaces.
- [ ] Eliminate retries that duplicate Lemon-recognizable syntax.
- [ ] Add a zero-growth ratchet for retry count and retry-layer source size.

### Phase 4 closure

- [ ] Run all lexer/parser tests in Debug and ASan/UBSan, parser fuzz targets,
  allocation failpoints, host-tool generation, and MySQL syntax expectations.
- [ ] Review every parser error path for forward progress, bounded work,
  ownership, and exact diagnostics.

## Phase 5: Temporal And Result-Metadata Compatibility

### `SEM-02`: `CONVERT_TZ()` boundaries

- [ ] Determine the exact MySQL 8.4.9 supported UTC conversion interval and
  boundary behavior under positive and negative source/target offsets.
- [ ] Preserve the original value when the source-zone-normalized instant is
  outside MySQL's conversion interval.
- [ ] Cover lower/upper boundaries, fractional seconds, leap days, and offset
  crossings.
- [ ] Update the temporal specification and compatibility documentation.

### `SEM-03`: function- and argument-specific metadata

- [ ] Replace broad family descriptors with function- and argument-specific
  result descriptors where MySQL metadata differs.
- [ ] Correct spatial text/binary/geometry, spatial predicates,
  `CONVERT_TZ()`, and aggregate/window metadata.
- [ ] Differentially verify protocol type, collation, length, decimals,
  flags, signedness, nullability, table/origin, and precision.
- [ ] Correct compatibility claims that currently promise metadata parity
  without paired field-metadata evidence.
- [ ] Reuse the same descriptors in native, mysqli, PDO, SHOW, and prepared
  result paths.

### Phase 5 closure

- [ ] Run temporal, spatial, aggregate/window, metadata, mysqli, PDO, ORM, and
  MySQL expectation suites.
- [ ] Review that value semantics and metadata semantics agree for every
  corrected function.

## Phase 6: End-to-End Performance Qualification

### `PERF-01`: durable autocommit qualification

- [ ] Match MyLite, SQLite, and MySQL durability configurations explicitly.
- [ ] Measure 1, 4, and 100 writes per transaction on block-backed ext4 and
  XFS, including single-row and multi-row statements.
- [ ] Record p50, p95, p99, throughput, sync syscall counts, journal behavior,
  and device/environment metadata.
- [ ] Preserve durability; do not improve a benchmark by weakening sync
  semantics.

### `PERF-02`: retained-write attribution

- [ ] Split each seed phase into native SQLite schema, MyLite physical
  schema/collations, generated guarded SQL executed directly, and full MyLite.
- [ ] Record `sqlite3_stmt_status` VM counters, callback counts, collation
  costs, scalar-function costs, generated guard costs, allocation counts, and
  sampled profiles.
- [ ] Explain the current 1M-row 3.67x ratio quantitatively before changing
  code.
- [ ] Optimize only the demonstrated dominant costs and rerun paired ABBA
  measurements at 100K and 1M rows.
- [ ] Keep linear scaling and correctness checks for every load phase.

### `PERF-03`: `LOAD DATA`

- [ ] Replace per-byte `fgetc()` input with chunked buffered reads.
- [ ] Replace per-field allocation/free churn with reusable row/field storage
  or bounded slices.
- [ ] Preserve escape, enclosure, line, warning, conversion, transaction, and
  error semantics.
- [ ] Benchmark by row width, field count, escape density, index count, and
  input size.
- [ ] Gate allocations per row, bytes per second, peak RSS, and MySQL-visible
  results.

### `PERF-04`: buffering, cold data, scale, and application breadth

- [ ] Measure native cursor versus buffered C, mysqli, and PDO results at 100K
  and 1M rows with narrow and wide values.
- [ ] Record time to first row, total time, peak RSS, allocation count, copied
  bytes, and cleanup time.
- [ ] Introduce slab/arena-backed result storage only if amplification is
  confirmed and API lifetime remains correct.
- [ ] Add cold-cache and memory-constrained runs.
- [ ] Add the previously deferred 10M-row qualification as a scheduled/manual
  tier, with explicit disk and time budgets.
- [ ] Add representative Drupal, Laravel, Doctrine, and MediaWiki traces,
  including PDO and ORM hydration.

### `SEC-02` performance integration

- [ ] Add spatial complexity scenarios to the paired performance harness.
- [ ] Enforce slope/operation-count limits for `ST_IsValid()` and deep geometry
  rejection.

### Phase 6 closure

- [ ] Publish raw samples, summaries, profiles, environment metadata, and
  correctness checks.
- [ ] Run existing WordPress/read/join/aggregate/metadata/cache scenarios and
  prove no regression.
- [ ] Update performance documentation with explained residuals rather than
  declaring unattributed time unavoidable.

## Phase 7: Compatibility Evidence, Product Scope, And Application Gates

### `GOV-01`: status semantics

- [ ] Separate behavior fidelity (`compatible`, `partial`, `placeholder`,
  `unsupported`) from evidence state (`specified`, `tested`, `MySQL-verified`,
  `CI-gated`).
- [ ] Reclassify green placeholder/no-op rows without losing evidence links.
- [ ] Publish profile-specific denominators rather than treating all green
  claims as one coverage percentage.
- [ ] Update the legend, maintenance rules, detailed guides, and generated
  evidence reports.

### `GOV-02`: application gate integrity

- [ ] Emit and consume JUnit/XML for every application runner.
- [ ] Enforce minimum executed-test and assertion counts.
- [ ] Establish explicit warning, skip, incomplete, and risky-test budgets with
  reason allowlists.
- [ ] Fail on suite shrinkage, unexpected exclusions, missing reports, or
  parser fallback ambiguity.
- [ ] Publish counts and allowlisted exceptions with each application claim.
- [ ] Expand Drupal and MediaWiki selections and replace two-test
  Laravel/Doctrine bridges with meaningful upstream database selections.

### `GOV-03`: drop-in product profiles

- [ ] Define readiness profiles for embedded C, core PHP, mysqli replacement,
  PDO MyLite, and any future wire-protocol server.
- [ ] Scope README and compatibility claims to the profiles actually
  qualified.
- [ ] Decide whether a MySQL wire protocol is a product requirement.
- [ ] If yes, create a separate authenticated protocol roadmap and release
  gate; if no, remove language implying stock mysqlnd/PDO MySQL compatibility.

### `GOV-04`: claim validator correctness

- [ ] Fix native-test set shadowing so unregistered tests fail validation.
- [ ] Add negative/mutation tests for every claim invariant.
- [ ] Validate CI job-to-runner invocation, exact pins, claim IDs, row text,
  native tests, MySQL fixtures, and relevant application gates.
- [ ] Govern detailed-table statuses directly or label them explicitly as
  non-release claims.
- [ ] Generate a human- and machine-readable evidence graph.

### `GOV-05` and `GOV-06`: actionable backlog and documentation hygiene

- [ ] Generate one normalized backlog from every non-green detailed row.
- [ ] Tag gaps by product profile, application demand, semantic risk,
  implementation state, and evidence state.
- [ ] Prioritize result/error, type, collation, transaction, DML, DDL, join,
  and subquery gaps using application query telemetry.
- [ ] Correct stale Laravel and Doctrine prepared-statement specifications.
- [ ] Close or archive the 18 stale completion/review/commit checklist items.
- [ ] Move the genuine deferred `LAST_INSERT_ID(expr)` residual into the
  normalized backlog.
- [ ] Add linting that prevents completed evidenced features from retaining
  contradictory implementation tasks.

### Phase 7 closure

- [ ] Run the claim validator and all mutation tests.
- [ ] Run changed-surface MySQL fixtures and the complete 794-fixture suite at
  the final commit.
- [ ] Run all application baselines under the new count and exception gates.
- [ ] Independently review every green product-profile claim.

## Phase 8: Architecture And Maintainability

These tasks follow correctness work so refactoring does not obscure behavioral
fixes.

### `ARCH-01`: execution/analyzer translation-unit boundary

- [ ] Record current included-fragment count, preprocessed size, compile time,
  object size, and static-analysis time.
- [ ] Extract prepared INSERT/UPDATE/DELETE plan ownership, matching, analysis,
  lowering, and teardown behind typed interfaces.
- [ ] Continue with cohesive statement families only when ownership and
  dependency direction are explicit.
- [ ] Avoid introducing a universal IR that merely relocates coupling.
- [ ] Add ratchets for execution translation-unit size and included-fragment
  count.
- [ ] Add direct/prepared equivalence tests around each extracted boundary.

### `ARCH-02`: connection and session ownership

- [ ] Define subsystem-owned transaction, variable, diagnostics,
  program-registry, random-state, and catalog-cache contexts.
- [ ] Replace direct cross-module field mutation with typed APIs.
- [ ] Make the connection header aggregate neutral or opaque contexts instead
  of depending upward on execution-specific types.
- [ ] Document synchronization and publication rules for every mutable
  subcontext.
- [ ] Add include and direct-field-access ratchets.

### `ARCH-04`: allocator boundary

- [ ] Define a small internal allocator interface with production, profiling,
  and failpoint backends.
- [ ] Migrate handle, statement, result, parser, plan, and catalog ownership
  before low-value leaf allocations.
- [ ] Remove forced macro interposition once all qualification counters and
  failpoints use the typed boundary.
- [ ] Preserve `mylite_free()` compatibility for public returned allocations.
- [ ] Measure call overhead, binary size, and allocation-accounting fidelity.

### `ARCH-05`: declarative built-in metadata

- [ ] Consolidate positional metadata arrays into typed records or one
  independently authored declarative source.
- [ ] Generate C only into the build directory if generation is selected.
- [ ] Add compile-time counts and runtime registry validation for names,
  indexes, column associations, and provider/queryability status.
- [ ] Differentially verify SHOW and INFORMATION_SCHEMA metadata.

### `ARCH-06`: enforce dependency direction

- [ ] Define allowed include edges for SQL core, storage, catalog, runtime,
  adapters, and generated code.
- [ ] Enforce them through object/interface targets or an include-graph CI
  check while retaining one final library artifact.
- [ ] Add negative tests or fixtures proving forbidden edges fail.

### Phase 8 closure

- [ ] Run formatting, clang-tidy, all build configurations, ABI checks, and
  complete tests after each extraction.
- [ ] Compare compile time, object size, runtime performance, and dependency
  graphs before and after.
- [ ] Update architecture and ownership documentation.

## Phase 9: Build, Packaging, Reproducibility, And Release

### `BUILD-03`: path-independent reproducibility

- [ ] Normalize or suppress checkout-specific Lemon `#line` paths.
- [ ] Include all generation-affecting paths and content in cache identity, or
  make generated output path-independent.
- [ ] Build from two clean checkouts at different absolute paths and compare
  generated sources, archives, binaries, SBOM, provenance, and checksums.
- [ ] Test parser-cache restoration across worktrees and build directories.
- [ ] Scrub environment, locale, umask, timestamp, and source-path variation.

### `BUILD-04`: zlib policy

- [ ] Document zlib rationale, license, supported/minimum version, update
  process, compatibility expectations, and security ownership.
- [ ] Enforce the supported range in CMake.
- [ ] Test the oldest supported and release-pinned versions.
- [ ] Record the actually linked zlib version in qualification and release
  metadata.

### `BUILD-05`: cross-platform shared ABI

- [ ] Add shared-library install/consumer jobs on Linux, macOS, and Windows.
- [ ] Compare complete defined-export inventories with platform-appropriate
  tools, including weak and nonstandard defined symbols.
- [ ] Verify SONAME/install-name/import-library, headers, CMake package,
  pkg-config where applicable, RPATH, and runtime loading.
- [ ] Keep the public ABI snapshot and additive-change policy authoritative.

### `BUILD-06` and `BUILD-07`: environment and size evidence

- [ ] Pin fixed runner images or immutable tool environments where practical.
- [ ] Retain runner-image, compiler, linker, package, and dependency inventories
  with release evidence.
- [ ] Regenerate size reports after the artifacts they describe.
- [ ] Fail when a report and artifact size differ.
- [ ] Run production-size budgets on relevant pull requests and publish
  per-object/per-section reports.
- [ ] Regenerate current-HEAD artifacts and verify static, shared, core PHP,
  mysqli, and PDO modules remain below budget.

### `BUILD-02`: durable release publication

- [ ] Add a post-qualification publication job scoped to exact version tags.
- [ ] Grant narrowly scoped write permission only to that job.
- [ ] Create an immutable release and attach archives, checksums, SBOM,
  provenance, size reports, ABI evidence, and qualification summaries.
- [ ] Verify published assets and attestations through the hosting API.
- [ ] Keep failed or partial qualification from publishing any release.

### `BUILD-01`: licensing release blocker

- [ ] Keep release metadata marked `NOASSERTION` and block public redistribution
  until a project license is selected.
- [ ] Obtain the project-owner license decision.
- [ ] Review first-party code, SQLite, zlib, PHP-facing packages, generated
  code, and release tooling against that decision.
- [ ] Add SPDX identifiers, `LICENSE`, and required notices to source and
  binary packages.
- [ ] Make release validation reject `NOASSERTION` or missing license/notice
  files after the license decision is made.

### Phase 9 closure

- [ ] Run clean reproducible builds from separate checkout roots.
- [ ] Run cross-platform static/shared install consumers and ABI checks.
- [ ] Run a dry-run tag release and verify every permanent asset.
- [ ] Do not claim redistribution readiness until `BUILD-01` is complete.

## Phase 10: Final Qualification And Closure

### Complete technical qualification

- [ ] Run Release and assertion-enabled Debug suites on GCC, Clang, Apple
  Clang, and Windows Clang.
- [ ] Run complete ASan/UBSan, native LSan, deterministic TSan, fuzz,
  allocator-failpoint, VFS-failpoint, crash, migration, and multi-process
  suites.
- [ ] Run all core, PHP adapter, WordPress, Drupal, Laravel, Doctrine, and
  MediaWiki tests under their count/exception gates.
- [ ] Run changed-surface and full 794-fixture MySQL 8.4.9 expectations.
- [ ] Run coverage thresholds, formatting, clang-tidy, ABI, installation,
  packaging, reproducibility, size, and release-tooling checks.
- [ ] Run paired performance qualification at normal scale, 1M rows, and the
  scheduled/manual 10M-row tier.
- [ ] Confirm no temporary databases, profiles, generated files, containers,
  or review agents remain.

### Independent closure review

- [ ] Re-run each original reproducer against the audited commit and final
  candidate, recording fail-before/pass-after evidence.
- [ ] Trace every finding ID through specification, code, tests, CI, and docs.
- [ ] Review all public ABI and file-format changes for compatibility.
- [ ] Review all concurrency transitions, plan invalidation rules, seal
  publication, and recovery boundaries.
- [ ] Review all parser/spatial untrusted-input paths for bounded stack, heap,
  CPU, and cleanup.
- [ ] Review every compatibility green row changed by this work.
- [ ] Update the whole-project assessment with final evidence and explicitly
  list any consciously deferred product work.
- [ ] Close the technical follow-up only when every non-license item below is
  complete and an independent review finds no remaining issue.

## Finding Coverage Matrix

| ID | Finding | Primary phase | Required closure evidence |
| --- | --- | --- | --- |
| `COR-01` | Stale DDL/DML plans before writer lock | 1 | Deterministic concurrency tests, TSan, reopen integrity |
| `COR-02` | Concurrent migration does not converge | 1 | Two-process N-1 migration and fault tests |
| `PREP-01` | Parameterless SELECT locks at prepare time | 2 | Native and PHP MySQL differential tests |
| `PREP-02` | Prepared SHOW/EXPLAIN replay stale rows | 2 | Mutation-between-reset tests |
| `API-01` | mysqli pending result is discarded | 2 | Error 2014 state-machine matrix |
| `API-02` | PDO statement diagnostics cross-contaminate | 2 | Independent live-statement diagnostics |
| `API-03` | Prepared PHP numeric values are stringified | 2 | Boundary type differential matrix |
| `API-04` | PDO server version reports package version | 2 | Attribute and `VERSION()` parity |
| `API-05` | Warning ABI and iteration are incomplete | 2 | Multi-warning direct/prepared tests |
| `API-06` | PDO metadata/row count and mysqli thread ID gaps | 2 | Metadata and multi-handle tests |
| `API-07` | Laravel/Doctrine are test bridges, not normal integration | 2, 7 | Fresh configured application tests |
| `SEC-01` | Unbounded spatial nesting can crash the host | 3 | Large-input sanitizer and fuzz tests |
| `SEC-02` | `ST_IsValid()` is quadratic and uncancellable | 3, 6 | Scaling, cancellation, and budget evidence |
| `SEC-03` | Embedded NUL truncates PHP database paths | 2 | Core/mysqli/PDO path tests and length-aware API |
| `SEM-01` | Fixed spatial epsilon changes topology/distance | 3 | Translation/scale differential matrix |
| `SEM-02` | `CONVERT_TZ()` ignores MySQL range behavior | 5 | Boundary differential tests |
| `SEM-03` | Function-family result metadata is inaccurate | 5 | Complete field-metadata comparison |
| `SQL-01` | Parser retry downgrades `NOMEM` | 4 | Allocation failpoint matrix |
| `SQL-02` | Recovery permits large heap amplification | 4 | Work/memory budgets and scaling tests |
| `SQL-03` | Retry AST rebasing misses payload spans | 4 | Snapshot/span invariant tests |
| `SQL-04` | Huge syntax diagnostic has unsafe cast/drift | 4 | Large-input safety and exact MySQL messages |
| `SQL-05` | Fixed parser-stack compatibility limit is unqualified | 4 | Nesting sweep and explicit contract |
| `STOR-01` | Integrity seal misses raw catalog corruption | 1 | Structured corruption and open-time rejection |
| `STOR-02` | Interrupted first creation is not recoverable | 1 | Cross-platform process-death recovery |
| `STOR-03` | Crash/fault matrix is narrower than DDL surface | 1 | Expanded VFS/crash matrix |
| `PERF-01` | Durable autocommit is not qualified | 6 | Block-device p50/p95/p99 and sync counts |
| `PERF-02` | Retained-write 3.67x residual is unattributed | 6 | Four-layer attribution and paired results |
| `PERF-03` | `LOAD DATA` has per-byte/per-field overhead | 6 | Allocation and throughput improvement |
| `PERF-04` | Buffered/cold/10M/non-WordPress gaps remain | 6 | RSS, cold, scale, and application evidence |
| `GOV-01` | Green conflates compatibility and placeholders | 7 | Two-axis status model and migration |
| `GOV-02` | Application gates ignore warnings/skips/shrinkage | 7 | JUnit count and exception budgets |
| `GOV-03` | "Drop-in" exceeds qualified product profiles | 7 | Profile definitions and wire decision |
| `GOV-04` | Claim validator has ineffective registration check | 7 | Fix plus negative validator tests |
| `GOV-05` | Compatibility gap inventory is not actionable | 7 | Generated prioritized backlog |
| `GOV-06` | Specs and task lists contain stale state | 7 | Cleanup plus drift lint |
| `ARCH-01` | Execution/analyzer translation unit remains oversized | 8 | Typed extraction and size ratchets |
| `ARCH-02` | Connection object is a dependency hub | 8 | Owned subcontexts and access ratchets |
| `ARCH-03` | Parser retries are a second recognizer | 4 | Retry inventory and zero-growth ratchet |
| `ARCH-04` | Allocation instrumentation uses macro interposition | 8 | Typed allocator backends |
| `ARCH-05` | Built-in metadata uses fragile positional declarations | 8 | Typed/generated declarations and validation |
| `ARCH-06` | Layering is not enforced by the build graph | 8 | Include-edge CI enforcement |
| `BUILD-01` | Project license is unresolved | 9 | License decision, SPDX, notices, package gate |
| `BUILD-02` | Tag workflow does not publish durable releases | 9 | Verified permanent release assets |
| `BUILD-03` | Generated/release reproducibility is path-sensitive | 9 | Two-root reproducible build |
| `BUILD-04` | zlib policy and version range are undefined | 9 | Documented/enforced dependency policy |
| `BUILD-05` | Shared ABI checks are Linux-only/incomplete | 9 | Cross-platform full export checks |
| `BUILD-06` | Quality environments remain partly mutable | 9 | Pinned/inventoried environments |
| `BUILD-07` | Size reports can be stale | 9 | Artifact/report freshness gate |

## Completion Rule

The technical chapter may close only when every finding except `BUILD-01` is
checked in the matrix, every required workflow is green at the same candidate
commit, and the independent closure review reports no remaining correctness,
security, compatibility, performance, architecture, or packaging finding.

Redistribution readiness additionally requires `BUILD-01`.
