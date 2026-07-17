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

- [ ] Specify typed parameter slots, bind lifetime, rebinding, reset, and
  schema/session invalidation for `mylite_stmt`.
- [ ] Add public NULL, signed integer, unsigned integer, floating-point, text,
  and blob binding APIs with length-aware values.
- [ ] Make lowered plans carry parameter descriptors and verify generated
  placeholder count and order against SQLite.
- [ ] Parse and prepare at prepare time; execute without reconstructing SQL.
- [ ] Migrate the core PHP statement API to native binding.
- [ ] Migrate the mysqli statement API to native binding and the cursor result
  path.
- [ ] Migrate PDO to native binding while preserving PDO type and lifetime
  semantics.
- [ ] Cover markers in strings, identifiers, ordinary comments, executable
  comments, binary data, embedded NULs, all relevant SQL modes, repeated
  execution, schema invalidation, and prepare-time versus execute-time errors.
- [ ] Re-run the `NO_BACKSLASH_ESCAPES` injection probes through all adapters.

### Source spans and token-aware normalization

- [x] Rebase nested executable-comment tokens and AST spans to absolute input
  offsets.
- [ ] Validate every source span using overflow-safe offset/length checks.
- [ ] Enforce executable-comment version gates for standalone and embedded
  forms.
- [ ] Replace SQL-sensitive raw substring rewriting with token- or AST-aware
  transformations.
- [ ] Prevent SET rewrites inside strings, identifiers, and comments.
- [ ] Preserve user-visible expression text instead of internal helper names.
- [x] Replace raw-SQL INFORMATION_SCHEMA bridge dispatch with typed plan data.
- [ ] Add successful, invalid, retry, nested-comment, and sanitizer coverage.

## Phase 2: Handle and transaction lifecycle

### Ownership and close behavior

- [x] Register live statements and cursors with their owning connection.
- [x] Define a fallible close contract or connection-core lifetime model and
  expose it consistently through the public ABI.
- [ ] Make step, reset, finalize, metadata access, and adapter destruction safe
  in every close order.
- [ ] Distinguish SQL NULL from empty text/blob in the streaming value API.
- [ ] Document database, statement, cursor, result, and returned-text lifetime.

### Statement completion and diagnostics

- [ ] Introduce one statement completion record for diagnostics, warnings,
  affected rows, insert ID, `ROW_COUNT()`, and `FOUND_ROWS()`.
- [x] Prevent delayed cursor exhaustion/finalization from overwriting state
  produced by a later statement.
- [ ] Replace broad cursor fallback with a typed unsupported-capability result.
- [ ] Preserve the original diagnostic when fallback is not permitted.

### Transaction truthfulness

- [x] Route mysqli autocommit through the core transaction state machine.
- [ ] Audit adapter methods that report success while discarding arguments.
- [x] Check and propagate savepoint rollback, release, full rollback, and
  commit cleanup failures.
- [x] Preserve the primary error and append cleanup context.
- [x] Poison handles whose atomicity or transaction state cannot be proven and
  reject further SQL until close.
- [x] Add autocommit transition, DDL, savepoint, error, close, and injected
  rollback/commit failure coverage.

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
- [ ] Add multi-process barriers, replacement/symlink tests, and VFS fault
  injection around create, open, sync, truncate, rename, and close.

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
- [ ] Stream metadata rows and push down predicates, projection, aggregation,
  ordering prerequisites, and LIMIT where valid.
- [x] Allocate only projected values and add bounded-memory count/limit paths.

### Collations, aggregates, and remaining semantic gaps

- [ ] Implement one shared character-set/collation service for comparison,
  equality, ordering, DISTINCT, grouping, indexes, and aggregates.
- [ ] Correct UTF-8 case/accent behavior for supported MySQL collations.
- [ ] Make `GROUP_CONCAT(DISTINCT)` use collation-aware equality and replace
  quadratic duplicate detection.
- [ ] Make numeric parsing/formatting locale-independent and reject partial
  conversions consistently.
- [ ] Define SQL PREPARE prepare-time/session-state semantics.
- [x] Synchronize PROCESSLIST session snapshots and replace quadratic sorting.

## Phase 5: Architecture, performance, and size

### Planning and execution boundaries

- [ ] Promote existing plan families into a typed analyzer boundary containing
  resolved object IDs, expression types/collations, parameters, side effects,
  and diagnostics.
- [ ] Remove execution-time AST dependencies incrementally.
- [ ] Split `mylite_execution.c` into cohesive translation units with explicit
  internal APIs and preserved caller-before-callee organization.
- [ ] Separate mutable session publication from statement-owned collections.
- [ ] Add scoped allocator and VFS failpoints used by qualification tests.

### Measured performance work

- [ ] Add allocation, descriptor-copy, metadata-step, parser-retry, statement-
  cache, and plan-cache counters to the benchmark/profile surface.
- [ ] Replace fixed 15.6 KiB column descriptors with compact hot metadata and
  separately owned/interned cold strings.
- [ ] Budget caches by bytes and use generation-safe borrowed/pinned spans.
- [ ] Bound invalid-SQL parser recovery work and make nested-parenthesis scans
  linear after measuring current scaling.
- [ ] Reuse analyzed prepared plans by schema generation and relevant session
  state after native binding is complete.
- [ ] Measure and correct administrative cache/concurrency scaling.

### Build, size, and packaging

- [ ] Add reproducible per-object and per-section size reports.
- [ ] Add controlled production LTO, function/data sections, linker garbage
  collection, and stripping profiles across supported toolchains.
- [ ] Compact generated metadata and measure parser state/table contributors.
- [ ] Add `lempar.c` as a parser-generation dependency and include first-party
  `.inc` files in format validation.
- [ ] Add installable headers, exported CMake/pkg-config targets, clean consumer
  tests, shared-library policy, ABI checks, and PHP packaging independent of
  checkout build paths.

## Phase 6: Release qualification

- [ ] Add assertion-enabled Debug jobs and representative Release jobs.
- [ ] Add reproducible ASan+UBSan presets and run the complete core suite.
- [ ] Add focused LSan and deterministic TSan concurrency tiers.
- [ ] Add first-party fuzz targets for the lexer/parser, normalizer, JSON,
  geometry, preamble, and catalog inputs.
- [ ] Add model-based DDL/DML, metamorphic rollback/reopen, multi-process crash,
  and power-failure/fault-injection tests.
- [ ] Add CTest labels, timeouts, resource controls, and failure artifacts.
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
