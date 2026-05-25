# Baseline SHOW PLUGINS Metadata

## Summary

This phase adds a narrow plugin-introspection surface for common client
discovery:

- `SHOW PLUGINS`
- `INFORMATION_SCHEMA.PLUGINS`

MyLite is embedded and does not load MySQL server plugins. The supported result
therefore exposes one synthetic active `InnoDB` storage-engine plugin row,
matching the storage-engine compatibility row already exposed by `SHOW ENGINES`
and `INFORMATION_SCHEMA.ENGINES`. This is not full MySQL plugin management or a
complete MySQL server plugin inventory.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline InnoDB engine surface:
  `docs/specs/baseline-innodb-engine-surface/specs.md`
- Baseline INFORMATION_SCHEMA static catalogs:
  `docs/specs/baseline-information-schema-static-catalogs/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW PLUGINS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-plugins.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.PLUGINS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-plugins-table.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_show_plugins_metadata_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records these observed behaviors:

- `SHOW PLUGINS` returns columns `Name`, `Status`, `Type`, `Library`, and
  `License`.
- The MySQL 8.4.9 `InnoDB` row reports `Status = ACTIVE`,
  `Type = STORAGE ENGINE`, `Library = NULL`, and `License = GPL`.
- `SHOW PLUGINS` does not accept `LIKE`, `WHERE`, `FULL`, or `FROM` clauses.
- Successful `SHOW PLUGINS` leaves `@@warning_count == 0` and makes the next
  `ROW_COUNT()` return `-1`.
- `INFORMATION_SCHEMA.PLUGINS` has eleven columns:
  `PLUGIN_NAME`, `PLUGIN_VERSION`, `PLUGIN_STATUS`, `PLUGIN_TYPE`,
  `PLUGIN_TYPE_VERSION`, `PLUGIN_LIBRARY`, `PLUGIN_LIBRARY_VERSION`,
  `PLUGIN_AUTHOR`, `PLUGIN_DESCRIPTION`, `PLUGIN_LICENSE`, and `LOAD_OPTION`.
- The observed MySQL 8.4.9 `INFORMATION_SCHEMA.PLUGINS` `InnoDB` row reports:
  `InnoDB`, `8.4`, `ACTIVE`, `STORAGE ENGINE`, `80409.0`, `NULL`, `NULL`,
  `Oracle Corporation`, `Supports transactions, row-level locking, and foreign
  keys`, `GPL`, and `FORCE`.
- System-view rows in `INFORMATION_SCHEMA.TABLES` use `TABLE_TYPE =
  'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`, `ROW_FORMAT = NULL`,
  `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` metadata for `PLUGINS` is pinned to observed
  MySQL 8.4.9 values by the expectation script. Notable details:
  `PLUGIN_DESCRIPTION` is `varchar(65535)` with character maximum length
  `21845`, and text metadata uses `utf8mb3` / `utf8mb3_general_ci`.

The full MySQL runtime exposes many installed or disabled server plugins. MyLite
intentionally exposes only rows that correspond to implemented embedded
semantics.

## Scope

The implementation must add:

- parser and AST support for `SHOW PLUGINS`;
- a planned result builder for `SHOW PLUGINS` with MySQL 8.4.9 column labels
  and the synthetic `InnoDB` row;
- a `PLUGINS` definition in MyLite's limited information-schema registry;
- one synthetic `INFORMATION_SCHEMA.PLUGINS` row for MyLite's embedded InnoDB
  compatibility surface;
- system `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` rows for
  the new view through the existing registration-driven metadata path;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit projection, aliases, `COUNT(*)`, the supported metadata predicate
  subset, one-column `ORDER BY`, and row-count `LIMIT`;
- warning and row-count behavior matching observed MySQL 8.4.9 and existing
  MyLite result conventions;
- fast C tests, parser tests, a MySQL 8.4.9 expectation artifact, and
  compatibility documentation.

## Non-Goals

This feature must not implement:

- `INSTALL PLUGIN`, `UNINSTALL PLUGIN`, `INSTALL COMPONENT`, or
  `UNINSTALL COMPONENT`;
- loadable plugin libraries, plugin descriptors, component descriptors, or
  plugin lifecycle state;
- the complete MySQL server plugin inventory, disabled plugin rows, NDB rows,
  Performance Schema rows, authentication plugins, audit plugins, or parser
  plugins;
- alternate storage engines beyond the existing embedded InnoDB surface;
- `SHOW PLUGINS` filters, `SHOW FULL PLUGINS`, schema-qualified `SHOW PLUGINS`,
  or privilege filtering;
- `INFORMATION_SCHEMA` joins, subqueries, grouping, aggregation beyond existing
  `COUNT(*)`, or wider information-schema query support;
- durable catalog migrations, SQLite metadata tables, SQLite fork patches, or
  extension hooks.

## Ownership Boundaries

- Public API: no new ABI. Applications use existing `mylite_execute()` and
  result accessors.
- Statement context: no new session state. Existing diagnostics, warning count,
  previous-row-count, and result lifetime behavior apply.
- Lexer/parser/AST: owns syntax admission for the new `SHOW PLUGINS` statement.
  Parser code remains independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner: no new descriptor resolution. Existing
  information-schema planning resolves projections, aliases, predicates,
  ordering, and limits against the synthetic `PLUGINS` table definition.
- Catalog module: no catalog rows are read or written. Persistent MyLite
  descriptors remain authoritative for user schemas and tables.
- Result builder: emits MySQL-shaped column labels and text/`NULL` values
  through existing `mylite_result` conventions.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: no SQLite table, arbitrary SQL pass-through, fork
  patch, or extension hook is required. This is MyLite-owned synthetic
  metadata.

## Supported Grammar

This phase adds one SHOW statement:

```sql
SHOW PLUGINS
```

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= show_plugins_statement(B). {
    A = B;
}

show_plugins_statement(A) ::= SHOW(S) PLUGINS(P). {
    A = mylite_sql_parser_make_show_plugins_statement(state, S, P);
}
```

Unsupported `SHOW PLUGINS LIKE ...`, `SHOW PLUGINS WHERE ...`,
`SHOW FULL PLUGINS`, `SHOW PLUGINS FROM ...`, and similar forms remain syntax
errors.

## `SHOW PLUGINS` Result

`SHOW PLUGINS` returns five columns in this order:

| Column | MyLite value |
| --- | --- |
| `Name` | `InnoDB` |
| `Status` | `ACTIVE` |
| `Type` | `STORAGE ENGINE` |
| `Library` | `NULL` |
| `License` | `GPL` |

The row is compatibility metadata for MyLite's embedded InnoDB-compatible
surface. It does not describe a loadable plugin library or MyLite's project
license.

Successful `SHOW PLUGINS` returns a row result, sets the previous row count to
`-1`, and reports `warning_count == 0`.

## `INFORMATION_SCHEMA.PLUGINS` Columns and Row

`PLUGINS` has eleven columns in this order:

| Column | MyLite value |
| --- | --- |
| `PLUGIN_NAME` | `InnoDB` |
| `PLUGIN_VERSION` | `8.4` |
| `PLUGIN_STATUS` | `ACTIVE` |
| `PLUGIN_TYPE` | `STORAGE ENGINE` |
| `PLUGIN_TYPE_VERSION` | `80409.0` |
| `PLUGIN_LIBRARY` | `NULL` |
| `PLUGIN_LIBRARY_VERSION` | `NULL` |
| `PLUGIN_AUTHOR` | `Oracle Corporation` |
| `PLUGIN_DESCRIPTION` | `Supports transactions, row-level locking, and foreign keys` |
| `PLUGIN_LICENSE` | `GPL` |
| `LOAD_OPTION` | `FORCE` |

System `COLUMNS` metadata follows observed MySQL 8.4.9 values:

- `PLUGIN_NAME` is non-null `varchar(64)` with observed maximum length `21`;
- `PLUGIN_VERSION` is non-null `varchar(20)` with observed maximum length `6`;
- `PLUGIN_STATUS` is non-null `varchar(10)` with observed maximum length `3`;
- `PLUGIN_TYPE` is non-null `varchar(80)` with observed maximum length `26`;
- `PLUGIN_TYPE_VERSION` is non-null `varchar(20)` with observed maximum length
  `6`;
- `PLUGIN_LIBRARY` is nullable `varchar(64)` with observed maximum length `21`;
- `PLUGIN_LIBRARY_VERSION` is nullable `varchar(20)` with observed maximum
  length `6`;
- `PLUGIN_AUTHOR` is nullable `varchar(64)` with observed maximum length `21`;
- `PLUGIN_DESCRIPTION` is nullable `varchar(65535)` with observed maximum
  length `21845`;
- `PLUGIN_LICENSE` is nullable `varchar(80)` with observed maximum length `26`;
- `LOAD_OPTION` is non-null `varchar(64)` with observed maximum length `21`;
- all text columns use `utf8mb3` / `utf8mb3_general_ci`, and privileges are
  exposed through the existing information-schema `COLUMNS` path as `select`.

The existing information-schema query limits still apply:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.PLUGINS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

## Diagnostics

- Unsupported `SHOW PLUGINS` grammar forms are syntax errors with the existing
  parser diagnostic shape.
- Unknown `INFORMATION_SCHEMA.PLUGINS` projection, predicate, or ordering
  columns use the existing information-schema unknown-column diagnostics.
- `INFORMATION_SCHEMA.PLUGINS` writes are covered by the existing
  `information_schema` read-only access policy.
- Allocation failures use existing `MYLITE_NOMEM` and handle diagnostics.
- Physical SQLite failures are not expected because no physical SQLite query is
  generated for this metadata surface; unexpected result-building failures use
  existing runtime error handling.

## Performance and Storage

`SHOW PLUGINS` builds a one-row in-memory result directly. `INFORMATION_SCHEMA`
queries reuse the existing synthetic metadata row engine, producing one row
before applying the supported projection, predicate, ordering, and limit logic.
No user table rows are scanned, no catalog rows are mutated, and no SQLite SQL
is generated for the plugin row itself.

## Test Plan

Fast C tests must cover:

- parser acceptance for `SHOW PLUGINS`;
- parser rejection for unsupported `LIKE`, `WHERE`, `FULL`, and `FROM` forms;
- successful `SHOW PLUGINS` column labels, values, `warning_count == 0`, and
  row-count behavior;
- `INFORMATION_SCHEMA.PLUGINS` wildcard and explicit projections;
- case-insensitive predicates over plugin names and status/type fields;
- ordering and limiting over the single row;
- `COUNT(*)` over matching and nonmatching plugin predicates;
- `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` metadata rows for
  `PLUGINS`;
- unknown information-schema projection, predicate, and ordering columns;
- file-backed reopen and independent handles preserving the same static rows
  and the `.mylite` preamble.

The MySQL expectation script must verify the MySQL 8.4.9 values, metadata, and
syntax diagnostics used by this spec.
