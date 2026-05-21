# Baseline Non-Strict Duplicate-Key Coercion

## Summary

This phase tightens the existing descriptor-driven
`INSERT ... ON DUPLICATE KEY UPDATE` implementation where it intersects with
non-strict SQL-mode conversion. It does not add new grammar. It changes only
duplicate-branch assignment conversion for statement shapes that MyLite already
parses and executes.

The main compatibility goal is to make common upsert writes behave like MySQL
8.4.9 after applications disable strict mode with statements such as
`SET sql_mode = ''` or `SET sql_mode = 'NO_ENGINE_SUBSTITUTION'`.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`
  - `docs/specs/baseline-insert-select-on-duplicate-key-update/specs.md`
  - `docs/specs/baseline-nonstrict-dml-coercion/specs.md`
  - `docs/specs/baseline-sql-mode-session-state/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT ... ON DUPLICATE KEY UPDATE`:
    <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - Server SQL modes: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_nonstrict_duplicate_key_coercion_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for this slice:

- Default strict mode keeps duplicate-branch conversion strict:
  overlength `VARCHAR`, `NULL` into `NOT NULL`, and `DEFAULT` for a no-default
  `NOT NULL` target remain errors.
- In non-strict mode, duplicate-branch string assignment to a supported
  `VARCHAR` target truncates to the descriptor length, updates the row, and
  records warning `1265`.
- In non-strict mode, duplicate-branch `DEFAULT` for a no-default `NOT NULL`
  integer target stores the implicit descriptor value `0` and records warning
  `1364` once for each duplicate row that executes that assignment.
- In non-strict mode, a single-row duplicate-branch `NULL` assignment to a
  `NOT NULL` target still fails with `1048 / 23000`.
- In non-strict mode, a multi-row `INSERT ... VALUES ... ON DUPLICATE KEY
  UPDATE` statement whose duplicate branch assigns literal `NULL` to a
  `NOT NULL` target stores the implicit descriptor value for the duplicate row
  and records warning `1048`.
- The affected-row count remains MySQL's existing duplicate-key update count:
  inserted rows count `1`, changed duplicate rows count `2`, and unchanged
  duplicate rows count `0` after conversion.
- `VALUES(column_name)` deprecation warning `1287` remains independent of this
  conversion slice.

## Scope

This feature supports only duplicate-key update statement shapes that MyLite
already supports:

- descriptor-backed `INSERT ... VALUES ... ON DUPLICATE KEY UPDATE`;
- descriptor-backed `INSERT ... SET ... ON DUPLICATE KEY UPDATE`;
- descriptor-backed `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE`, only where
  the existing insert-select duplicate-key feature already admits the shape.

For literal duplicate assignment values in those shapes:

- non-strict `VARCHAR`, `CHAR`, and baseline text-family assignment literals use
  the same descriptor-owned truncation policy as supported `UPDATE`
  assignments;
- non-strict `DEFAULT` for a no-default target continues to materialize the
  descriptor implicit value and warning through the existing DML default path;
- non-strict literal `NULL` into `NOT NULL` targets is adjusted only for
  multi-row `VALUES` duplicate-key statements, matching the verified MySQL
  8.4.9 behavior;
- strict mode behavior remains unchanged.

## Non-Goals

This feature does not add:

- new SQL grammar;
- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE`;
- arbitrary duplicate assignment expressions;
- column-to-column duplicate assignments;
- broader implicit type coercion;
- binary string, `BIT`, temporal, decimal, approximate, `ENUM`, `SET`, JSON, or
  spatial non-strict conversion changes beyond the current existing paths;
- non-strict inserted-row explicit `NULL` conversion for ordinary multi-row
  `INSERT` / `REPLACE`;
- non-strict inserted-row explicit `NULL` conversion for `VALUES(column_name)`
  duplicate assignments;
- warning order changes for existing `VALUES(column_name)` deprecation
  diagnostics;
- optimizer behavior, protocol info strings, triggers, cascades, privilege
  semantics, or SQLite fork patches.

The inserted-row explicit `NULL` cases are intentionally deferred because they
belong to a broader multi-row insert conversion slice. This phase changes only
duplicate-branch literal assignment conversion.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` keeps the existing result-handle
  ownership and public misuse behavior.
- Statement context and diagnostics: own affected rows, warning counts, and
  warning records produced by adjusted duplicate assignments.
- Session state: owns the handle-local SQL-mode bits that decide strict versus
  non-strict conversion.
- Parser/AST: unchanged. Existing duplicate-key assignment nodes are reused.
- Analyzer/planner: continues to resolve target tables, duplicate assignment
  targets, and descriptor metadata before execution.
- Runtime conversion: owns duplicate-branch value conversion from admitted AST
  values into descriptor-owned physical values.
- Catalog: remains authoritative for target column type, nullability, default
  metadata, key descriptors, and stable physical table names. This feature does
  not mutate catalog rows, descriptor versions, catalog generation, descriptor
  caches, or `sqlite_schema_generation`.
- SQLite physical storage: receives descriptor-built, quoted, prepared SQL with
  bound values. SQLite does not decide MySQL strictness or warnings.
- Storage/VFS/file format: unchanged. Adjusted duplicate updates write only to
  the shifted SQLite payload and must not touch the `.mylite` preamble.

## Grammar

No grammar changes are required. The relevant existing Lemon-syntax sketch is:

```lemon
insert_statement ::= INSERT insert_modifier_opt INTO table_name insert_values
    on_duplicate_key_update_opt.

on_duplicate_key_update_opt ::= .
on_duplicate_key_update_opt ::= ON DUPLICATE KEY UPDATE duplicate_assignment_list.

duplicate_assignment ::= identifier EQUAL duplicate_update_value.

duplicate_update_value ::= insert_value.
duplicate_update_value ::= DEFAULT.
duplicate_update_value ::= VALUES LP identifier RP.
```

Runtime conversion remains narrower than the grammar and rejects unsupported
assignment values deterministically.

## Semantics

Strictness is evaluated from the connection's current session SQL mode at
execution time:

- strict if `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES` is active;
- non-strict otherwise.

For duplicate-branch literal assignments:

- strict mode preserves current errors for overlength strings, `NULL` into
  `NOT NULL`, and no-default `DEFAULT`;
- non-strict string assignment into supported character/text descriptors
  records the existing truncation warning and stores the truncated value;
- non-strict no-default `DEFAULT` records warning `1364` and stores the
  descriptor implicit value or SQL `NULL` for nullable dropped-default columns;
- non-strict literal `NULL` into `NOT NULL` records warning `1048` and stores
  the descriptor implicit value only when the statement has more than one
  planned values row;
- single-row duplicate-key statements still error for literal `NULL` into
  `NOT NULL`, even in non-strict mode.

`VALUES(column_name)` remains a copy of the already planned inserted-row value
and is not broadened by this feature.

## Diagnostics

Supported adjusted duplicate assignments append these warnings:

| Case | Code | SQLSTATE | Message Shape |
| --- | --- | --- | --- |
| no-default `DEFAULT` adjusted | `1364` | `HY000` | `Field '<column>' doesn't have a default value` |
| multi-row duplicate literal `NULL` adjusted | `1048` | `23000` | `Column '<column>' cannot be null` |
| non-strict character truncation | `1265` | `01000` | `Data truncated for column '<column>' at row <n>` |

Unsupported or strict cases keep the existing errors, including:

- `1048 / 23000` for single-row duplicate literal `NULL` into `NOT NULL`;
- `1364 / HY000` for strict duplicate `DEFAULT` with no explicit default;
- `1406 / 22001` for strict overlength character assignment;
- existing parse, unknown-column, duplicate-key, allocation, and physical
  SQLite failure diagnostics.

## Generated SQLite Handling

The generated physical `UPDATE` shape remains the existing descriptor-built
prepared statement over the stable physical table name. This feature changes
only the bound values and diagnostics produced before binding:

- identifiers are quoted by existing dynamic SQL helpers;
- duplicate-key predicates and assignments continue to use bound parameters;
- integer, text, blob, real, and `NULL` values are bound through existing
  `planned_value` binding helpers;
- no SQLite conflict algorithm or SQLite default expression is used as the
  compatibility authority.

No SQLite fork patch is required.

## Tests

The implementation must cover:

- MySQL 8.4.9 expectation probes for strict and non-strict duplicate branch
  `NULL`, `DEFAULT`, and string truncation behavior;
- successful non-strict duplicate `DEFAULT` on a no-default integer target;
- successful non-strict duplicate string truncation on a `VARCHAR` target;
- successful non-strict multi-row duplicate literal `NULL` into a `NOT NULL`
  integer target;
- strict errors for the same target families;
- single-row non-strict literal `NULL` preserving MySQL's error behavior;
- affected rows, warning counts, warning records where stable, and final stored
  values;
- rollback on strict duplicate-branch failures;
- persistence and existing duplicate-key lifecycle coverage.
