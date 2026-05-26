# Baseline Storage Engine Substitution

## Status

This feature specifies MyLite's first storage-engine substitution slice for
`CREATE TABLE` engine options. MyLite remains an embedded single-engine
database: every supported persistent and temporary user table is physically
stored through the existing MyLite-owned InnoDB-compatible SQLite table path.

The new behavior is deliberately narrow. `ENGINE=InnoDB` remains the only
fully accepted engine name. If an explicit non-InnoDB engine name is supplied
while the session SQL mode does not include `NO_ENGINE_SUBSTITUTION`, MyLite
accepts the statement, records MySQL-shaped warnings, and creates the table
through the existing InnoDB-compatible path. If `NO_ENGINE_SUBSTITUTION` is
enabled, the same engine name remains an error.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline InnoDB engine surface:
  `docs/specs/baseline-innodb-engine-surface/specs.md`
- Baseline SQL mode system variable:
  `docs/specs/baseline-sql-mode-system-variable/specs.md`
- Baseline temporary table lifecycle:
  `docs/specs/baseline-temporary-table-lifecycle/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, storage-engine selection:
  https://dev.mysql.com/doc/refman/8.4/en/storage-engine-setting.html
- MySQL 8.4 Reference Manual, SQL modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_storage_engine_substitution_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- With MySQL's default SQL mode, which includes `NO_ENGINE_SUBSTITUTION`,
  `CREATE TABLE t(id INT) ENGINE=NoSuchEngine` fails with error `1286`,
  SQLSTATE `42000`, and `Unknown storage engine 'NoSuchEngine'`.
- With `SET SESSION sql_mode=''`, the same unavailable engine name succeeds.
  MySQL records two warnings in this order:
  `Warning 1286 Unknown storage engine 'NoSuchEngine'` and
  `Warning 1266 Using storage engine InnoDB for table 't'`.
- The substituted table renders through `SHOW CREATE TABLE` with
  `ENGINE=InnoDB`.
- `ENGINE=''` follows the same loose-mode substitution path, preserving the
  empty engine name inside warning `1286`.
- `CREATE TEMPORARY TABLE ... ENGINE=NoSuchEngine` follows the same strict and
  loose substitution policy. Loose-mode `SHOW CREATE TABLE` renders
  `CREATE TEMPORARY TABLE ... ENGINE=InnoDB`.
- `CREATE TABLE IF NOT EXISTS existing ... ENGINE=NoSuchEngine` in loose mode
  records the engine warnings first, then note `1050` for the existing table.
- The tested MySQL 8.4.9 runtime supports real `MyISAM` and `MEMORY` engine
  plugins. MyLite does not implement those physical engines; this feature
  intentionally treats every non-InnoDB engine name as unavailable for MyLite.

## Scope

The implementation must add:

- SQL-mode-aware validation of `CREATE TABLE` and `CREATE TEMPORARY TABLE`
  `ENGINE` table options;
- unchanged acceptance for `ENGINE [=] InnoDB`, case-insensitive and after the
  existing identifier/string decoding;
- strict-mode rejection of every decoded non-InnoDB engine name with error
  `1286`, SQLSTATE `42000`, and `Unknown storage engine '<name>'`;
- loose-mode substitution for every decoded non-InnoDB engine name when
  `NO_ENGINE_SUBSTITUTION` is not active;
- loose-mode warning `1286` for the requested engine name followed by warning
  `1266` for the InnoDB substitution;
- unchanged physical table planning, stable physical names, descriptor
  authority, catalog writes, generated SQLite SQL, and `.mylite` preamble
  behavior;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and the
  documented MyLite-specific treatment of alternative engines.

## Non-Goals

This feature must not implement:

- real MyISAM, MEMORY, CSV, ARCHIVE, BLACKHOLE, MERGE, FEDERATED, NDB,
  Performance Schema, or other non-InnoDB storage behavior;
- durable per-table engine metadata for substituted engine names;
- alternate rows in `SHOW ENGINES`, `INFORMATION_SCHEMA.ENGINES`, or
  `INFORMATION_SCHEMA.PLUGINS`;
- mutable `@@default_storage_engine`;
- `default_tmp_storage_engine`, `disabled_storage_engines`, engine plugin
  loading, startup options, or persisted variables;
- `ALTER TABLE ... ENGINE=...`, `CREATE TABLE ... LIKE ... ENGINE=...`, engine
  attributes, secondary engines, or partition-engine routing;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement
  execution, result ownership, diagnostics snapshotting, and cleanup.
- Statement context owns SQL-mode state, live diagnostics, previous diagnostics
  snapshots, `warning_count`, and `ROW_COUNT()`.
- Lexer/parser/AST already own `ENGINE [=] option_name` syntax admission. This
  feature does not add grammar.
- Runtime create-table planning owns engine name decoding, SQL-mode-aware
  substitution, and engine diagnostics.
- Catalog descriptors remain authoritative for schema/table/column/index
  metadata. This feature does not add an engine field because substituted
  tables are MyLite InnoDB-compatible tables.
- Result builders and metadata renderers keep the existing InnoDB suffix and
  fixed InnoDB metadata for substituted tables.
- SQLite owns only the generated physical rowid table. Engine names are never
  interpolated into SQLite SQL.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. Engine
  validation must not touch preamble bytes.

## Supported SQL Grammar

This slice reuses the current table-option grammar:

```sql
CREATE [TEMPORARY] TABLE table_name (
    table_definition[, ...]
) ENGINE [=] engine_name

engine_name:
    identifier
  | quoted_identifier
  | string_literal
```

Relevant independently authored MyLite Lemon-syntax shape, already present in
the parser:

```lemon
table_option(A) ::= ENGINE(E) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_engine_option(state, E, N);
}

equal_opt ::= .
equal_opt ::= EQUAL.

option_name ::= identifier.
option_name ::= string_literal.
```

No new syntax is admitted. Unsupported table-option shapes remain governed by
their existing parser and runtime diagnostics.

## Runtime Semantics

Engine names are decoded through the existing table-option name decoder:

- identifiers and backtick-quoted identifiers use identifier decoding;
- string literals use the current SQL-mode-aware string decoder;
- decoded `NUL` bytes remain syntax errors through the existing diagnostics;
- decoded names compare ASCII case-insensitively to `InnoDB`.

If the decoded name is `InnoDB`, planning succeeds without warnings.

If the decoded name is not `InnoDB` and the current session mode includes
`NO_ENGINE_SUBSTITUTION`, planning fails with:

| Field | Value |
| --- | --- |
| Error number | `1286` |
| SQLSTATE | `42000` |
| Message | `Unknown storage engine '<decoded name>'` |

If the decoded name is not `InnoDB` and `NO_ENGINE_SUBSTITUTION` is absent,
planning records:

| Level | Code | Message |
| --- | --- | --- |
| `Warning` | `1286` | `Unknown storage engine '<decoded name>'` |
| `Warning` | `1266` | `Using storage engine InnoDB for table '<target table>'` |

Execution then continues through the current create-table path exactly as
though no engine option had been supplied. Substituted tables:

- use MyLite descriptors and stable `_mylite_user_table_<table_id>` physical
  names;
- render `ENGINE=InnoDB` through `SHOW CREATE TABLE`;
- expose InnoDB metadata through current `SHOW TABLE STATUS` and
  `INFORMATION_SCHEMA.TABLES`;
- persist and reopen like other MyLite base tables;
- do not store the requested unsupported engine name.

`CREATE TABLE IF NOT EXISTS` validates the engine option and records
substitution warnings before the existing-table note, matching the observed
MySQL unavailable-engine warning order.

## Diagnostics

This slice covers:

- strict unsupported engine diagnostics;
- loose unsupported engine warnings and substitution warning order;
- decoded empty engine names;
- decoded string, quoted identifier, and unquoted identifier engine names;
- raw and escaped `NUL` diagnostics through existing decoder policy;
- missing default schema, unknown schema, duplicate table, duplicate columns,
  invalid defaults, unsupported table definitions, physical SQLite failures,
  and allocation failures through existing table-lifecycle paths.

Supported `ENGINE=InnoDB` statements still produce no warnings. Loose
substitution produces exactly two engine warnings before any later
`IF NOT EXISTS` note.

## Compatibility Decisions

The exact MySQL 8.4.9 runtime has several enabled storage engines beyond
InnoDB, including MyISAM and MEMORY. MyLite does not implement those physical
engines because its architecture is a single-file embedded database with a
MyLite-owned compatibility layer on top of SQLite. For this baseline, non-InnoDB
engine names are treated like unavailable engines. This lets compatibility
test harnesses and applications that clear `NO_ENGINE_SUBSTITUTION` continue
schema setup while keeping metadata honest: the created table is an
InnoDB-compatible MyLite table and renders as such.

This feature uses MyLite wrapper/translation logic only. No public SQLite
extension API or targeted SQLite fork hook is needed.

## Tests

Tests must cover:

- MySQL 8.4.9 expectation script for unavailable-engine strict errors and
  loose substitution warnings;
- `CREATE TABLE` and `CREATE TEMPORARY TABLE` substitution;
- `ENGINE=NoSuchEngine`, string-literal engine names, empty-string engine
  names, and a common MyLite-specific alternative such as `ENGINE=MyISAM`;
- `NO_ENGINE_SUBSTITUTION` default/strict rejection and `SET sql_mode=''`
  acceptance;
- `SHOW WARNINGS`, `SHOW COUNT(*) WARNINGS`, `@@warning_count`, result warning
  count, and affected-row behavior;
- `SHOW CREATE TABLE` rendering `ENGINE=InnoDB` after substitution;
- `IF NOT EXISTS` warning/note order;
- row insertion/readback, close/reopen persistence, and independent handles
  after substituted creates;
- unchanged `.mylite` preamble bytes;
- existing engine, SQL mode, create-table, temporary-table, diagnostics,
  parser, and runtime lifecycle tests still pass.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/embedded-design-decisions.md`;
- `docs/compatibility/sql-table-ddl.md`;
- `docs/compatibility/runtime-system-variables.md` only if the documented
  `sql_mode` or `default_storage_engine` wording needs this substitution
  behavior clarified.
