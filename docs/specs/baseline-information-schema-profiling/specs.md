# Baseline INFORMATION_SCHEMA PROFILING

## Status

This phase adds `INFORMATION_SCHEMA.PROFILING` as a MySQL-shaped synthetic
information-schema system view. The table is queryable, exposes MySQL 8.4.9
column and system-table metadata, and returns no rows in MyLite because MyLite
does not implement deprecated statement profiling state or `SHOW PROFILE`
families yet.

The slice also preserves MySQL's direct-read deprecation warning for the table.
It does not add mutable `profiling` / `profiling_history_size` system-variable
semantics, statement profiler collection, `SHOW PROFILE`, `SHOW PROFILES`, or
Performance Schema instrumentation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- Existing information-schema tests under `packages/libmylite/tests/`
- MySQL 8.4 Reference Manual, `PROFILING`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-profiling-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.PROFILING` exists as an `information_schema`
  `SYSTEM VIEW`.
- The target runtime starts with `@@profiling = 0` and
  `@@profiling_history_size = 15`.
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING` returns `0` in the
  default session, and direct reads emit warning `1287 / HY000` with the
  message
  `'INFORMATION_SCHEMA.PROFILING' is deprecated and will be removed in a future release. Please use Performance Schema instead`.
- `SELECT * FROM INFORMATION_SCHEMA.PROFILING WHERE QUERY_ID = -1` emits the
  same warning even though it returns no rows. `LIMIT 0` reads return no rows
  and leave `@@warning_count = 0`.
- When MySQL profiling is enabled with `SET profiling = 1`, MySQL emits a
  separate deprecated-variable warning and later statements can produce
  `PROFILING` rows. MyLite intentionally defers variable mutation and dynamic
  profiler rows.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`, `TABLE_NAME = 'PROFILING'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports 18 columns in order:
  `QUERY_ID`, `SEQ`, `STATE`, `DURATION`, `CPU_USER`, `CPU_SYSTEM`,
  `CONTEXT_VOLUNTARY`, `CONTEXT_INVOLUNTARY`, `BLOCK_OPS_IN`,
  `BLOCK_OPS_OUT`, `MESSAGES_SENT`, `MESSAGES_RECEIVED`,
  `PAGE_FAULTS_MAJOR`, `PAGE_FAULTS_MINOR`, `SWAPS`, `SOURCE_FUNCTION`,
  `SOURCE_FILE`, and `SOURCE_LINE`.
- Integer columns report `DATA_TYPE = 'int'`, `COLUMN_TYPE = 'int'`,
  `COLUMN_DEFAULT = ''`, SQL `NULL` numeric precision and scale, and SQL
  `NULL` character metadata. `QUERY_ID` and `SEQ` are non-null; later integer
  metric columns are nullable.
- `STATE` reports `DATA_TYPE = 'varchar'`, `COLUMN_TYPE = 'varchar(30)'`,
  character set `utf8mb3`, collation `utf8mb3_general_ci`, maximum character
  length `10`, octet length `30`, `COLUMN_DEFAULT = ''`, and non-nullability.
- `SOURCE_FUNCTION` reports the same character metadata as `STATE` and is
  nullable.
- `SOURCE_FILE` reports `DATA_TYPE = 'varchar'`,
  `COLUMN_TYPE = 'varchar(20)'`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, maximum character length `6`, octet length `20`,
  `COLUMN_DEFAULT = ''`, and nullability.
- `DURATION`, `CPU_USER`, and `CPU_SYSTEM` report `DATA_TYPE = 'decimal'`,
  `COLUMN_TYPE = 'decimal(905,0)'`, `COLUMN_DEFAULT = ''`, and SQL `NULL`
  numeric precision and scale. `DURATION` is non-null; CPU columns are nullable.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.PROFILING` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- direct-read deprecation warning `1287 / HY000` unless a supported query is
  limited to zero rows;
- stable zero-row behavior in all MyLite sessions.

Out of scope:

- mutable `profiling` or `profiling_history_size` system variables;
- `SHOW PROFILE` and `SHOW PROFILES`;
- statement profiling collection, profiler state names, timings, CPU counters,
  context switch counters, block I/O counters, page-fault counters, source-code
  locations, or dynamic profiler rows;
- storage, catalog, VFS, or SQLite fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `PROFILING` as a supported information-schema
  system view, returns an empty row set, and appends the deprecation warning
  when MySQL would report one for the supported direct-read forms.
- Information-schema metadata builder: owns `TABLES` and `COLUMNS` synthetic
  rows for this table.
- Profiler/runtime instrumentation: unchanged. No profiler events or timings
  are collected in this slice.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING;
SELECT * FROM INFORMATION_SCHEMA.PROFILING;
SELECT p.QUERY_ID FROM INFORMATION_SCHEMA.PROFILING AS p LIMIT 1;
SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'information_schema'
   AND TABLE_NAME = 'PROFILING'
 ORDER BY ORDINAL_POSITION;
```

## Runtime Semantics

`PROFILING` is registered in the static information-schema table registry. Row
production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static descriptors;
- profiler rows are not generated because MyLite has no statement profiler;
- direct reads append one deprecation warning unless the supported query shape
  has `LIMIT 0`;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

Direct reads of `INFORMATION_SCHEMA.PROFILING` append warning `1287 / HY000`:

```text
'INFORMATION_SCHEMA.PROFILING' is deprecated and will be removed in a future release. Please use Performance Schema instead
```

The feature otherwise relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, ordering, predicates, and limits retain the
  current information-schema query subset behavior;
- allocation failures use existing MyLite allocation diagnostics.

## Performance

The row set is static and empty. Metadata rows are generated from in-memory
descriptors and do not touch durable catalog rows, user storage, profiler state,
or query planner internals.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- empty row count in the default MySQL and MyLite session state;
- deprecation warning count and `SHOW WARNINGS` row for supported direct reads;
- no direct-read warning for supported `LIMIT 0` reads;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- column names for `SELECT *`;
- `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all 18 columns;
- MySQL-runtime observation that enabling deprecated profiling can produce
  dynamic rows, which this baseline defers;
- reopen/file preamble preservation and independent file-backed handles.

Verification before commit:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.information_schema_(profiling|static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_profiling_expectations.sh
cmake --workflow --preset check
```
