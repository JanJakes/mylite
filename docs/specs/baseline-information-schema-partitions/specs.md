# Baseline INFORMATION_SCHEMA PARTITIONS

## Summary

This phase adds a narrow metadata-only `INFORMATION_SCHEMA.PARTITIONS` slice.
MyLite does not implement partitioned table DDL in this phase. Instead, it
matches MySQL's observable shape for currently supported nonpartitioned base
tables by exposing one descriptor-derived partitions row per visible persistent
base table and one synthetic row per supported `information_schema` system
view.

The row surface is useful for common schema-discovery clients that probe
`INFORMATION_SCHEMA.PARTITIONS` even when an application does not use table
partitioning.

## Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.PARTITIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-partitions-table.html>
- Existing MyLite information-schema static catalog and descriptor metadata
  specs.
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_partitions_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `INFORMATION_SCHEMA.PARTITIONS` has 25 columns:
  `TABLE_CATALOG`, `TABLE_SCHEMA`, `TABLE_NAME`, `PARTITION_NAME`,
  `SUBPARTITION_NAME`, `PARTITION_ORDINAL_POSITION`,
  `SUBPARTITION_ORDINAL_POSITION`, `PARTITION_METHOD`,
  `SUBPARTITION_METHOD`, `PARTITION_EXPRESSION`, `SUBPARTITION_EXPRESSION`,
  `PARTITION_DESCRIPTION`, `TABLE_ROWS`, `AVG_ROW_LENGTH`, `DATA_LENGTH`,
  `MAX_DATA_LENGTH`, `INDEX_LENGTH`, `DATA_FREE`, `CREATE_TIME`,
  `UPDATE_TIME`, `CHECK_TIME`, `CHECKSUM`, `PARTITION_COMMENT`, `NODEGROUP`,
  and `TABLESPACE_NAME`.
- A nonpartitioned user table still appears as one row. Partition and
  subpartition name, method, ordinal, expression, and description columns are
  `NULL`.
- The nonpartitioned row reports table statistics in the storage columns:
  `TABLE_ROWS`, `AVG_ROW_LENGTH`, `DATA_LENGTH`, `MAX_DATA_LENGTH`,
  `INDEX_LENGTH`, and `DATA_FREE`.
- `PARTITION_COMMENT` and `NODEGROUP` are non-`NULL` empty strings for a
  nonpartitioned user table; `TABLESPACE_NAME` is `NULL`.
- `information_schema` system views also appear in `PARTITIONS`. The observed
  `INFORMATION_SCHEMA.PARTITIONS` row for `information_schema.PARTITIONS`
  reports `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, an empty partition comment and
  node group, and `TABLESPACE_NAME = NULL`.
- Successful reads leave `@@warning_count == 0` and make the following
  `ROW_COUNT()` return `-1`.
- Unknown projected, predicate, or order columns use the existing
  `INFORMATION_SCHEMA` unknown-column diagnostics.

Observed partitioned-table rows include real partition names, methods,
expressions, descriptions, and multiple rows per table. MyLite defers those
values because partition DDL is not yet part of the supported table model.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.PARTITIONS` through the existing limited
  information-schema query engine;
- wildcard projection, explicit projection, aliases, `COUNT(*)`, supported
  metadata predicates, one-column `ORDER BY`, and row-count `LIMIT`;
- one synthetic system-view row for each supported `information_schema` table
  definition;
- one descriptor-derived nonpartitioned row for each persistent MyLite base
  table;
- row counts, average row length, index length, auto-tracked create/update
  timestamps, and fixed storage placeholders reused from the existing
  descriptor-driven `INFORMATION_SCHEMA.TABLES` / `SHOW TABLE STATUS` path;
- descriptor-authoritative schema and table names;
- `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` metadata for
  the new system view.

Deferred:

- partitioned table DDL, partition descriptors, subpartitions, partition
  pruning, and partition-maintenance statements;
- partition rows for temporary tables;
- `PARTITION_EXPRESSION`, `PARTITION_DESCRIPTION`, partition comments,
  tablespaces, or node-group behavior beyond the nonpartitioned placeholder
  values;
- physical storage segmentation, partition statistics, partition-level
  timestamps, or optimizer behavior;
- joins, grouping, subqueries, arbitrary functions, wider predicates, privilege
  filtering, or Performance Schema integration;
- SQLite fork patches.

## Ownership Boundaries

- Public API: no new ABI. Applications keep using `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: existing information-schema result, diagnostics,
  warning-count, and previous-row-count behavior apply.
- Parser/AST: no grammar changes. Existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` parsing is reused.
- Analyzer/planner: the existing information-schema query planner resolves
  projections, aliases, predicates, ordering, and limits against the new table
  definition.
- Catalog: MyLite schema/table descriptors remain authoritative. No partition
  descriptors or catalog migrations are introduced.
- Result builder: emits MySQL-shaped text/`NULL` rows through the existing
  `mylite_result` conventions.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: no SQLite metadata reads, arbitrary SQL
  pass-through, fork patch, virtual table, or extension hook is required.

## Query Surface

No new Lemon grammar is required. This feature registers a new table in the
existing information-schema table-definition registry:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.PARTITIONS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The existing information-schema limits still apply. This is not a general
relational expression feature.

## Row Values

For supported MyLite persistent base tables, each table emits exactly one
nonpartitioned row:

| Column | MyLite value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | descriptor schema name |
| `TABLE_NAME` | descriptor table name |
| `PARTITION_NAME` | `NULL` |
| `SUBPARTITION_NAME` | `NULL` |
| `PARTITION_ORDINAL_POSITION` | `NULL` |
| `SUBPARTITION_ORDINAL_POSITION` | `NULL` |
| `PARTITION_METHOD` | `NULL` |
| `SUBPARTITION_METHOD` | `NULL` |
| `PARTITION_EXPRESSION` | `NULL` |
| `SUBPARTITION_EXPRESSION` | `NULL` |
| `PARTITION_DESCRIPTION` | `NULL` |
| `TABLE_ROWS` | exact physical row count from the existing status path |
| `AVG_ROW_LENGTH` | existing status average-row-length placeholder |
| `DATA_LENGTH` | fixed current MyLite table data-length placeholder |
| `MAX_DATA_LENGTH` | `0` |
| `INDEX_LENGTH` | existing descriptor-derived index-length placeholder |
| `DATA_FREE` | `0` |
| `CREATE_TIME` | descriptor creation timestamp rendered in session time zone |
| `UPDATE_TIME` | descriptor update timestamp rendered in session time zone or `NULL` |
| `CHECK_TIME` | `NULL` |
| `CHECKSUM` | `NULL` |
| `PARTITION_COMMENT` | empty string |
| `NODEGROUP` | empty string |
| `TABLESPACE_NAME` | `NULL` |

For supported `information_schema` system views, each view emits one synthetic
nonpartitioned row with `TABLE_CATALOG = def`, `TABLE_SCHEMA =
information_schema`, the system-view table name, partition identity columns
`NULL`, `TABLE_ROWS = 0`, `AVG_ROW_LENGTH = 0`, `DATA_LENGTH = 0`,
`MAX_DATA_LENGTH = 0`, `INDEX_LENGTH = 0`, `DATA_FREE = 0`, timestamp and
checksum columns `NULL`, empty partition comment and node group, and
`TABLESPACE_NAME = NULL`.

## Diagnostics

The feature reuses existing information-schema diagnostics:

- unknown information-schema table: `1109 / 42S02`;
- unknown projected column: `1054 / 42S22`;
- unknown `WHERE` or `ORDER BY` column: existing context-specific
  `1054 / 42S22`;
- unsupported query shapes: existing deterministic MyLite unsupported
  diagnostics;
- allocation failure: `MYLITE_NOMEM` with handle diagnostics.

Successful supported reads introduce no warnings and return row sets through
the existing public result conventions.

## Performance

Rows are built by iterating currently registered information-schema table
definitions and persistent base-table descriptors. MyLite does not scan SQLite
schema text and does not materialize user row data beyond the existing exact
row-count query already used by table-status metadata. This keeps behavior in
the descriptor-owned metadata path and avoids adding partition-specific storage
structures before partition DDL exists.

## Tests

Add a MySQL 8.4.9 expectation script and a fast C runtime test covering:

- system-view metadata through `INFORMATION_SCHEMA.TABLES` and
  `INFORMATION_SCHEMA.COLUMNS`;
- `SELECT *` and explicit projections from `INFORMATION_SCHEMA.PARTITIONS`;
- one nonpartitioned row per persistent base table;
- row count, average row length, index length, create/update timestamps,
  placeholder partition fields, empty comment/nodegroup values, and `NULL`
  tablespace values;
- `COUNT(*)`, `WHERE`, `ORDER BY`, aliases, and `LIMIT` through the existing
  information-schema query engine;
- schema-qualified and selected-`information_schema` reads;
- unknown projected, predicate, and order columns;
- close/reopen persistence, independent handles, and file-format preamble
  preservation;
- existing information-schema, table-status, parser, runtime, file-format, and
  compatibility tests still passing.
