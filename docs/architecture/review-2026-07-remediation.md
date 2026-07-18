# July 2026 Engineering Assessment Remediation Plan

## Objective

This plan closes every technical issue identified in the
[July 2026 whole-project assessment](review-2026-07.md). Project licensing is
deliberately excluded. An item is complete only after its implementation,
focused regression coverage, broader verification, and documentation are all
complete.

## Delivery rules

- Preserve MySQL 8.4.9 behavior and verify semantic changes against the real
  runtime where applicable.
- Land independently reviewable commits in dependency order.
- Reproduce each defect in a test before or with its correction.
- Do not replace truthful diagnostics with silent fallback behavior.
- Run focused tests after every unit and the complete affected suites before
  committing it.
- Keep ABI additions explicit and document ownership, lifetime, nullability,
  invalidation, and error behavior.
- Treat sanitizer, concurrency, and fault-injection failures as correctness
  failures, not optional hardening.
- Record measured baselines before performance or size changes and retain only
  changes with a demonstrated benefit.

## Phase 1: Native parameters and SQL front-end safety

### Typed statement binding

- [x] Specify typed parameter slots, bind lifetime, rebinding, reset, and
  schema/session invalidation for `mylite_stmt`.
- [x] Add public NULL, signed integer, unsigned integer, floating-point, text,
  and blob binding APIs with length-aware values.
- [x] Make lowered plans carry parameter descriptors and verify generated
  placeholder count and order against SQLite.
- [x] Parse and prepare at prepare time; execute without reconstructing SQL.
- [x] Migrate the core PHP statement API to native binding.
- [x] Migrate the mysqli statement API to native binding and the cursor result
  path.
- [x] Migrate PDO to native binding while preserving PDO type and lifetime
  semantics.
- [x] Migrate SQL `PREPARE`/`EXECUTE` to a retained native statement with typed,
  length-aware user-variable binding and no execute-time SQL reconstruction.
- [x] Cover markers in strings, identifiers, ordinary comments, executable
  comments, binary data, embedded NULs, all relevant SQL modes, repeated
  execution, schema invalidation, and prepare-time versus execute-time errors.
- [x] Re-run the `NO_BACKSLASH_ESCAPES` injection probes through all adapters.
  Native coverage distinguishes default double-quoted strings from
  `ANSI_QUOTES` identifiers and verifies active executable-comment markers;
  core PHP, mysqli, and PDO each preserve hostile text as data while the schema
  and existing rows remain unchanged.

### Source spans and token-aware normalization

- [x] Rebase nested executable-comment tokens and AST spans to absolute input
  offsets.
- [x] Validate every source span using overflow-safe offset/length checks.
- [x] Enforce executable-comment version gates for standalone and embedded
  forms.
- [x] Replace SQL-sensitive raw substring rewriting with token- or AST-aware
  transformations.
- [x] Prevent SET rewrites inside strings, identifiers, and comments.
- [x] Preserve user-visible expression text instead of internal helper names.
- [x] Replace raw-SQL INFORMATION_SCHEMA bridge dispatch with typed plan data.
- [x] Add successful, invalid, retry, nested-comment, and sanitizer coverage.

## Phase 2: Handle and transaction lifecycle

### Ownership and close behavior

- [x] Register live statements and cursors with their owning connection.
- [x] Define a fallible close contract or connection-core lifetime model and
  expose it consistently through the public ABI.
- [x] Make step, reset, finalize, metadata access, and adapter destruction safe
  in every close order.
- [x] Distinguish SQL NULL from empty text/blob in the streaming value API.
- [x] Document database, statement, cursor, result, and returned-text lifetime.
  Live statements are registered and detached before connection destruction;
  detached operations return documented sentinels or `MYLITE_MISUSE`, and
  finalization remains valid. Core PHP and mysqli cover connection-first and
  statement-first teardown, while PDO verifies its statement-held database
  reference. The complete native close-order matrix passes under ASan+UBSan.

### Statement completion and diagnostics

- [x] Introduce one statement completion record for diagnostics, warnings,
  affected rows, insert ID, `ROW_COUNT()`, and `FOUND_ROWS()`.
- [x] Prevent delayed cursor exhaustion/finalization from overwriting state
  produced by a later statement.
- [x] Replace broad cursor fallback with a typed unsupported-capability result.
- [x] Preserve the original diagnostic when fallback is not permitted. Cursor
  planning now distinguishes ready, unsupported, and failed attempts. Only an
  explicitly tagged planner capability gap or a recognized built-in metadata
  dispatch may materialize; semantic, catalog, allocation, and SQLite failures
  propagate without clearing their diagnostics. Direct, native prepared,
  streaming, buffered, materialized, failed, and parse-failed statements now
  capture one owned completion record and publish it through a single
  publish-once boundary. Stale materialized cursors cannot replace a later
  statement's row count, found rows, warnings, or error snapshot. The focused
  runtime and all PHP adapter suites pass, including cursor and diagnostics
  coverage under ASan+UBSan.

### Transaction truthfulness

- [x] Route mysqli autocommit through the core transaction state machine.
- [x] Audit adapter methods that report success while discarding arguments.
- [x] Check and propagate savepoint rollback, release, full rollback, and
  commit cleanup failures.
- [x] Preserve the primary error and append cleanup context.
- [x] Poison handles whose atomicity or transaction state cannot be proven and
  reject further SQL until close.
- [x] Add autocommit transition, DDL, savepoint, error, close, and injected
  rollback/commit failure coverage. mysqli transaction start, chain, no-chain,
  and no-release flags now lower to supported MyLite SQL; conflicting flags,
  release, names, network ports, client flags, asynchronous modes, connection
  options, TLS/debug controls, and statement attributes return explicit
  unsupported diagnostics instead of false success. Both store-result wrappers
  validate their advertised mode, and custom-object fetching invokes
  constructors with populated row properties and positional or named
  arguments. PDO rejects scroll cursors, non-next fetch orientations, offsets,
  and named insert-ID sequences. The complete eight-test PHP adapter matrix
  passes.

## Phase 3: Storage and generated identities

### File identity, initialization, and recovery

- [x] Bind create, validation, SQLite open, and failure cleanup to one proven
  file identity.
- [x] Ensure failed-open cleanup never unlinks an unverified pathname.
- [x] Add explicit preamble initialization states and deterministic interrupted
  initialization recovery.
- [x] Reject preamble-only, replaced, truncated, or mismatched files without
  silently reinitializing them.
- [x] Qualify hot rollback-journal recovery and enforce the explicit no-WAL
  policy through the shifted VFS.
- [x] Add multi-process barriers, replacement/symlink tests, and VFS fault
  injection around create, open, sync, truncate, rename, and close. The
  file-backed lifecycle suite now coordinates a live initialization owner and
  competing opener through explicit ready/release files on POSIX and Windows,
  preserves the opened identity across pathname replacement, and verifies that
  a rejected symlink target and its link survive failed open cleanup. One-shot
  thread-local shifted-VFS failpoints cover create, existing open, write, sync,
  truncate, delete, and close without cross-test or cross-thread leakage. The
  injected initial-write path also proves cleanup publishes a complete
  recovery-required preamble. The focused suite passes in Debug and under
  ASan+UBSan, including post-fault reopen integrity.

### Lock-byte address mapping

- [x] Enforce a deterministic logical size limit below the first unsafe
  shifted lock-byte page as immediate containment.
- [x] Add a SQLite extension point or revise the format so the physical
  pending-lock page and SQLite pager reservation agree.
- [x] Verify all VFS offset-sensitive controls, sector/device properties, size
  hints, and future mmap/shared-memory behavior.
- [x] Test below, at, and above the boundary on POSIX and Windows across
  supported page sizes and journal modes.

### AUTO_INCREMENT integrity

- [x] Acquire the serialized writer boundary before reading or reserving the
  high-water mark.
- [x] Keep reservation, row mutation, and counter advancement in one explicit
  transaction protocol.
- [x] Prevent generated ODKU keys from targeting rows committed after planning.
- [x] Define clean-close persistence and document process-death gap reuse as an
  explicit single-writer storage limitation.
- [x] Cover plain INSERT, IGNORE, REPLACE, ODKU, multi-row, explicit values,
  rollback, savepoints, concurrent handles, close, reopen, and process death.

## Phase 4: Catalog, metadata, and SQL semantics

### Catalog integrity and compatibility

- [x] Validate catalog table definitions, indexes, constraints, relationships,
  ordinals, and physical-schema correspondence on open.
- [x] Reject missing physical tables and orphaned catalog children.
- [x] Define separate reader and writer format compatibility rules.
- [x] Test actual N and N-1 binaries against the declared minimum-reader
  contract.
- [x] Replace unchecked system-metadata parallel arrays with typed records or
  generated definitions with static validation.
- [x] Pin borrowed key metadata or give it an explicit acquisition/release
  lifetime.

### Metadata correctness and scale

- [x] Produce MySQL-compatible type, flags, charset, collation, length, and
  nullability descriptors for synthetic metadata.
- [x] Remove positive synthetic storage/process state that MyLite cannot
  truthfully observe, or expose explicit capability provenance.
- [x] Reuse cached combined key metadata for SHOW and INFORMATION_SCHEMA.
- [x] Add direct exact schema/table lookup paths for COLUMNS and STATISTICS.
- [x] Stream metadata rows and push down predicates, projection, aggregation,
  ordering prerequisites, and LIMIT where valid.
- [x] Allocate only projected values and add bounded-memory count/limit paths.

### Collations, aggregates, and remaining semantic gaps

- [x] Implement one shared character-set/collation service for comparison,
  equality, ordering, DISTINCT, grouping, indexes, and aggregates.
- [x] Correct UTF-8 case/accent behavior for supported MySQL collations.
- [x] Make `GROUP_CONCAT(DISTINCT)` use collation-aware equality and replace
  quadratic duplicate detection.
- [x] Make numeric parsing/formatting locale-independent and reject partial
  conversions consistently. Runtime numeric text now uses one cached C-locale
  service; strict consumers validate the returned end pointer, while MySQL
  coercion paths retain their deliberate numeric-prefix behavior. Locale
  coverage exercises JSON, spatial, decimal, SYS, system-variable, and
  approximate-number paths under a comma-decimal locale.
- [x] Define SQL PREPARE prepare-time/session-state semantics. SQL-level handlers
  now retain lexer modes, default schema, and connection charset/collation;
  statement-effective accessors preserve those parse/name/literal semantics
  while parameter values, session-variable reads, and validation modes remain
  execute-time.
- [x] Synchronize PROCESSLIST session snapshots and replace quadratic sorting.

## Phase 5: Architecture, performance, and size

### Planning and execution boundaries

- [ ] Promote existing plan families into a typed analyzer boundary containing
  resolved object IDs, expression types/collations, parameters, side effects,
  and diagnostics.
- [ ] Remove execution-time AST dependencies incrementally. Row-scalar SELECT
  items now retain owned result labels, normalized aliases, and compact typed
  source-metadata shapes; SQL lowering, result metadata, derived-source
  construction, and ORDER BY alias resolution no longer dereference their
  source-expression or alias AST nodes. Result setup also avoids the former
  per-execution label allocation. Standalone COUNT and column-aggregate plans
  likewise own their normalized result labels; aggregate execution no longer
  retains expression/alias AST nodes or allocates labels for those families.
  Multi-item COUNT-expression aggregates use the same analyzed-label
  ownership and release it through every partial-planning failure path.
  Grouped projections, GROUP BY keys, and grouped aggregate items now retain
  typed columns/scalar expressions plus owned normalized labels only. GROUP
  BY, HAVING, ORDER BY, derived-source construction, lowering, and result
  metadata consume those analyzed names without expression/alias AST access.
  UPDATE and `ON DUPLICATE KEY UPDATE` same-column arithmetic plans now retain
  the parsed delta and its validity instead of a literal AST pointer. Deferred
  execution errors remain conditional on a matching UPDATE row or an actual
  duplicate-key evaluation.
- [ ] Split `mylite_execution.c` into cohesive translation units with explicit
  internal APIs and preserved caller-before-callee organization.
- [ ] Separate mutable session publication from statement-owned collections.
- [x] Add scoped allocator and VFS failpoints used by qualification tests.
  The dedicated `fault-injection` preset force-includes a test-only allocator
  shim into first-party library sources; production and profiling builds keep
  direct libc allocation. Its one-shot thread-local control sweeps every MyLite
  allocation reached by open and representative statement execution, checking
  fatal `MYLITE_NOMEM` ownership, valid optional-allocation fallback, and
  post-failure handle recovery. Shifted-VFS failpoints remain independently
  scoped by operation and thread.

### Measured performance work

- [x] Add allocation, descriptor-copy, metadata-step, parser-retry, statement-
  cache, and plan-cache counters to the benchmark/profile surface.
  Normalization, parse, SELECT-plan, SELECT-lowering, and retained-plan cache
  counters were already present. Descriptor-copy bytes, catalog metadata
  steps, parser retry attempts/acceptances, and execution/catalog statement
  cache outcomes are now included in runtime snapshots and benchmark JSON with
  focused tests. Profiling builds force-include a portable allocator shim for
  the MyLite target, while production builds retain direct libc allocation.
- [ ] Replace fixed 15.6 KiB column descriptors with compact hot metadata and
  separately owned/interned cold strings. As an intermediate ownership step,
  SELECT aggregates and INSERT/UPDATE/DELETE plans now pin generation-safe
  cached column spans instead of cloning them. This removed all measured full
  descriptor copies from the six-query WordPress frontend and five-query write
  scenarios, reducing cumulative requested allocation by 14.2% and 16.9%,
  respectively. Descriptor SELECT projections now retain immutable pointers
  into those pinned spans instead of copying one 15.6 KiB descriptor per
  output column. That reduced another WordPress frontend allocation sample
  from 108.6 MB to 93.0 MB over 50 requests (14.4%) and reduced the Release
  128-column projection median from 268.7 us to 116.6 us (56.6%). Explicit
  name resolution returns a source reference directly; copy-returning callers
  keep their existing owned semantics. Predicate nodes now retain references
  to a predicate-owned, identity-deduplicated descriptor set instead of
  embedding two descriptors in every node; a compile-time bound keeps nodes at
  512 bytes or less. A balanced 2,048-leaf OR scenario fell from 75.7 ms to
  10.2 ms median in controlled Release builds (7.4x). Row-scalar expressions
  now allocate descriptors only for the zero to three column references they
  actually use and release them through the existing iterative teardown. The
  expression structure fell from 47,488 bytes to 640 bytes (98.7%), and a
  controlled 128-expression projection fell from 9.291 ms to 1.764 ms median
  (5.3x). The nested `planned_column_aggregate` consequently fell from 183,104
  bytes to 42,560 bytes. Compile-time bounds keep expression state below 1 KiB.
  ORDER BY plans now use independently owned descriptor pointers and geometric
  item growth, preserving planner-family lifetimes while reducing order items
  from 15,920 bytes to 304 bytes and order state from 15,664 bytes to 56 bytes.
  A controlled 128-key ORDER BY scenario fell from 1.651 ms to 0.998 ms median
  (39.6%). Grouped keys and projections now use the same stable owned-reference
  model, grouped projection arrays grow geometrically, and aggregate items use
  an item-level ownership boundary shared by stored and temporary plans. Grouped
  keys/projections fell from 16,296 bytes to 680 bytes, grouped aggregate items
  from 17,128 bytes to 1,512 bytes, and column aggregates from 26,952 bytes to
  11,336 bytes. A controlled 128-projection grouped query fell from 2.719 ms to
  1.365 ms median (49.8%). The underlying catalog descriptor representation
  itself remains open for compact hot metadata and separately owned cold text.
- [x] Budget caches by bytes and use generation-safe borrowed/pinned spans.
  Column and deep key-metadata payloads now have independent 8 MiB limits in
  addition to their 64-entry caps. Insertion evicts only unpinned LRU entries;
  oversized or fully pinned workloads retain statement-owned metadata instead.
  End-to-end coverage warms both caches with 40 wide composite-key tables and
  verifies bounded bytes, eviction, pin survival, and release after
  invalidation.
- [x] Bound invalid-SQL parser recovery work and make nested-parenthesis scans
  linear after measuring current scaling. Predicate retries now precompute one
  byte of WHERE-clause context per token instead of repeatedly rescanning the
  token prefix. A 64-expression malformed SELECT with 128 balanced parenthesis
  levels per expression fell from 23.551 seconds to 0.096 seconds in the same
  Debug build (about 245x), and remains covered under ASan+UBSan. Reusing the
  fixed Lemon parser allocation was also prototyped but showed no reliable
  normal-allocator improvement, so that added lifetime complexity was not
  retained.
- [x] Reuse analyzed prepared plans by schema generation and relevant session
  state after native binding is complete. Native prepared SELECT plans retain
  stable typed parameter slots, the input-type signature used during analysis,
  and lowered SQL across reset when parameters occupy direct comparison,
  BETWEEN, or IN-list value positions. Current values are read from
  statement-owned bindings only when SQLite parameters are bound; a changed
  input type or catalog/SQLite schema generation forces reanalysis. The reuse
  key also covers SQL mode, time-zone conversion offset, `LAST_INSERT_ID()`, and
  `sql_auto_is_null`, the session values currently consumed during SELECT
  analysis. Parameter uses that can affect structure or metadata, including
  projections, function options, and LIMIT/OFFSET, conservatively disable reuse.
  A profiling regression verifies stable-type value changes, text/integer type
  changes, schema and session invalidation, scalar-projection fallback, and
  dynamic LIMIT behavior. The WordPress
  prepared option lookup records 1,000 plan and lowering cache hits with zero
  rebuilds; its median per-request p50 across seven pinned Release samples fell
  from about 33.9 us to 21.8 us (35.7%).
- [x] Measure and correct administrative cache/concurrency scaling. Descriptor
  caches use LRU replacement rather than fixed-slot churn, PROCESSLIST publishes
  synchronized session snapshots and sorts with `qsort`, and ordinary writes
  on another handle no longer discard structural table descriptors. A changed
  SQLite data version with an unchanged catalog generation now marks only the
  three mutable table-status fields stale; the next table read refreshes those
  fields once from the reader's established snapshot. Cross-handle lifecycle
  coverage proves status freshness while tracing zero full descriptor reads and
  one narrow status read. The threaded read/write benchmark remained dominated
  by scheduler and SQLite locking noise, so no latency improvement is claimed.

### Build, size, and packaging

- [x] Add reproducible per-object and per-section size reports. The
  `mylite_size_report` target records decimal section/object sizes and a sorted
  symbol inventory without embedding checkout-specific artifact paths.
- [x] Add controlled production LTO, function/data sections, linker garbage
  collection, and stripping profiles across supported toolchains. The retained
  production profile uses section GC and install-time stripping. In a
  controlled GCC comparison, section GC reduced the core PHP module from
  9,125,944 to 9,017,288 bytes and its loaded sections from 8,633,107 to
  8,536,167 bytes. Install stripping reduced the packaged module to 8,544,368
  bytes. LTO increased the module and loaded-section sizes by about 1.9% and
  2.6%, respectively, so it remains opt-in instead of being enabled by the
  production preset.
- [x] Compact generated metadata and measure parser state/table contributors.
  Qualified SYS `SHOW CREATE VIEW` statements are now reconstructed from the
  canonical executable definition, system-table column registry, and the
  unqualified MySQL text instead of retaining a second full SQL copy for each
  of 100 views. Registry-wide equivalence coverage proved both forms before
  removal, and all SYS view tests exercise the compact path. The production
  shared library fell from 8,995,352 to 8,827,080 bytes (1.9%), loaded sections
  from 8,517,459 to 8,359,818 bytes (1.9%), and the SYS-view object from 439,572
  to 285,950 bytes (34.9%). The generated Lemon parser contributes 1,485,419
  loaded bytes; its compressed action and lookahead tables account for
  1,282,038 bytes. Lemon's default compressed-table mode is already in use, and
  disabling compression or eliminating state resorting did not produce a valid
  smaller parser, so no parser-generation tradeoff was retained.
- [x] Add `lempar.c` as a parser-generation dependency and include first-party
  `.inc` files in format validation.
- [x] Add installable headers, exported CMake/pkg-config targets, clean consumer
  tests, shared-library policy, ABI checks, and PHP packaging independent of
  checkout build paths. Static and shared packages build and execute clean
  CMake and pkg-config consumers from an isolated install prefix. Shared builds
  are versioned with SOVERSION 0 and checked against an explicit public-symbol
  manifest. PHP modules install to a configurable package directory and consume
  the exported public-header target.

## Phase 6: Release qualification

- [x] Add assertion-enabled Debug jobs and representative Release jobs. The
  684-test Debug suite passes locally and CI retains the four-platform Release
  matrix alongside a dedicated Clang Debug job.
- [ ] Add reproducible ASan+UBSan presets and run the complete core suite. The
  preset and focused prepared-statement qualification are complete; the full
  suite remains to be run in CI.
- [ ] Add focused LSan and deterministic TSan concurrency tiers. Leak detection
  and a labeled two-test TSan tier pass locally; remaining sleep-based test
  coordination and CI coverage remain open.
- [x] Add first-party fuzz targets for the lexer/parser, normalizer, JSON,
  geometry, preamble, and catalog inputs. Six Clang libFuzzer targets run under
  ASan+UBSan with copied seed corpora and deterministic bounded smoke budgets;
  the local tier passes 51,000 in-memory mutations and 1,000 database opens.
- [ ] Add model-based DDL/DML, metamorphic rollback/reopen, multi-process crash,
  and power-failure/fault-injection tests.
- [x] Add CTest labels, timeouts, resource controls, and failure artifacts.
  Every native test has a bounded timeout and functional label, concurrency
  tests retain serial resource controls, and all native CI configurations
  publish JUnit results on success or failure.
- [ ] Create stable compatibility claim IDs and enforce
  claim-to-spec-to-MySQL-fixture-to-native-test-to-CI mappings.
- [ ] Run changed MySQL fixtures on pull requests and all 792 scripts in
  sharded nightly/release tiers.
- [ ] Add coverage reports with ratcheted module thresholds.
- [ ] Add stable-runner benchmark history and statistically tolerant
  regression checks.
- [ ] Pin mutable CI/container/tool inputs and add a formal reproducible release
  workflow with checksums, provenance, and an SBOM excluding license selection.

## Final closure audit

- [ ] Every checkbox above has direct code/test/documentation evidence.
- [ ] All focused reproducers fail on the audited base commit and pass on the
  remediated tree.
- [ ] Debug, Release, sanitizer, concurrency, crash, MySQL, PHP, application,
  installation, ABI, size, and performance tiers pass.
- [ ] An independent code review finds no remaining assessment item or
  regression hidden by fallback behavior.
- [ ] The whole-project assessment is updated with the final evidence and no
  technical stop-ship finding remains.
