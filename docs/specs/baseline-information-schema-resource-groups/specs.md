# Baseline INFORMATION_SCHEMA RESOURCE_GROUPS

## Status

This phase adds `INFORMATION_SCHEMA.RESOURCE_GROUPS` as a queryable synthetic
information-schema system view. It exposes MySQL 8.4.9-shaped table and column
metadata and returns the two built-in default resource groups that MySQL shows
in a fresh server.

The slice is metadata-only. It does not add resource-group DDL, scheduler
integration, thread assignment, optimizer hints, privilege filtering, mutable
resource groups, or persisted resource-group descriptors.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.RESOURCE_GROUPS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-resource-groups-table.html
- MySQL 8.4 Reference Manual, Resource Groups:
  https://dev.mysql.com/doc/refman/8.4/en/resource-groups.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.RESOURCE_GROUPS` exists as an `information_schema`
  `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`, `TABLE_NAME = 'RESOURCE_GROUPS'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports five columns in order:
  `RESOURCE_GROUP_NAME`, `RESOURCE_GROUP_TYPE`, `RESOURCE_GROUP_ENABLED`,
  `VCPU_IDS`, and `THREAD_PRIORITY`.
- `RESOURCE_GROUP_NAME` is non-null `varchar(64)`, character set `utf8mb3`,
  collation `utf8mb3_general_ci`, character maximum length `64`, octet length
  `192`, and `COLUMN_DEFAULT = NULL`.
- `RESOURCE_GROUP_TYPE` is non-null `enum('SYSTEM','USER')`, character set
  `utf8mb3`, collation `utf8mb3_bin`, character maximum length `6`, octet
  length `18`, and `COLUMN_DEFAULT = NULL`.
- `RESOURCE_GROUP_ENABLED` is non-null `tinyint(1)` with `DATA_TYPE =
  'tinyint'`, numeric precision `3`, scale `0`, and SQL `NULL` character
  metadata.
- `VCPU_IDS` is nullable `blob` with character maximum and octet lengths
  `65535`, SQL `NULL` character set/collation, and SQL `NULL` numeric
  metadata.
- `THREAD_PRIORITY` is non-null `int` with numeric precision `10`, scale `0`,
  and SQL `NULL` character metadata.
- A fresh runtime returns two default rows. With explicit ordering by
  `RESOURCE_GROUP_NAME` on the probed runtime, the rows were
  `SYS_default / SYSTEM / 1 / 0-17 / 0` and
  `USR_default / USER / 1 / 0-17 / 0`.
- Runtime probes returned both default rows; tests use an explicit
  `ORDER BY RESOURCE_GROUP_NAME` rather than depending on natural scan order.
- MySQL documents `VCPU_IDS` for default groups as the range of all available
  virtual CPUs, and the displayed value varies by system. MyLite should
  therefore expose a deterministic all-online-CPU range for the current
  process, falling back to `0` when the platform does not report a count.
- Successful supported reads leave `@@warning_count = 0`, and `ROW_COUNT()`
  reports `-1` after the `SELECT`.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.RESOURCE_GROUPS` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- two built-in metadata rows:
  `USR_default / USER / 1 / <all-online-vcpus> / 0` and
  `SYS_default / SYSTEM / 1 / <all-online-vcpus> / 0`;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- `CREATE RESOURCE GROUP`, `ALTER RESOURCE GROUP`, `DROP RESOURCE GROUP`, and
  `SET RESOURCE GROUP`;
- `RESOURCE_GROUP` optimizer hints;
- mutable resource-group descriptors, persisted resource-group state, thread
  assignment, OS scheduling, priority enforcement, CPU affinity enforcement, or
  resource-group status variables;
- user-defined resource groups;
- privilege filtering or account-specific visibility;
- physical MySQL data dictionary tables;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `RESOURCE_GROUPS` as a supported synthetic
  information-schema system view and appends two static default rows.
- Catalog metadata: unchanged. No descriptors are introduced.
- Session/thread runtime: unchanged. Resource groups are visible metadata only;
  they do not affect execution.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.RESOURCE_GROUPS;
SELECT RESOURCE_GROUP_NAME, RESOURCE_GROUP_TYPE, RESOURCE_GROUP_ENABLED,
       VCPU_IDS, THREAD_PRIORITY
  FROM INFORMATION_SCHEMA.RESOURCE_GROUPS
 ORDER BY RESOURCE_GROUP_NAME;
SELECT r.THREAD_PRIORITY
  FROM INFORMATION_SCHEMA.RESOURCE_GROUPS AS r
 WHERE r.RESOURCE_GROUP_NAME = 'USR_default';
```

## Runtime Semantics

`RESOURCE_GROUPS` is registered in the static information-schema table
registry. Row production emits the MySQL default resource groups:

- `USR_default`, type `USER`, enabled `1`, priority `0`;
- `SYS_default`, type `SYSTEM`, enabled `1`, priority `0`.

Both rows share the same `VCPU_IDS` text. MyLite formats this as `0` for a
single online CPU and `0-N` for more than one online CPU, where `N` is the last
zero-based online CPU index reported by the current process. If the platform
cannot report an online CPU count, MyLite uses `0`. This follows MySQL's
documented system-dependent display while keeping the embedded implementation
metadata-only.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, ordering, and limits
  retain the current information-schema query subset behavior;
- allocation and formatting failures use existing MyLite runtime diagnostics.

Resource-group DDL, resource-group assignment statements, and optimizer hints
remain outside this table's surface and continue to follow the existing parser
or unsupported-feature behavior.

## Performance

The table emits two synthetic rows and formats one short CPU range string. It
does not read or write MyLite catalog descriptors, physical row storage, SQLite
tables, or OS scheduler state.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels for `SELECT *`;
- the two default resource-group rows, ordered by `RESOURCE_GROUP_NAME`;
- case-insensitive table-name lookup;
- alias projection for one default row;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all five columns;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_resource_groups_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_resource_groups|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_resource_groups_expectations.sh
git diff --check
cmake --workflow --preset check
```
