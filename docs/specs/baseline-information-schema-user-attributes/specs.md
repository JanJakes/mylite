# Baseline INFORMATION_SCHEMA USER_ATTRIBUTES

## Status

This phase adds a limited synthetic
`INFORMATION_SCHEMA.USER_ATTRIBUTES` view. MyLite has no account store,
authentication, `CREATE USER`, `ALTER USER`, `mysql.user` table, user comments,
or arbitrary user attributes. The supported baseline is therefore a single
row for MyLite's embedded `root@%` identity with `ATTRIBUTE = NULL`, plus
MySQL 8.4.9-shaped columns and system-view metadata.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing current-user and privilege metadata surfaces:
  `docs/specs/baseline-current-user-identity/specs.md` and
  `docs/specs/baseline-information-schema-privileges/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.USER_ATTRIBUTES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-user-attributes-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_user_attributes_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- The table has columns `USER`, `HOST`, and `ATTRIBUTE`.
- `USER` is non-null `char(32)` with character length `32`, octet length
  `96`, character set `utf8mb3`, collation `utf8mb3_bin`, and default `''`.
- `HOST` is non-null `char(255)` with character and octet length `255`,
  character set `ascii`, collation `ascii_general_ci`, and default `''`.
- `ATTRIBUTE` is nullable `longtext` with character and octet length
  `4294967295`, character set `utf8mb4`, collation `utf8mb4_bin`, and
  default `NULL`.
- The target root runtime exposes rows for `mysql.infoschema@localhost`,
  `mysql.session@localhost`, `mysql.sys@localhost`, `root@%`, and
  `root@localhost`; all have `ATTRIBUTE = NULL` in the default container.
- Reused MySQL comparison runtimes can contain extra accounts created by other
  probes. The MySQL expectation artifact filters to the default accounts above
  when recording the target-runtime row list.
- `SELECT USER, HOST, ATTRIBUTE ... WHERE USER = 'root' AND HOST = '%'`
  returns one `root`, `%`, `NULL` row.
- `INFORMATION_SCHEMA.TABLES` reports `USER_ATTRIBUTES` as a system view with
  `ENGINE = NULL`, `VERSION = 10`, `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`,
  `DATA_LENGTH = 0`, empty `CREATE_OPTIONS`, and empty `TABLE_COMMENT`.
- Successful supported reads return warning count `0` and make the following
  `ROW_COUNT()` return `-1`.
- Unsupported predicate columns fail with `1054 / 42S22`.

## Scope

The implementation must add:

- a synthetic `INFORMATION_SCHEMA.USER_ATTRIBUTES` table definition;
- a single MyLite-owned row for the embedded `root@%` identity:
  `USER = 'root'`, `HOST = '%'`, `ATTRIBUTE = NULL`;
- matching `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS`
  system metadata for the table;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit metadata-column projection, aliases, `COUNT(*)`, supported
  predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- selected-`information_schema` resolution for unqualified
  `USER_ATTRIBUTES`;
- fast C tests plus a reproducible MySQL 8.4.9 expectation script.

## Non-Goals

This feature must not implement:

- account storage, authentication, passwords, `CREATE USER`, `ALTER USER`,
  `DROP USER`, `RENAME USER`, `mysql.user`, roles, grants, revokes, or
  privilege enforcement;
- user comments, user-defined account attributes, JSON attribute mutation, or
  `ATTRIBUTE` expression extraction behavior beyond returning the stored
  `NULL` value;
- MySQL's internal system-account rows such as `mysql.session` or
  `mysql.sys`;
- privilege-dependent row filtering;
- joins, grouping, subqueries, or wider information-schema query support;
- physical `information_schema` SQLite tables, storage-format changes, or
  SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Parser/AST: no grammar changes. The existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` syntax is reused.
- Analyzer/planner: the existing information-schema resolver owns source
  matching, projection, aliases, predicates, ordering, and limits against the
  synthetic table definition.
- Catalog metadata: the row is MyLite-owned session identity metadata, not a
  persistent catalog descriptor. Future account descriptors must be designed
  before additional rows are emitted.
- Result builder: emits MySQL-shaped text and `NULL` values through
  `mylite_result`.
- Storage/VFS and SQLite physical storage: unchanged. No `.mylite` file-format
  change, public SQLite extension API, or targeted SQLite fork hook is needed.

## Supported Query Surface

No new Lemon grammar is required. The existing limited information-schema
`SELECT` surface admits:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.USER_ATTRIBUTES [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified or selected-`information_schema` source resolution;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, arbitrary expressions, grouping, mutation, or DDL
  through this table.

## Columns

| Column | Type metadata | MyLite value source |
| --- | --- | --- |
| `USER` | non-null `char(32)`, `utf8mb3_bin`, default `''` | Fixed `root` |
| `HOST` | non-null `char(255)`, `ascii_general_ci`, default `''` | Fixed `%` |
| `ATTRIBUTE` | nullable `longtext`, `utf8mb4_bin`, default `NULL` | Fixed SQL `NULL` |

## Runtime Semantics

- `SELECT * FROM INFORMATION_SCHEMA.USER_ATTRIBUTES` returns the one embedded
  root identity row.
- Predicates are evaluated against that row using the existing
  information-schema predicate evaluator.
- `COUNT(*)` counts the synthetic MyLite row after predicate filtering.
- The `USER` column remains a metadata column in this table; unquoted
  `SELECT USER FROM INFORMATION_SCHEMA.USER_ATTRIBUTES` must resolve as that
  column, not as `USER()`.
- `INFORMATION_SCHEMA.TABLES` and `SHOW TABLES` / `SHOW FULL TABLES` listing
  include the table because the built-in table directory already lists
  MySQL 8.4.9 built-in schema names. This slice makes the table queryable and
  adds column metadata.
- Mutating built-in schema access remains governed by the existing built-in
  schema write guards.

## Tests

MySQL 8.4.9 expectation coverage:

- verify the target runtime version;
- verify the default root `root@%` row is visible with `ATTRIBUTE = NULL`;
- record the default target-runtime row list for context;
- verify successful read diagnostics;
- verify unsupported predicate-column diagnostics;
- verify `INFORMATION_SCHEMA.TABLES` system-view metadata;
- verify `INFORMATION_SCHEMA.COLUMNS` metadata for all three columns.

MyLite C runtime coverage:

- wildcard and explicit column projection, including the unquoted `USER`
  column;
- `COUNT(*)`, predicate filtering, alias resolution, ordering, and selected
  `information_schema` resolution;
- `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` rows;
- unsupported predicate-column diagnostics.

Focused verification:

```sh
packages/libmylite/tests/mysql_baseline_information_schema_user_attributes_expectations.sh
cmake --build --preset dev --target mylite_runtime_information_schema_user_attributes_test
ctest --preset dev --output-on-failure -R '^libmylite\.runtime\.information_schema_user_attributes$'
git diff --check
CC=clang cmake --workflow --preset check
```
