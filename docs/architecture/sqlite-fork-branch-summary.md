# SQLite Fork Branch Summary

This document summarizes the `sqlite-fork` branch as of May 7, 2026. The branch
adds a source-tree SQLite fork, wires MyLite to it, and establishes the boundary
between SQLite's public extension surface and the fork points MyLite needs for
transparent MySQL compatibility.

## Branch Scope

The branch adds 58 commits on top of `main`. The large diff is dominated by the
vendored SQLite source-tree import in `packages/libmylite_fork`. After that
import, the actual SQLite upstream patch surface is targeted: 18 upstream files
changed, with roughly 3,870 inserted lines and 24 deleted lines.

The work includes:

- `packages/libmylite_fork`, a private SQLite source-tree fork with pinned
  SQLite inputs, generated parser/opcode build steps, fork tests, and public
  MyLite fork APIs.
- MyLite runtime wiring so `libmylite` uses `MyLite::mylite_fork`.
- A large direct fork test suite in
  `packages/libmylite/tests/sqlite_fork_test.c`, including WordPress-like CRUD
  fixtures and MySQL-runtime-backed type/coercion fixtures.
- Specs for the fork package, CRUD foundation, type coercion, diagnostics,
  column descriptors, read descriptor hydration, and extension point map.

## Implemented Foundation

The branch moves core MySQL compatibility primitives closer to SQLite instead
of duplicating them in a parallel execution layer.

Implemented fork/runtime primitives include:

- MySQL function registration for `CONCAT`, `CONCAT_WS`, `IF`, `BIT_LENGTH`,
  `BIT_COUNT`, `DATABASE`, `SCHEMA`, `LAST_INSERT_ID`, `ROW_COUNT`, current
  temporal functions, `ISNULL`, `STRCMP`, and length functions.
- MySQL collation registration on SQLite connections, with the current
  ASCII/PAD SPACE-focused collation behavior and prefix-unique collation
  propagation.
- Column descriptors attached to SQLite schema columns for integer, string,
  binary, text/blob, decimal, date/datetime, timestamp, time, year, enum, set,
  bit, and JSON families.
- VDBE write-time type checking through `OP_MyliteTypeCheck`.
- VDBE read-time display/numeric-context transforms for `ENUM`, `SET`, and
  `BIT`.
- Fork diagnostics bridge for MySQL conditions from descriptor failures,
  scalar warnings, and native SQLite constraints.
- Native constraint mapping for `NOT NULL`, `UNIQUE`, `PRIMARY KEY`, `CHECK`,
  and immediate foreign-key failures.
- Statement/session completion state for `ROW_COUNT()` and `LAST_INSERT_ID()`.
- Statement-stable time access for `NOW()` and current temporal functions.

The direct SQLite parser now accepts a useful MySQL CRUD subset:

- `TRUNCATE [TABLE]`
- `AUTO_INCREMENT`
- MySQL table options such as `ENGINE`, `DEFAULT CHARSET`, `DEFAULT CHARACTER
  SET`, `COLLATE`, `DEFAULT COLLATE`, table `COMMENT`, and
  `AUTO_INCREMENT=N`
- table-level `KEY`, `INDEX`, `UNIQUE KEY`, and `UNIQUE INDEX`
- prefix key parts such as `slug(191)`, lowered to SQLite expression indexes
- column-level `CHARACTER SET`, `CHARSET`, and `COMMENT`
- `INSERT IGNORE`
- `INSERT ... SET`
- `ON DUPLICATE KEY UPDATE`, including `VALUES(column)`
- `UPDATE IGNORE`
- `DROP TEMPORARY TABLE`, routed to SQLite's `temp` schema for unqualified
  names

## Why The Fork Was Needed

SQLite's public extension surface is useful, but it is too late for several
MySQL compatibility requirements. MyLite needs source-level fork points when
SQLite must understand MySQL semantics while it is tokenizing SQL, building
schema objects, generating bytecode, constructing records, reporting
diagnostics, or completing a statement.

The fork is currently needed for:

- tokenizer and Lemon grammar admission for MySQL syntax SQLite rejects before
  callbacks can run
- schema-builder metadata such as MySQL column descriptors, table options,
  inline indexes, prefix indexes, and auto-increment promotion
- VDBE assignment conversion before records are assembled
- read-time value transformation where physical storage and MySQL-visible
  values differ
- constraint and type-conversion diagnostics with MySQL condition metadata
- statement-completion state for affected rows, generated ids, result-set
  completion, and truncate row counts
- statement-time state for MySQL current temporal functions
- future pager/file-format work if the `.mylite` file reserves bytes before
  SQLite page 1
- future full auto-increment behavior, including gaps, zero handling, failed
  statement side effects, and unsigned `BIGINT` limits

The branch does not rewrite SQLite's storage engine, optimizer, joins, CTEs,
transactions, savepoints, grouping, sorting, window execution, or b-tree layer.
Those remain SQLite responsibilities.

## What Public SQLite Extensions Can Cover

The public SQLite extension surface can and should cover primitives that do not
need to alter parse trees, schema objects, bytecode generation, storage
decisions, or statement-completion timing.

Use public SQLite APIs for:

- scalar, aggregate, and window functions when SQLite already parses the
  function call
- collations via `sqlite3_create_collation_v2`
- JSON parsing as a validation primitive where SQLite's parser is sufficient
- virtual tables for future synthetic schemas such as `information_schema`,
  `performance_schema`, `sys`, and diagnostic surfaces
- connection client data for compact session state consumed by callbacks
- `sqlite3_db_config()` and related connection policy, such as enabling native
  foreign keys
- VFS hooks for many file-opening policies when the SQLite database image still
  starts where SQLite expects it
- authorizer, trace, progress, busy, update/preupdate, commit, and rollback
  hooks for observation and policy
- native SQLite execution features such as constraints, transactions, joins,
  CTEs, window functions, UPSERT execution, temp schema behavior, expression
  indexes, and rollback

## Current Architectural Call

The right direction is a hybrid:

1. Keep SQLite's native execution machinery responsible for storage, indexing,
   transactions, joins, CTEs, windows, sorting, grouping, and rollback.
2. Use public SQLite APIs for functions, collations, virtual schemas,
   instrumentation, and connection policy whenever the semantics fit.
3. Patch the fork only where MySQL compatibility must be visible before or
   during SQLite parse, schema construction, bytecode generation, record
   assembly, diagnostics, or statement completion.
4. Keep adding MySQL syntax by lowering it into SQLite-native primitives rather
   than building a second SQL execution engine beside SQLite.

In short: the branch confirms that MyLite does need a SQLite fork for a
transparent MySQL foundation, but the fork should stay narrow. Most heavy
execution work should remain SQLite's job, with MyLite adding MySQL-aware
syntax, descriptors, conversion, diagnostics, and session semantics at the
smallest internal points that make those behaviors native.
