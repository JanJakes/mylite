# Baseline Implementation Strategy

## Status

This document records the baseline implementation strategy for MyLite. It is an
architecture and implementation plan, not a supported MySQL compatibility
feature by itself. It does not move any row in `COMPATIBILITY.md` out of
unsupported status.

The goal is to implement the baseline compatibility matrix using SQLite as the
storage and relational execution foundation, while keeping MySQL compatibility
semantics in MyLite-owned code. SQLite public extension APIs should be used
whenever they provide the right surface. Targeted SQLite fork patches are
reserved for behavior that must be visible inside SQLite before, during, or
immediately after bytecode generation and execution.

## Sources

- MyLite README architecture:
  `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MyLite compatibility baseline:
  `COMPATIBILITY.md`
- MyLite parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- SQLite source snapshot notes:
  `third_party/sqlite/README.md`
- `sqlite-fork` branch commit
  `758a673944a57d56f08a6e56bcccf9c7cf9f99a8`
- SQLite application-defined functions:
  https://www.sqlite.org/appfunc.html
- SQLite application-defined collations:
  https://www.sqlite.org/c3ref/create_collation.html
- SQLite virtual tables:
  https://www.sqlite.org/vtab.html
- SQLite VFS:
  https://www.sqlite.org/vfs.html
- SQLite authorizer:
  https://sqlite.org/c3ref/set_authorizer.html
- SQLite pre-update hook:
  https://www.sqlite.org/c3ref/preupdate_blobwrite.html
- SQLite Lemon parser generator:
  https://www.sqlite.org/lemon.html
- SQLite architecture:
  https://www.sqlite.org/arch.html
- SQLite ALTER TABLE generalized procedure:
  https://www.sqlite.org/lang_altertable.html#otheralter

This specification is independently authored from project documentation,
observed repository experiments, and public SQLite documentation. It does not
copy MySQL, MariaDB, Percona, TiDB, or other restrictively licensed
implementation sources.

## Compatibility Scope

The near-term baseline is the set of unsupported rows currently listed in
`COMPATIBILITY.md` under "Baseline Compatibility". A row may move out of
unsupported status only after its own feature implementation, MySQL 8.4.9
runtime comparison tests, metadata/warning/error coverage, and compatibility
documentation are complete.

This architecture plan must support the following baseline surfaces:

- runtime identity, selected database, diagnostics, warnings, and result
  metadata;
- base tables, temporary tables, and views;
- table DDL, table options, indexes, constraints, and `AUTO_INCREMENT`;
- MySQL column types, literals, assignment conversion, casts, and SQL modes;
- character sets and collations;
- DML, query expressions, joins, subqueries, CTEs, grouping, ordering, and
  aggregates;
- MySQL scalar, aggregate, window, JSON, temporal, numeric, string, system, and
  comparison functions;
- `INFORMATION_SCHEMA`, `SHOW`, utility statements, transactions, locking, and
  table maintenance placeholders or behavior.

## Architectural Decision

Use a descriptor-driven hybrid architecture:

1. Parse MySQL syntax into a MyLite AST.
2. Analyze the AST against MySQL metadata, SQL modes, type rules, collations,
   functions, diagnostics, and statement semantics.
3. Build a MyLite logical plan with logical MySQL descriptors and physical
   SQLite encodings.
4. Classify each operation by the safest SQLite execution strategy.
5. Lower safe operations to SQLite SQL, registered callbacks, native collations,
   virtual tables, or targeted fork hooks.
6. Keep unsupported or warning-sensitive behavior in explicit MyLite fallback
   code until it is proven safe to push down.
7. Return MySQL-compatible results, metadata, diagnostics, row counts, insert
   ids, and side effects from MyLite-owned state.

SQLite may execute a plan, but MyLite must decide what that plan means.

Consequences:

- MyLite runtime handles are the first compatibility boundary. Session state,
  statement state, diagnostics, warnings, selected schema, row counts, insert
  ids, and SQLite backend ownership belong to `mylite_db`, statement contexts,
  and related MyLite-owned runtime objects.
- MyLite metadata is authoritative. SQLite schema text and `PRAGMA` output are
  physical implementation details, not the source of MySQL-visible truth.
- MyLite should lower from analyzed descriptors and a typed plan, not from raw
  parser nodes or string substitutions.
- Metadata resolution is part of semantic analysis. It is not just a helper for
  final SQL string translation.
- Result metadata, diagnostics, warning state, affected rows, insert ids, and
  session-visible side effects belong to the MyLite statement lifecycle.
- SQLite should still own scans, joins, indexes, grouping, sorting, CTEs,
  windows, constraints, rollback, and durable b-tree changes whenever MySQL
  semantics can be preserved.

## Confirmed Baseline Decisions

The baseline implementation should follow these decisions:

1. Add minimal runtime handles before feature execution work. Public ABI can
   remain narrow, but internal `mylite_db` and statement context ownership must
   exist before DDL, DML, functions, diagnostics, and metadata grow.
2. Keep the MyLite lexer, parser, and analyzer authoritative. SQLite should see
   generated SQLite SQL or intentionally routed direct-parser subsets, not raw
   user MySQL SQL as the primary path.
3. Make MyLite catalog descriptors authoritative. SQLite schema objects are
   physical storage objects generated from MyLite descriptors.
4. Implement basic table lifecycle before generalized table rebuilds. Start
   with `CREATE TABLE`, `DROP TABLE`, `RENAME TABLE`, and `TRUNCATE TABLE`,
   then add complex `ALTER TABLE` paths once descriptor-aware copy and
   assignment conversion exist.
5. Implement assignment conversion at a narrow SQLite write boundary, calling
   MyLite-owned conversion code before records and index entries are assembled.
6. Add logical MySQL type descriptors on SQLite schema columns. Do not add
   custom SQLite storage classes. Descriptor-driven comparison hooks and broader
   comparison hooks remain later options if analyzer lowering, collations,
   read transforms, and helper keys do not cover the needed behavior cleanly.
7. Add a SQLite fork diagnostics bridge for native constraint failures,
   descriptor failures, callback warnings, and VDBE-owned failure points. The
   MyLite diagnostics area remains the public truth.
8. Let MyLite own statement lifecycle, with narrow SQLite lifecycle and
   completion hooks only where native execution has the first accurate signal.
9. Keep the `.mylite` version-1 preamble at 4096 bytes. Attempt shifted opening
   through a VFS offset shim first, and use a pager/file-layer patch only if the
   VFS path fails correctness tests. Plain SQLite tools must not open `.mylite`
   files as valid SQLite databases.
10. Make `AUTO_INCREMENT` MyLite-owned semantically. SQLite rowid or
    `AUTOINCREMENT` storage may be used only when it preserves MySQL-visible
    allocation, metadata, and insert-id behavior.
11. Implement `INFORMATION_SCHEMA` as read-only virtual tables backed by MyLite
    descriptors, with `xBestIndex` support for common filters.
12. Implement `SHOW`, `DESCRIBE`, diagnostics, and table-maintenance output as
    planned result builders over MyLite catalog and runtime state.
13. Treat embedded maintenance statements as real embedded operations where
    possible: resolve table names for `LOCK TABLES` and `UNLOCK TABLES`, run
    SQLite `ANALYZE` for `ANALYZE TABLE`, run physical and catalog checks for
    `CHECK TABLE`, and reuse descriptor-driven table rebuilds for
    `OPTIMIZE TABLE` and safe `REPAIR TABLE` cases.
14. Require per-family physical encoding specs before marking MySQL type support
    complete. Each type family must define storage, assignment conversion,
    expression conversion, comparison/order/group behavior, metadata, indexes or
    helper keys, diagnostics, warnings, and MySQL 8.4.9 runtime fixtures.

## Parser Strategy

MyLite should keep its independent MySQL lexer and Lemon parser as the primary
SQL frontend. Parser code must not depend on SQLite. The output is a MyLite AST
consumed by analyzer and runtime layers.

The parser should produce a compact typed AST with stable source spans. An
arena-backed representation is preferred once the AST grows large enough to
matter; lazy materialization is acceptable for bindings or debugging views, but
the analyzer should see typed MyLite nodes rather than generic parse-tree
arrays.

SQLite's parser is not a public extension point. Extending it means carrying a
SQLite source-tree fork, maintaining tokenizer and `parse.y` patches, and
rebasing those patches on upstream SQLite releases. That is acceptable only for
narrow syntax admission when the SQLite parser itself must see a construct
before code generation, not as the primary MySQL parser strategy.

Rejected baseline options:

- copying or adapting MySQL parser sources, because that conflicts with MyLite's
  licensing goals;
- making TiDB's grammar the authoritative parser, because it adds a large
  third-party grammar, is not the MySQL 8.4.9 runtime, and still needs a MyLite
  semantic AST;
- making the forked SQLite grammar the MySQL parser, because that pushes MySQL
  compatibility policy and AST ownership into SQLite internals.

Acceptable parser uses for the SQLite fork:

- admitting a small syntax form when MyLite intentionally routes that exact
  form through SQLite's native parser;
- separating user SQL from SQLite internal SQL before changing token meanings
  such as `||`;
- supporting a direct-parser fast path only after the MyLite AST/analyzer path
  has specified and tested the same behavior.

## Translation Safety Classes

Each expression, clause, and statement plan should carry a translation class:

- **Class 0, direct SQLite:** MySQL and SQLite semantics are proven equivalent
  for the resolved descriptors and SQL mode.
- **Class 1, decorated SQLite:** SQLite can execute the operation with explicit
  MyLite collation, cast, hidden projection, or syntax decoration.
- **Class 2, MyLite hook:** SQLite executes scans, joins, grouping, or ordering
  while calling a MyLite scalar, aggregate, window, comparison, cast, or key
  function.
- **Class 3, helper key:** A generated column, shadow column, expression index,
  sidecar table, or stable key function makes a hard MySQL semantic indexable.
- **Class 4, runtime fallback:** MyLite executes the operation because pushdown
  would risk different MySQL-visible results, warnings, side effects, or
  metadata.

The translator should prefer the lowest safe class. For performance, avoid
wrapping indexed columns in helper functions when constant pre-coercion plus a
native predicate or MySQL collation can preserve index use.

Expression comparison and assignment conversion are separate paths. Assignment
conversion belongs at the SQLite write boundary. Expression conversion belongs
to MyLite analysis and planning, using logical descriptors, collation metadata,
constant and parameter pre-coercion, specialized callbacks, helper keys, or
fallback execution. A broad SQLite comparison hook is not part of the initial
baseline, but descriptor-driven comparison hooks remain a valid later extension
point when narrower lowering produces wrapper complexity or loses important
planner behavior.

## Catalog And Information Schema

MyLite needs a durable MySQL catalog that is richer than SQLite's schema. The
catalog should preserve logical MySQL descriptors and the chosen physical SQLite
encoding for each object. DDL must update this catalog transactionally, and
physical SQLite schema changes must be generated from the resulting catalog
descriptor rather than from ad hoc SQL text.

Catalog state should include, at minimum:

- schemas and the selected database identity;
- persistent and temporary tables;
- columns, logical types, physical encodings, generated expressions, defaults,
  nullability, comments, character sets, collations, and visibility;
- primary keys, unique keys, secondary indexes, prefix lengths, expressions,
  index order, and index comments;
- foreign keys, checks, named constraints, table options, auto-increment state,
  view definitions, trigger placeholders, and routine or event placeholders;
- helper-key definitions, function versions, collation versions, and rebuild
  requirements.

Persistent catalog rows live in reserved internal tables inside the `.mylite`
file. Temporary object metadata should be connection-local or temporary-schema
backed so it follows MySQL temporary table lifetime and name precedence rules.
User SQL must not be able to access or mutate internal `_mylite_*` objects
unless a surface is intentionally exposed.

`INFORMATION_SCHEMA` should be implemented as read-only virtual tables backed
by the catalog, with `xBestIndex` support for common predicates such as schema
name, table name, column name, and constraint name. Computed values such as
current auto-increment values may combine catalog descriptors with SQLite
runtime state such as `sqlite_sequence`, but the visible schema definition must
come from MyLite descriptors.

The baseline implementation should not reconstruct MySQL metadata from SQLite
introspection. Reverse reconstruction is a later migration, recovery, legacy
import, or repair-tool concern. Even there, reconstructed metadata must be
marked as inferred until it has been validated or replaced by authoritative
MyLite descriptors.

Connections should cache compact table and column descriptors and invalidate
them using a MyLite catalog generation, SQLite schema generation, and session
state dependencies. Prepared SQLite statements must hold the descriptor version
they were prepared against.

## Statement Execution And Result Metadata

Every top-level MyLite statement needs a statement context before any SQLite
statement is prepared or stepped. That context owns:

- warning reset and warning collection;
- statement-stable time values;
- affected-row accounting and previous-row-count state;
- first generated insert id and `LAST_INSERT_ID()` state;
- per-statement function state such as seeded random streams;
- last executed SQL and trace/profiling information;
- MySQL condition mapping for parser, analyzer, callback, and SQLite failures.

Statements should run inside a MyLite-controlled statement boundary whenever
MyLite needs atomicity beyond one SQLite statement. This includes MySQL
statements that lower to multiple SQLite statements, statements that must update
catalog and physical schema together, and statements that need precise
statement-level rollback. If the user is not already in a transaction, MyLite
should open a wrapper transaction. If the statement may write, the SQLite
backend should use `BEGIN IMMEDIATE` for the wrapper so SQLite does not discover
a lock upgrade failure midway through the operation. Inside a user transaction,
MyLite should use savepoints when it needs statement-level rollback without
ending the user's transaction.

Planned result builders are the correct surface for statements that do not map
to a physical SQLite result set, including selected `SHOW`, `DESCRIBE`, table
maintenance, diagnostics, utility, and placeholder statements. Result column
labels, origin names, logical types, flags, lengths, charset, decimals, and
nullability should come from the analyzer output and catalog descriptors, not
from SQLite's default result metadata after rewriting. When MyLite rewrites a
SELECT expression, it must preserve the MySQL-visible label that the original
query would have exposed unless the user supplied an explicit alias.

## Public SQLite Extension Surface

Use SQLite public APIs for primitives that can be registered or observed without
changing SQLite parse trees, schema objects, record assembly, pager layout, or
statement completion timing.

Use public APIs for:

- scalar, aggregate, and window functions;
- MySQL collation names via `sqlite3_create_collation_v2()`;
- virtual tables for `information_schema`, `performance_schema`, `sys`, `mysql`
  metadata surfaces, diagnostics views, and table-valued functions;
- connection-local client data consumed by registered callbacks;
- authorizer, trace, progress, busy, update, pre-update, commit, and rollback
  hooks for policy, invalidation, profiling, and observation;
- `sqlite3_db_config()` connection policy, including trusted-schema and foreign
  key settings;
- VFS shims when file behavior can be implemented without changing pager
  assumptions;
- SQLite execution primitives such as ordinary b-trees, transactions,
  savepoints, native indexes, expression indexes, CTEs, joins, grouping,
  sorting, windows, and native constraints when MySQL-visible behavior can be
  preserved.

Registered functions and collations should be native MyLite callbacks. Each
function must declare accurate SQLite flags such as deterministic, innocuous,
direct-only, and subtype behavior. Functions whose behavior depends on SQL
mode, time zone, character set, warning rules, user variables, or statement
state must read that state from the MyLite connection or statement context.

Public APIs are not enough for:

- MySQL syntax SQLite rejects before callbacks are consulted;
- assignment conversion that must happen before records are assembled;
- read-time display or numeric-context transforms for columns whose physical
  storage differs from MySQL-visible values;
- structured MySQL diagnostics emitted from SQLite-owned constraint or VDBE
  failure points;
- statement-start and statement-halt state such as warning reset, row count,
  first generated insert id, and statement-stable time;
- shifted SQLite page offsets if a VFS shim cannot satisfy all pager, WAL,
  journal, backup, vacuum, and mmap paths.

## Targeted SQLite Fork Surface

The fork should expose small extension points to MyLite, not contain broad MySQL
compatibility code.

Required fork points:

- **Source-tree package:** build a pinned SQLite source tree from reproducible
  inputs, with patches guarded by a compile-time MyLite option.
- **Logical type descriptors:** let MyLite register compact logical MySQL type
  descriptors against live SQLite schema columns. The descriptors define
  assignment conversion, read transforms, numeric context, metadata, and future
  comparison/helper-key policy while SQLite storage remains ordinary SQLite
  storage classes.
- **Column descriptors:** allow MyLite to attach compact logical/physical
  descriptors to live SQLite table columns before statement preparation.
- **Write boundary:** emit a small MyLite assignment-check opcode at the same
  boundary where SQLite applies table affinity and builds records.
- **Read boundary:** emit read-transform hooks only for types whose physical
  storage and visible MySQL value differ, such as future `ENUM`, `SET`, and
  `BIT` encodings.
- **Diagnostics bridge:** let VDBE hooks, native constraints, and registered
  callbacks publish MySQL condition level, error number, and SQLSTATE into
  connection-owned diagnostics state.
- **Statement lifecycle:** expose top-level statement start and halt points for
  warning reset, affected-row state, generated-id state, and statement time.
- **Constraint context:** expose enough native constraint context to map common
  failures to MySQL conditions without replacing SQLite's constraint engine.
- **File-offset support:** add pager support only if the `.mylite` shifted
  payload cannot be implemented correctly as a VFS shim.
- **Optional descriptor comparisons:** add descriptor-aware comparison support
  only after logical descriptors and analyzer lowering prove a narrower hook is
  needed for correctness or planner preservation.

Avoid in the fork:

- a MySQL parser as the primary frontend;
- large MySQL type-conversion implementations embedded in `vdbe.c`;
- custom SQLite storage classes for MySQL types;
- user-table virtual tables as the default storage engine;
- broad rewrites of SQLite planner, b-tree, transaction, or VDBE architecture.

## Table DDL And ALTER TABLE

MyLite should treat DDL as descriptor transformation first and physical SQLite
schema mutation second. The analyzer should build the target MySQL table
descriptor, validate it against MySQL rules, then choose the cheapest safe
physical operation. Within the statement transaction, MyLite should stage or
record the catalog mutation first, generate physical SQLite DDL from the target
descriptor, and commit or roll back the catalog and physical schema as one unit.

The DDL planner should use three paths:

- **Native SQLite path:** use SQLite's supported `ALTER TABLE` operations when
  the operation is semantically equivalent, does not lose MySQL metadata, and
  can be committed atomically with the MyLite catalog update.
- **Generalized rebuild path:** create a new physical table with the target
  descriptor generated from the catalog, copy data through descriptor-aware
  assignment conversion, rebuild indexes and dependent objects, validate
  foreign keys and constraints, replace the old table, and commit catalog plus
  physical changes together.
- **Metadata-only path:** update MyLite descriptors only when the change has no
  physical SQLite effect and every prepared statement/cache dependency can be
  invalidated safely.

The generalized rebuild path should follow SQLite's documented safe order for
arbitrary table changes: create the replacement table, copy data, drop the old
table, then rename the replacement into place. MyLite should not rename the old
table first because that can disturb dependent references before the replacement
is ready.

The rebuild planner must account for:

- hidden physical columns, helper keys, generated columns, and shadow objects;
- MySQL column order and metadata even when SQLite storage order differs;
- auto-increment sequence transfer or reset semantics;
- index, constraint, view, trigger, and foreign-key descriptor recreation;
- rollback safety if copy, validation, catalog update, or final rename fails;
- warning and error behavior for lossy conversions during `ALTER TABLE`.

Direct `sqlite_schema` text editing through writable-schema mode should not be
part of the baseline runtime path. It may be considered only for offline repair
tools after separate design and corruption-safety tests.

## Maintenance And Embedded Server Surfaces

Server-oriented MySQL surfaces should be mapped to embedded behavior explicitly
instead of being silently ignored.

`LOCK TABLES` and `UNLOCK TABLES` should parse and resolve table names so MyLite
can return MySQL-compatible object diagnostics. The baseline can record
connection-local lock intent and should verify MySQL implicit-commit and
autocommit interactions before starting or ending SQLite transactions for this
surface.

`ANALYZE TABLE` should run SQLite `ANALYZE` for the physical table and indexes
when possible, then return a MySQL-shaped planned result set.

`CHECK TABLE` should combine SQLite physical integrity checks, preferably
scoped to the target table where the pinned SQLite version supports that, with
MyLite catalog and descriptor consistency checks.

`OPTIMIZE TABLE` should use the same descriptor-driven table rebuild machinery
required for complex `ALTER TABLE`, followed by analysis where appropriate.
`REPAIR TABLE` may use that rebuild machinery only when source rows and catalog
metadata can be read safely; otherwise it must return a clear diagnostic rather
than claiming repair success.

## Assessment Of The `sqlite-fork` Branch

The `sqlite-fork` branch is useful design evidence. It demonstrates that a
source-tree SQLite fork can provide real compatibility leverage, especially for
column descriptors, write-time type checks, read transforms, diagnostics,
statement lifecycle state, collations, functions, and direct CRUD experiments.

The strongest ideas to reuse:

- source-tree SQLite package and reproducible generated SQLite files;
- connection bootstrap for functions, collations, trusted-schema policy, and
  foreign-key policy;
- column descriptor attachment before statement preparation;
- VDBE write-time assignment boundary rather than per-statement SQL wrappers;
- assignment-aware `UPDATE` checks that use SQLite's changed-column mask;
- structured diagnostics bridge from fork failures into MyLite diagnostics;
- statement-start and statement-halt hooks for warning lifecycle, row count,
  insert id, and statement time;
- MySQL-runtime-backed fixtures for application-like CRUD, type conversion,
  prefix collations, and constraint diagnostics.

The parts to improve before adopting:

- move MySQL conversion implementations out of SQLite internals and into
  MyLite-owned callback modules;
- keep direct SQLite parser admission behind the MyLite parser/analyzer path,
  not ahead of it;
- reduce parser fork patches to small admission hooks;
- keep MyLite metadata authoritative instead of relying on SQLite schema text
  to preserve MySQL descriptors;
- define patch categories and rebase tests before carrying a large fork.

## Baseline Implementation Phases

### Phase 0: Runtime Handles And Statement Context

- Add internal `mylite_db` and statement-context objects before feature
  execution work.
- Own SQLite connection lifetime, selected schema, SQL modes, time zone,
  character-set state, system variables, user variables, diagnostics, warnings,
  row counts, insert ids, and statement-stable time in MyLite runtime state.
- Keep public ABI narrow until the internal handle contracts are stable, but
  avoid feature-local state plumbing that bypasses the runtime handles.
- Add statement boundary helpers for warning reset, diagnostics reset,
  statement time, affected rows, insert ids, wrapper transactions, savepoints,
  and result builders.

### Phase 1: SQLite Package, Patch Discipline, And File Opening

- Add a `libmylite_sqlite` or `libmylite_fork` package built from the pinned
  SQLite source tree when a source-tree fork is needed.
- Generate SQLite derived files in the build tree.
- Keep upstream files mechanically refreshable.
- Add an explicit local patch ledger and rebase tests before semantic fork
  hooks grow.
- Open ordinary SQLite databases through the MyLite runtime first.
- Keep the `.mylite` version-1 preamble at 4096 bytes and validate it before
  opening the SQLite payload.
- Implement `.mylite` preamble-aware opening with a VFS offset shim before
  pager changes.
- Add tests for rollback journal, WAL, backup, vacuum, `VACUUM INTO`, truncate,
  file size, mmap behavior if enabled, preamble corruption, payload corruption,
  and accidental plain-SQLite-tool opening.
- Add a pager/file-layer offset patch only if the VFS shim cannot satisfy these
  correctness requirements cleanly.

### Phase 2: Connection Bootstrap And Public SQLite APIs

- Register MySQL function callbacks by public SQLite APIs.
- Register baseline MySQL collation names.
- Disable trusted schema for untrusted schema contexts.
- Mark functions accurately as deterministic, direct-only, innocuous, and
  subtype-aware.
- Attach MyLite connection-local client data consumed by registered callbacks.
- Reserve `_mylite_*` helper names and block user-authored references unless
  intentionally exposed.
- Install hooks needed for cache invalidation, profiling, and catalog safety.
- Enable SQLite connection policy that MyLite intentionally relies on, including
  trusted-schema policy and foreign-key enforcement where applicable.

### Phase 3: Parser, Analyzer, And Expression Descriptors

- Grow the MyLite Lemon grammar feature by feature from the compatibility
  baseline.
- Store parsed statements in a typed AST with source spans and stable node
  ownership.
- Keep parsed-but-unimplemented features returning explicit unsupported,
  placeholder, warning, or diagnostic behavior.
- Add an analyzer that resolves names, schemas, scopes, types, collations,
  SQL modes, defaults, functions, metadata, warnings, and errors.
- Preserve source spans for diagnostics and metadata.
- Do not lower raw parser nodes directly to SQLite.
- Add expression descriptors that carry MySQL logical type, nullability,
  length, decimals, charset, collation, coercibility, origin metadata, and
  warning dependencies.
- Classify expression comparison and conversion through the translation safety
  classes. Use direct SQLite only when proven equivalent, then decorated
  SQLite, callbacks, helper keys, or fallback as needed.

### Phase 4: Logical Metadata, Type Encodings, And Physical Storage

- Define durable MyLite catalog tables for schemas, tables, columns, indexes,
  constraints, views, routines/placeholders, collations, and options.
- Store MySQL logical descriptors separately from SQLite physical encoding.
- Expose `INFORMATION_SCHEMA` as read-only virtual tables over MyLite
  descriptors.
- Add catalog versioning, descriptor caching, and invalidation.
- Defer reconstruction from SQLite schema to a later repair/import tool; do not
  include it in the baseline runtime path.
- Choose physical encodings per type family in feature specs.
- For each type family, specify assignment conversion, expression conversion,
  readback, numeric context, comparison/order/group behavior, result metadata,
  diagnostics, warning behavior, and helper-key policy before marking support.
- Keep result metadata, `SHOW`, and information-schema output driven by MyLite
  descriptors, not by SQLite's metadata alone.
- Use ordinary SQLite b-trees for user tables.

### Phase 5: Basic Table Lifecycle

- Implement `CREATE TABLE`, `DROP TABLE`, `TRUNCATE TABLE`, and `RENAME TABLE`
  by MyLite AST/analyzer/planner before generalized `ALTER TABLE`.
- Lower physical user tables and indexes to SQLite.
- Update MyLite catalog rows first, generate physical SQLite schema from the
  target descriptors, and commit both in the same statement boundary.
- Use SQLite native constraints when their enforcement timing is compatible and
  diagnostics can be mapped.
- Implement `AUTO_INCREMENT` as MyLite-owned semantic state, using SQLite rowid
  or `AUTOINCREMENT` only when the physical path preserves MySQL allocation,
  explicit-value, zero-handling, metadata, and insert-id behavior.
- Use SQLite expression indexes or generated helper indexes only after
  canonical helper SQL and versioning exist.

### Phase 6: Logical Type Descriptors And Write Boundary

- Attach MyLite logical type descriptors to SQLite schema columns before
  preparing reads and writes.
- Emit a small fork opcode that calls MyLite-owned assignment conversion.
- Preserve ordinary SQLite storage classes; do not add custom SQLite storage
  classes for MySQL types.
- Cover strict and non-strict SQL modes, warnings, `IGNORE`, truncation, range
  checks, temporal parsing, JSON validation, character and byte length, and
  nullability in MyLite code.
- Add read-time transforms for types whose physical storage and MySQL-visible
  display or numeric context differ.
- Let SQLite perform physical row writes, index updates, constraints,
  statement rollback, and transaction integration.
- Add MySQL-runtime fixtures per type family before marking support.

### Phase 7: Diagnostics And Statement Lifecycle Hooks

- Add the SQLite fork diagnostics bridge for descriptor failures, native
  constraints, registered callback warnings, and VDBE-owned failure points.
- Keep final MySQL diagnostics area ownership in MyLite statement context.
- Add narrow SQLite lifecycle/completion hooks for native row counts, generated
  ids, statement time, statement-start reset, and completion state only where
  MyLite cannot observe the accurate signal through wrapper execution.
- Map native `NOT NULL`, unique, primary-key, check, and immediate foreign-key
  failures to MySQL condition numbers and SQLSTATE values before relying on
  native constraints for supported behavior.

### Phase 8: DML Baseline

- Lower `INSERT`, `INSERT ... SET`, `INSERT ... SELECT`, `INSERT IGNORE`,
  `INSERT ... ON DUPLICATE KEY UPDATE`, `REPLACE`, `UPDATE`, and `DELETE` to
  SQLite primitives where safe.
- Open write-intent wrapper transactions for standalone writes and savepoints
  inside user transactions when statement-level rollback is needed.
- Use wrapper transactions whenever one MySQL statement lowers to multiple
  SQLite statements that must commit or roll back as a unit.
- Keep `AUTO_INCREMENT`, duplicate-key affected-row rules, warning demotion,
  and `LAST_INSERT_ID()` in MyLite-owned state with fork lifecycle hooks where
  SQLite has the first accurate signal.
- Prefer native SQLite uniqueness and foreign-key enforcement once MySQL
  diagnostics and timing are covered.

### Phase 9: Query Baseline

- Build SELECT plans with descriptors for outputs, predicates, ordering,
  grouping, aggregates, windows, CTEs, and subqueries.
- Push down direct SQLite work for safe scans, joins, predicates, grouping,
  sorting, limits, CTEs, and windows.
- Use native collations before comparison helper functions.
- Use hidden projections or derived query shapes for MySQL-visible `ORDER BY`,
  `DISTINCT`, and metadata behavior.
- Keep runtime fallback for user variables, warning-sensitive evaluation order,
  unsafe SQL modes, and unverified comparisons.
- Add descriptor-driven comparison hooks only if analyzer lowering, native
  collations, specialized callbacks, read transforms, and helper keys produce
  persistent wrapper complexity or lose important planner behavior.

### Phase 10: System Schemas, Introspection, And Maintenance

- Expose `INFORMATION_SCHEMA`, `SHOW`, and selected `mysql.*` surfaces through
  virtual tables or planned result builders backed by MyLite metadata.
- Synthesize `SHOW`, `DESCRIBE`, and selected table-maintenance result sets
  from catalog and runtime state.
- Use virtual-table `xBestIndex` for common filters such as schema and table
  names.
- Keep ordinary user storage out of virtual tables unless profiling proves a
  specific need.
- Implement `ANALYZE TABLE` using SQLite `ANALYZE` with a MySQL-shaped result.
- Implement `CHECK TABLE` through physical SQLite checks plus MyLite catalog and
  descriptor consistency checks.
- Implement `LOCK TABLES` and `UNLOCK TABLES` by resolving objects and recording
  connection-local lock intent until MySQL-verified transaction behavior is
  specified.

### Phase 11: Complex ALTER, OPTIMIZE, REPAIR, And Helper Keys

- Add generalized descriptor-driven table rebuilds for complex `ALTER TABLE`
  after assignment conversion, diagnostics, indexes, constraints, and copy paths
  are stable.
- Reuse the rebuild machinery for `OPTIMIZE TABLE`, and for `REPAIR TABLE` only
  when source rows and catalog metadata can be read safely.
- Add helper-key infrastructure only after correctness paths exist.
- Use generated columns, shadow columns, expression indexes, or sidecar tables
  for high-value indexed semantics such as collation weights, decimal ordering,
  temporal normalization, JSON path indexes, and prefix indexes.
- Store helper function versions and add rebuild/reindex handling.
- Add plan tests that assert index use where performance matters.

## Testing Requirements

Every supported feature needs MySQL 8.4.9 runtime comparison tests. Expected
results must cover:

- result rows and column values;
- field names, origin metadata, types, flags, lengths, charset, and decimals;
- SQLSTATEs, error numbers, warning numbers, warning count, and warning order;
- affected rows, previous row count, insert ids, and statement side effects;
- `SHOW` and information-schema visibility;
- transaction and rollback behavior;
- SQL mode and session-state interactions.

Architecture-level tests should cover:

- SQLite source-tree pin and patch guard;
- descriptor attachment before preparation;
- descriptor changes not mutating already-prepared statements unexpectedly;
- direct SQLite, decorated SQLite, hook, helper-key, and fallback plan
  classification;
- trusted-schema security for registered functions;
- VFS or pager behavior for shifted `.mylite` payloads;
- rebase checks that local SQLite patch hunks remain small and documented.

No MySQL runtime fixture is attached to this architecture document because it
does not define one user-visible SQL feature. Feature specs derived from this
plan must include MySQL-runtime-verified expectations before implementation.

## Performance Policy

Correctness comes first, but the baseline must be shaped for SQLite-native
performance:

- use ordinary SQLite b-trees for user data;
- preserve native indexes by pre-coercing constants and parameters;
- emit matching SQLite collation names in table/index definitions and queries;
- avoid generic comparison functions on indexed columns unless no native or
  decorated path is correct;
- specialize helper functions by type/domain instead of routing all comparisons
  through one generic dispatcher;
- add helper keys for hot hard semantics rather than accepting permanent
  full-scan fallback;
- profile representative application queries before adding invasive SQLite
  patches.

## Known Risks And Decisions

- Full Unicode MySQL collation fidelity may require either a compact
  independently authored implementation or a carefully reviewed dependency.
- `BIGINT UNSIGNED`, exact `DECIMAL`, `ENUM`, `SET`, temporal zero values, and
  JSON have physical-encoding decisions that must be specified separately.
- SQLite optimizer behavior can change warning timing. Warning-producing
  expressions need capability checks and fallback.
- Prepared SQLite statements depend on session state such as SQL mode, time
  zone, character set, collation, and function settings. MyLite needs
  invalidation or parameterization rules.
- Internal helper functions in schema contexts are a security risk. Trusted
  schema must be disabled unless a helper is intentionally safe.
- Descriptor-driven comparisons may eventually need SQLite fork support if
  analyzer lowering and callbacks cause unacceptable wrapper complexity or lose
  important index, sort, grouping, or join behavior. A broad comparison hook
  should remain a last resort because it touches planner-sensitive semantics.
- VFS offset handling for `.mylite` files may not cover every SQLite pager,
  journal, WAL, backup, vacuum, and mmap path cleanly. If tests expose an
  unsafe edge, a narrow pager/file-layer offset patch is acceptable.
- `LOCK TABLES` may need transaction or autocommit side effects. The baseline
  should not guess those effects without MySQL 8.4.9 runtime verification.
- Fork patches must remain small enough to rebase onto a fresh SQLite snapshot.
  A broad parser or VDBE rewrite is a maintenance failure.

## Implementation Handoff

The first implementation batch should be:

1. internal `mylite_db` and statement-context runtime handles with diagnostics,
   warning reset, selected schema, row-count, insert-id, statement-time, and
   SQLite connection ownership;
2. connection bootstrap for public function/collation registration,
   trusted-schema policy, and SQLite connection policy;
3. source-tree SQLite package or patch-stack machinery with zero semantic
   patches, plus rebase checks before the first hook;
4. `.mylite` VFS opening experiment for the 4096-byte preamble, with journal,
   WAL, backup, vacuum, truncate, mmap, and corruption-boundary tests;
5. catalog-backed descriptor API prototype with one integer assignment
   descriptor attached to SQLite schema columns before statement preparation;
6. VDBE write-boundary hook that calls back into MyLite-owned conversion code;
7. diagnostics bridge for that one descriptor failure and one native constraint
   failure;
8. one MySQL-runtime-verified DDL/DML fixture that proves the end-to-end path
   through parser, analyzer, catalog, descriptor attachment, SQLite execution,
   diagnostics, result metadata, row counts, and insert ids;
9. documentation of every fork hunk and a rebase check in tests.

This keeps the hook-in surface small while proving the central architecture:
MyLite decides MySQL semantics, SQLite executes the durable relational work,
and the fork only exposes missing extension points.
