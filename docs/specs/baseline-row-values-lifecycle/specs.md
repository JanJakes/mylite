# Baseline Row Values Lifecycle

## Status

This feature specifies the next narrow user-visible row lifecycle slice for
file-backed `.mylite` handles. It adds limited `INSERT ... VALUES` row writes
and descriptor-driven table `SELECT` reads on top of the existing
`mylite_execute()` API, statement context, MyLite parser scaffold, shifted
`.mylite` storage, durable catalog descriptors, and basic create/drop/rename
table lifecycle.

The feature is intentionally not full DML or full `SELECT` support. It supports
only persistent catalog-backed base tables, integer and `NULL` value literals,
and simple table scans without predicates, ordering, aliases, joins, grouping,
or expression evaluation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, integer type ranges:
  https://dev.mysql.com/doc/refman/8.4/en/integer-types.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited `INSERT INTO ... VALUES` subset;
- parser and AST support for `SELECT * FROM table_name` and
  `SELECT column_name[, column_name ...] FROM table_name`;
- single-row and multi-row `VALUES (...), (...)` inserts with statement-level
  all-or-nothing behavior;
- unqualified and schema-qualified table target resolution through the selected
  schema policy already used by table lifecycle statements;
- descriptor-driven insert column resolution, select projection resolution, and
  physical SQLite SQL generation;
- assignment conversion for integer literals with optional unary sign and
  `NULL`, before SQLite binding;
- range checks for `INT`, `INT UNSIGNED`, `BIGINT`, and `BIGINT UNSIGNED`
  descriptor families within the physical signed 64-bit SQLite encoding;
- nullable omitted-column handling and `NOT NULL` no-default rejection;
- affected-row reporting for successful inserts;
- descriptor-driven text results for successful table selects;
- MySQL-shaped diagnostics for the supported subset and deterministic
  diagnostics for deliberate MyLite limitations;
- tests covering success, persistence, rename/drop interaction, atomicity,
  diagnostics, public result behavior, and file-format safety.

## Non-Goals

This feature must not implement:

- general `SELECT`, aliases, table-qualified column references, `WHERE`,
  `ORDER BY`, `LIMIT`, joins, grouping, subqueries, CTEs, set operations,
  window functions, aggregate functions, locking clauses, or expression
  metadata;
- arbitrary SQLite SQL pass-through;
- `UPDATE`, `DELETE`, `REPLACE`, `LOAD DATA`, `INSERT ... SET`,
  `INSERT ... SELECT`, `INSERT IGNORE`, `INSERT ... ON DUPLICATE KEY UPDATE`,
  priorities, partitions, aliases, row aliases, `RETURNING`, or `VALUES ROW()`;
- `DEFAULT` value syntax or default expression semantics;
- auto-increment, `LAST_INSERT_ID()`, generated columns, checks, indexes,
  primary/unique/foreign keys, triggers, temporary tables, views, comments,
  table options, privileges, or locks;
- full type conversion, non-strict warning demotion, charset/collation
  conversion, decimal/float/string/date/time/blob/json values, unsigned 64-bit
  storage above the physical signed 64-bit range, or expression comparison
  semantics;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, and failure cleanup.
- Statement context owns each top-level statement boundary: diagnostics reset,
  warning count, affected rows, backend status, and statement transaction
  state. Successful `INSERT` copies the inserted row count to the public
  result.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves schemas, tables, and columns against MyLite
  catalog descriptors; rejects unsupported grammar and object kinds; builds
  assignment plans; and generates physical SQLite DML/query SQL from
  descriptors.
- The catalog module owns `_mylite_catalog_*` tables, descriptor rows,
  descriptor versions, catalog generation, and descriptor-cache invalidation.
  DML and table `SELECT` do not mutate descriptor rows or catalog generation.
- The result builder owns descriptor-driven result column names and text values.
  SQL `NULL` is represented as a `NULL` pointer returned by
  `mylite_result_value_text()`.
- SQLite owns physical b-tree row storage and rollback durability for generated
  prepared statements. SQLite schema text, `sqlite_master`, and `PRAGMA` output
  remain physical implementation details, not metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Row DML and table scans occur only inside the shifted SQLite payload and must
  not touch byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported `INSERT` subset:

```sql
INSERT INTO table_name VALUES (value[, ...])[, (value[, ...])] ...
INSERT INTO table_name (column_name[, ...]) VALUES (value[, ...])[, (value[, ...])] ...

value:
    integer_literal
  | + integer_literal
  | - integer_literal
  | NULL
```

Supported table `SELECT` subset:

```sql
SELECT * FROM table_name
SELECT column_name[, column_name ...] FROM table_name
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

Only unqualified select column names are supported. Insert column-list names are
also unqualified. The following are rejected in this phase: table-qualified
column references, expressions such as `1 + 2`, string/decimal/float/hex/bit
literals, `TRUE`, `FALSE`, parameters, functions, subqueries, `DEFAULT`,
`VALUE` as a synonym, `VALUES ROW()`, `INSERT ... SET`, `INSERT ... SELECT`,
`INSERT IGNORE`, priorities, partitions, aliases, `ON DUPLICATE KEY UPDATE`,
`RETURNING`, `SELECT` aliases, and every `SELECT` clause after `FROM`.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= insert_values_statement.
statement ::= table_select_statement.

insert_values_statement ::=
    INSERT INTO table_name insert_column_list_opt VALUES insert_row_list.

insert_column_list_opt ::= .
insert_column_list_opt ::= LPAREN identifier_list RPAREN.

identifier_list ::= identifier.
identifier_list ::= identifier_list COMMA identifier.

insert_row_list ::= insert_row.
insert_row_list ::= insert_row_list COMMA insert_row.

insert_row ::= LPAREN insert_value_list RPAREN.

insert_value_list ::= insert_value.
insert_value_list ::= insert_value_list COMMA insert_value.

insert_value ::= INTEGER.
insert_value ::= PLUS INTEGER.
insert_value ::= MINUS INTEGER.
insert_value ::= NULL.

table_select_statement ::= SELECT STAR FROM table_name.
table_select_statement ::= SELECT identifier_list FROM table_name.

table_name ::= identifier.
table_name ::= identifier DOT identifier.
```

The existing parser rule that treats a reserved word after `.` as an identifier
continues to apply. `+1` is admitted because MySQL accepts unary plus in value
expressions and it is still a sign on a literal, not general arithmetic.

## Schema And Table Resolution

`INSERT INTO t ...` and `SELECT ... FROM t` use the connection's selected
schema. If no selected schema exists, the statement fails with MySQL error
`1046`, SQLSTATE `3D000`, and message `No database selected`.

`INSERT INTO db.t ...` and `SELECT ... FROM db.t` resolve `db` against existing
schema descriptors regardless of selected schema. Unknown schemas fail with
MySQL error `1049`, SQLSTATE `42000`, and message
`Unknown database '<schema>'`.

The source table descriptor must exist and must have
`kind == MYLITE_CATALOG_TABLE_KIND_BASE`. Unknown tables fail with MySQL error
`1146`, SQLSTATE `42S02`, and message
`Table '<schema>.<table>' doesn't exist`. Unsupported object kinds are rejected
with a deterministic unsupported diagnostic until view and temporary-table
descriptors are specified.

User-authored schema and table names beginning with `_mylite_`, using ASCII
case-insensitive comparison after identifier unquoting, are rejected before any
SQLite SQL is generated. This preserves the reserved catalog and physical-name
namespace.

## Column Resolution

Column resolution uses MyLite column descriptors read in
`ordinal_position` order. SQLite metadata is not consulted.

For `INSERT` without a column list, the target columns are all table columns in
catalog ordinal order, and each row must provide exactly that many values.

For `INSERT` with a column list:

- each name is resolved case-insensitively against descriptor column names;
- duplicate target names are rejected before conversion or SQLite execution;
- unknown names are rejected before conversion or SQLite execution;
- target ordering follows the column list, not table ordinal order;
- omitted nullable columns are bound as SQL `NULL`;
- omitted `NOT NULL` columns with no default fail because this phase has no
  default descriptors.

For table `SELECT`:

- `*` expands to all descriptor columns in catalog ordinal order;
- explicit projection names are resolved case-insensitively against descriptor
  column names;
- result column names are the descriptor names of the selected columns;
- duplicate projected column names are allowed and produce duplicate result
  columns, matching MySQL's ordinary projection behavior;
- unknown names fail before SQLite SQL is generated;
- qualified column references are rejected in this phase.

The current catalog stores `(table_id, name)` with SQLite's default binary
uniqueness. MyLite analysis nevertheless compares the supported identifier
subset with ASCII case-insensitive folding, matching the previous table
lifecycle duplicate-column rule and observed MySQL behavior.

## Assignment Conversion And Ranges

Supported values are converted by MyLite before SQLite binding:

- `NULL` becomes a SQL NULL binding;
- an unsigned magnitude is parsed from the integer token without using locale;
- optional unary `+` keeps the magnitude positive;
- optional unary `-` negates the magnitude when the target type permits it;
- unsupported literal or expression nodes fail before SQLite execution.

The supported descriptor type ranges are:

| Logical type | Supported range in this phase |
| --- | --- |
| `INT` | `-2147483648` through `2147483647` |
| `INT UNSIGNED` | `0` through `4294967295` |
| `BIGINT` | `-9223372036854775808` through `9223372036854775807` |
| `BIGINT UNSIGNED` | `0` through `9223372036854775807` |

MySQL supports `BIGINT UNSIGNED` up to `18446744073709551615`, but the current
physical encoding stores user values in SQLite `INTEGER`, whose public binding
API is signed 64-bit. This phase therefore explicitly does not support
`BIGINT UNSIGNED` values above `9223372036854775807`; such assignments fail
with MySQL error `1264`, SQLSTATE `22003`, and a message identifying the
column and row. Compatibility docs must not claim full `BIGINT UNSIGNED` value
support until a wider physical encoding is specified and implemented.

This phase follows MySQL's strict-mode transactional behavior for the admitted
integer subset: out-of-range assignments fail, produce no warnings, and leave
the statement's rows uninserted.

## Physical SQLite Handling

Generated physical DML and query SQL must be built only from descriptors:

- target table names come from `_mylite_catalog_tables.physical_name`;
- physical column names come from descriptor column names;
- every SQLite identifier is double-quoted with embedded double quotes doubled;
- every inserted value is bound through `sqlite3_bind_null()` or
  `sqlite3_bind_int64()`;
- generated SQL contains no interpolated user literals.

The `INSERT` implementation prepares one descriptor-driven SQLite statement per
logical `INSERT`:

```sql
INSERT INTO "<physical_name>" ("<col1>", "<col2>", ...)
VALUES (?1, ?2, ...)
```

Each row reuses the prepared statement by binding the converted values, stepping
to `SQLITE_DONE`, resetting, and clearing bindings.

The `SELECT` implementation prepares one descriptor-driven SQLite statement:

```sql
SELECT "<col1>", "<col2>", ... FROM "<physical_name>"
```

`SELECT *` uses the same shape after wildcard expansion. Read values are copied
into MyLite's result builder as text through SQLite's public column APIs:
`SQLITE_NULL` becomes a `NULL` public value pointer and `SQLITE_INTEGER` becomes
its decimal text representation. Any other SQLite storage class for these
descriptor-backed integer columns is treated as physical corruption or internal
failure for this phase.

## Transaction And Failure Unwinding

`INSERT` runs inside one MyLite-owned SQLite `BEGIN IMMEDIATE` transaction. The
implementation must preflight name resolution, target column mapping, omitted
column checks, row value counts, duplicate column checks, and all row value
conversions before the first SQLite row write. It then executes all row writes
inside the transaction and commits only after every row succeeds.

If conversion, binding, stepping, allocation, or SQLite execution fails, the
transaction is rolled back and no row from that statement remains visible.
Successful multi-row inserts report `affected_rows` equal to the number of rows
inserted. Successful supported inserts produce `warning_count == 0`.

Table `SELECT` opens no mutation transaction and does not change descriptor
rows, catalog generation, descriptor versions, descriptor caches, or
`sqlite_schema_generation`.

DML does not mutate `_mylite_catalog_*` rows. Catalog generation and descriptor
versions remain unchanged after successful and failed `INSERT` and `SELECT`.
`sqlite_schema_generation` also remains unchanged.

## Read Ordering

MySQL does not guarantee a general result order for table scans without
`ORDER BY`. Observed MySQL 8.4.9 behavior for the simple single-table InnoDB
heap scans in this feature returns rows in insertion order. MyLite's physical
SQLite table scans currently return rowid insertion order for these generated
tables. Tests may rely on deterministic single-row reads and on insertion order
only for this narrow generated-table proof; compatibility docs must not claim
general unordered `SELECT` ordering compatibility.

## Result Behavior

Successful `INSERT` returns an empty DML result:

- `column_count == 0`;
- `row_count == 0`;
- `affected_rows == inserted row count`;
- `warning_count == 0`.

Successful table `SELECT` returns a text result:

- `column_count` equals the selected descriptor column count;
- `row_count` equals the number of rows read;
- `affected_rows == 0`;
- `warning_count == 0`;
- column names come from selected descriptors;
- integer values are decimal strings;
- SQL `NULL` values return `NULL` from `mylite_result_value_text()`.

This phase does not claim full MySQL result metadata, column type codes, flags,
charsets, origin metadata, display lengths, expression labels, or protocol
metadata.

## Diagnostics

The public function return code indicates MyLite API status. SQL diagnostics
are stored on the database handle.

| Condition | Return | Diagnostic |
| --- | --- | --- |
| Success | `MYLITE_OK` | `0`, `00000`, `not an error` |
| Lexer or parser error | `MYLITE_ERROR` | `1064`, `42000`, MySQL-style syntax message |
| Unsupported parsed insert/select form | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| No selected schema | `MYLITE_ERROR` | `1046`, `3D000`, `No database selected` |
| Unknown schema | `MYLITE_ERROR` | `1049`, `42000`, `Unknown database '<schema>'` |
| Unknown table for insert/select | `MYLITE_ERROR` | `1146`, `42S02`, `Table '<schema>.<table>' doesn't exist` |
| Reserved schema name | `MYLITE_ERROR` | `1102`, `42000`, `Incorrect database name '<name>'` |
| Reserved table name | `MYLITE_ERROR` | `1103`, `42000`, `Incorrect table name '<name>'` |
| Unknown insert/select column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'field list'` |
| Duplicate insert target column | `MYLITE_ERROR` | `1110`, `42000`, `Column '<column>' specified twice` |
| Too few or too many values | `MYLITE_ERROR` | `1136`, `21S01`, `Column count doesn't match value count at row <n>` |
| `NULL` assigned to `NOT NULL` | `MYLITE_ERROR` | `1048`, `23000`, `Column '<column>' cannot be null` |
| Omitted `NOT NULL` column with no default | `MYLITE_ERROR` | `1364`, `HY000`, `Field '<column>' doesn't have a default value` |
| Integer out of range | `MYLITE_ERROR` | `1264`, `22003`, `Out of range value for column '<column>' at row <n>` |
| Unsupported object kind | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| Physical SQLite failure | `MYLITE_ERROR` | `1105`, `HY000`, deterministic internal failure message |
| Allocation failure | `MYLITE_NOMEM` | `MYLITE_NOMEM`, `HY001`, allocation message |

Warnings are not generated by the supported in-range subset.

## MySQL 8.4.9 Runtime Observations

The following behavior was checked on 2026-05-07 against the official
`mysql:8.4.9` Docker image with:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --force --show-warnings
```

Observed behavior used by this feature:

| SQL | MySQL 8.4.9 observation |
| --- | --- |
| `SELECT @@sql_mode` | Includes `STRICT_TRANS_TABLES` in the official image's default mode. |
| `INSERT INTO t VALUES (1, 2, 3)` with all columns present | `ROW_COUNT() = 1`, `@@warning_count = 0`. |
| `INSERT INTO t (nn, a) VALUES (6, +7)` | Succeeds; omitted nullable columns become `NULL`; unary plus is accepted. |
| `INSERT INTO t (a, nn) VALUES (8, 9), (10, 11)` | Succeeds with affected rows `2` and no warnings. |
| Failed multi-row insert due to row 2 value-count mismatch | Error `1136`, SQLSTATE `21S01`; preceding row from the statement is not visible in the transactional table. |
| Failed multi-row insert due to bad row 2 value conversion | Error `1366`, SQLSTATE `HY000`; preceding row from the statement is not visible. MyLite rejects unsupported literals earlier with deterministic syntax/unsupported diagnostics. |
| Too many or too few values | Error `1136`, SQLSTATE `21S01`, row number in message. |
| Unknown insert column | Error `1054`, SQLSTATE `42S22`. |
| Duplicate insert target column | Error `1110`, SQLSTATE `42000`. |
| `NULL` into `NOT NULL` | Error `1048`, SQLSTATE `23000`. |
| Omitted `NOT NULL` column with no default | Error `1364`, SQLSTATE `HY000`. |
| Signed and unsigned integer out-of-range assignments in strict mode | Error `1264`, SQLSTATE `22003`, no inserted row. |
| `SELECT * FROM t` | Column names match table descriptors; `NULL` displays as SQL NULL. |
| `SELECT a, nn FROM t` | Result labels are `a` and `nn`; rows are read from the table. |
| Unknown select column | Error `1054`, SQLSTATE `42S22`. |
| Unknown insert/select table | Error `1146`, SQLSTATE `42S02`. |
| Unknown schema-qualified insert/select table | Error `1049`, SQLSTATE `42000`. |

The reproducible probe script for this phase is
`packages/libmylite/tests/mysql_baseline_row_values_lifecycle_expectations.sh`.

## Compatibility Status

This feature moves only the exact supported subset to partial support:

- `INSERT ... VALUES`: limited persistent base-table insert with optional
  column lists, multi-row values, integer/null values only, no defaults, no
  duplicate-key behavior, no generated ids, and no warnings for the supported
  in-range subset;
- `SELECT`: limited descriptor-driven single base-table projection with `*` or
  unqualified descriptor column names only;
- `INT` / `INTEGER` and `BIGINT`: limited assignment conversion and result text
  readback for the supported literal subset;
- `INT UNSIGNED`: limited assignment conversion and result text readback for
  values in MySQL's `INT UNSIGNED` range;
- `BIGINT UNSIGNED`: limited assignment conversion and result text readback for
  `0` through `9223372036854775807` only;
- type conversion: limited integer/null assignment conversion only.

Full DML, full query expressions, full numeric literal handling, SQL modes,
warning demotion, unsigned 64-bit storage, result metadata, and expression
semantics remain unsupported.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.row_values_lifecycle`.

Coverage must include:

- parser/AST acceptance for supported `INSERT ... VALUES`, multi-row values,
  explicit insert column lists, `SELECT * FROM t`, and `SELECT a, b FROM t`;
- parser or analyzer rejection for unsupported insert/select syntax and value
  forms;
- successful full-row insert and descriptor-driven `SELECT *`;
- successful explicit-column insert where the column order differs from table
  order;
- successful multi-row insert with `affected_rows == row_count`;
- supported range boundaries for `INT`, `INT UNSIGNED`, `BIGINT`, and the
  signed-64-bit subset of `BIGINT UNSIGNED`;
- `NULL` into nullable columns and public `NULL` result value behavior;
- `NULL` into `NOT NULL`, omitted `NOT NULL`, unknown columns, duplicate insert
  target columns, value-count mismatches, out-of-range values, and unsupported
  value forms;
- schema-qualified and unqualified resolution, no selected schema, unknown
  schema, unknown table, and reserved `_mylite_*` source names;
- select projection names, wildcard column order, duplicate projected columns,
  row count, text values, `NULL` values, warning count, and affected-row
  semantics;
- reopen persistence for inserted rows;
- row visibility after `RENAME TABLE`;
- drop after inserted rows removes the physical table and descriptor-driven
  select fails;
- failed multi-row insert leaves no partial rows visible;
- independent file-backed handles keep independent row state;
- unchanged MyLite preamble bytes across row DML and table selects;
- zero-initialized cleanup for new planner/result helpers;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, basic create/drop lifecycle, table rename lifecycle, client-data,
  and registration tests still pass.

MySQL-runtime expectation artifacts must cover the supported user-visible SQL
behavior above and must be checked against MySQL 8.4.9 before implementation
expectations are changed.

## Build Integration

Add any new runtime/analyzer/planner/catalog SQL execution sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.row_values_lifecycle$' --output-on-failure
ctest --preset dev -R '^libmylite\.(parser|runtime\.basic_table_lifecycle|runtime\.table_rename_lifecycle)$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_row_values_lifecycle_expectations.sh
cmake --workflow --preset check
```

Then review the final diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, descriptor-driven physical DML, assignment-conversion correctness,
atomicity, file-format safety, VFS preservation, zero-init safety, cleanup on
failure, scope control, compatibility-matrix accuracy, and test relevance.
