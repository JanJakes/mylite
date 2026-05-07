# Baseline Basic Table Lifecycle

## Status

This feature specifies the first narrow user-visible SQL execution and
persistent table lifecycle slice for file-backed `.mylite` handles. It adds a
small public execution/result API, parses and executes a limited `USE`,
`CREATE TABLE`, `DROP TABLE`, and `SHOW TABLES` surface, stores logical table
descriptors in the MyLite catalog, and creates or drops the corresponding
physical SQLite table inside the shifted payload.

The feature is intentionally not full table DDL support. It supports only
persistent base tables with simple integer columns and catalog-backed metadata.
General DML, `SELECT`, `CREATE DATABASE`, table options, keys, constraints,
defaults, temporary tables, views, `INFORMATION_SCHEMA`, and generalized
introspection remain out of scope.

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
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `DROP TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/drop-table.html
- MySQL 8.4 Reference Manual, `SHOW TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-tables.html
- MySQL 8.4 Reference Manual, `USE`:
  https://dev.mysql.com/doc/refman/8.4/en/use.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, keywords:
  https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- MySQL 8.4 Reference Manual, integer types:
  https://dev.mysql.com/doc/refman/8.4/en/integer-types.html
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- a narrow public SQL execution API and result handle;
- parser and AST support for the limited grammar in this spec;
- execution of `USE schema_name` for existing MyLite catalog schemas;
- execution of `CREATE TABLE` for persistent base tables with explicit column
  lists;
- execution of `DROP TABLE` for one persistent base table;
- execution of a minimal descriptor-driven `SHOW TABLES` result;
- schema resolution against connection state and MyLite catalog descriptors;
- duplicate table, unknown table, duplicate column, reserved-name, unsupported
  syntax, and physical SQLite failure diagnostics;
- statement-boundary atomicity for catalog row changes and physical SQLite
  schema changes;
- durable catalog generation advancement and descriptor-cache invalidation after
  successful table DDL;
- tests for catalog rows, physical SQLite schema, persistence, independent
  handles, file-format preamble safety, public API misuse, result cleanup, and
  MySQL-runtime-verified behavior.

## Non-Goals

This feature must not implement:

- public schema creation or deletion (`CREATE DATABASE`, `DROP DATABASE`);
- general `SELECT`, `INSERT`, `UPDATE`, `DELETE`, joins, or expressions beyond
  the grammar needed for this feature;
- arbitrary SQLite SQL pass-through;
- table options, `IF NOT EXISTS`, `IF EXISTS`, multi-table `DROP TABLE`,
  temporary tables, views, indexes, keys, constraints, defaults, generated
  columns, comments, partitions, triggers, routines, events, or
  `INFORMATION_SCHEMA`;
- table reconstruction from SQLite schema text;
- full MySQL type conversion, assignment checks, auto-increment semantics, or
  write-boundary conversion;
- SQLite fork patches.

## Public API

Add an opaque public result handle and length-aware execution function to
`mylite/mylite.h`:

```c
typedef struct mylite_result mylite_result;

MYLITE_API int mylite_execute(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
);
MYLITE_API void mylite_result_free(mylite_result *result);

MYLITE_API size_t mylite_result_column_count(const mylite_result *result);
MYLITE_API const char *mylite_result_column_name(
    const mylite_result *result,
    size_t column_index
);
MYLITE_API size_t mylite_result_row_count(const mylite_result *result);
MYLITE_API const char *mylite_result_value_text(
    const mylite_result *result,
    size_t row_index,
    size_t column_index
);
MYLITE_API int64_t mylite_result_affected_rows(const mylite_result *result);
MYLITE_API size_t mylite_result_warning_count(const mylite_result *result);
```

`mylite_execute()` borrows `sql` for the duration of the call. `database`,
`sql`, and `out_result` are required. `sql_size` is a byte length and may be
zero when `sql` points to an empty string. On entry, `*out_result` is set to
`NULL`.

On success, `mylite_execute()` returns `MYLITE_OK` and stores an owned result
handle in `*out_result`. The caller must release it with
`mylite_result_free()`. DDL and `USE` success produce an empty result:
`column_count == 0`, `row_count == 0`, `affected_rows == 0`, and
`warning_count == 0`. `SHOW TABLES` success produces one text column and zero or
more rows. Result column names and values are owned by the result and remain
valid until `mylite_result_free()`.

On SQL failure, `mylite_execute()` returns `MYLITE_ERROR`, leaves
`*out_result == NULL`, and stores the MySQL-shaped condition on `mylite_db`.
On allocation failure it returns `MYLITE_NOMEM`, leaves `*out_result == NULL`,
and stores `HY001` diagnostics where a handle is available. On public API
misuse it returns `MYLITE_MISUSE`, leaves `*out_result == NULL`, and stores
misuse diagnostics where a handle is available.

`mylite_result_free(NULL)` is a no-op. Result accessors return `0` or `NULL`
for `NULL` handles or out-of-range indexes.

The public API remains C-compatible and does not expose SQLite types. Public
status codes remain MyLite-owned status codes. SQL diagnostic numbers stored on
the database handle may be MySQL error numbers.

## Ownership Boundary

- The public API owns SQL execution entry, result-handle allocation, result
  lifetime, public cleanup, and public misuse behavior.
- Statement context owns the per-call statement boundary: diagnostics reset,
  warning count, affected rows, backend status, wrapper transaction state, and
  planned-result metadata.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves names, validates this feature's semantic
  subset, chooses logical descriptors, creates physical names, and generates
  SQLite DDL strings. It does not mutate storage directly before the statement
  transaction is open.
- The catalog module owns `_mylite_catalog_*` tables, descriptor rows,
  descriptor versions, generation advancement, and cache invalidation.
- SQLite owns only physical b-trees, transaction durability, and execution of
  generated internal SQL. SQLite schema text and `PRAGMA` output are not
  MySQL-visible metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)` after open.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call. Empty
input returns an empty successful result. Multi-statement scripts are rejected
with a deterministic unsupported-statement diagnostic.

Supported `USE` subset:

```sql
USE schema_name
```

Supported `CREATE TABLE` subset:

```sql
CREATE TABLE table_name (
    column_name integer_type [NULL | NOT NULL]
    [, column_name integer_type [NULL | NOT NULL]] ...
)

integer_type:
    INT
  | INTEGER
  | BIGINT
  | INT UNSIGNED
  | INTEGER UNSIGNED
  | BIGINT UNSIGNED

table_name:
    identifier
  | identifier.identifier
```

Supported `DROP TABLE` subset:

```sql
DROP TABLE table_name
```

Supported `SHOW TABLES` subset:

```sql
SHOW TABLES
SHOW TABLES FROM schema_name
SHOW TABLES IN schema_name
```

`IF NOT EXISTS`, `IF EXISTS`, multi-table drop, temporary tables, table options,
column defaults, display widths, `SIGNED`, `ZEROFILL`, keys, constraints, and
all non-integer types are unsupported in this phase.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= use_statement.
statement ::= create_table_statement.
statement ::= drop_table_statement.
statement ::= show_tables_statement.

use_statement ::= USE identifier.

create_table_statement ::= CREATE TABLE table_name LPAREN column_definition_list RPAREN.
column_definition_list ::= column_definition.
column_definition_list ::= column_definition_list COMMA column_definition.
column_definition ::= identifier integer_type nullability_opt.

integer_type ::= INT.
integer_type ::= INTEGER.
integer_type ::= BIGINT.
integer_type ::= INT UNSIGNED.
integer_type ::= INTEGER UNSIGNED.
integer_type ::= BIGINT UNSIGNED.

nullability_opt ::= .
nullability_opt ::= NULL.
nullability_opt ::= NOT NULL.

drop_table_statement ::= DROP TABLE table_name.

show_tables_statement ::= SHOW TABLES.
show_tables_statement ::= SHOW TABLES FROM identifier.
show_tables_statement ::= SHOW TABLES IN identifier.

table_name ::= qualified_identifier.
```

The existing parser rule that treats a reserved word after `.` as an identifier
continues to apply. Unquoted reserved words outside qualified-name tail
positions remain syntax errors unless this grammar maps them to a dedicated
terminal.

## Schema Resolution

`CREATE TABLE t (...)`, `DROP TABLE t`, and `SHOW TABLES` without a schema
qualifier use the connection's selected schema. If no selected schema exists,
the statement fails with MySQL error `1046`, SQLSTATE `3D000`, and message
`No database selected`.

`USE schema_name` requires an existing schema descriptor in the MyLite catalog.
On success, it copies the schema name into connection-local session state and
sets the selected-schema flag. Unknown schema fails with MySQL error `1049`,
SQLSTATE `42000`, and message `Unknown database '<schema>'`.

`CREATE TABLE db.t (...)`, `DROP TABLE db.t`, and `SHOW TABLES FROM db` /
`SHOW TABLES IN db` resolve `db` against existing schema descriptors regardless
of the selected schema. Unknown schema fails with error `1049`.

Public schema DDL is out of scope. Tests may seed schema descriptors through
the internal catalog API before exercising this table lifecycle surface.

## Identifier Handling

Identifiers are copied from AST spans into owned, NUL-terminated internal
buffers. Backtick-quoted identifiers are unquoted and doubled backticks are
collapsed. This phase rejects identifiers that do not fit the existing catalog
identifier capacity.

User-authored schema, table, and column names beginning with `_mylite_`, using
ASCII case-insensitive comparison, are reserved for MyLite and are rejected
before any physical SQLite SQL is generated. This prevents collisions with
catalog tables and generated physical table names.

Duplicate column names are rejected by MyLite analysis before catalog or
physical schema mutation. This phase compares column names byte-for-byte after
identifier unquoting; broader MySQL identifier case-folding policy remains a
future schema/identifier feature.

## Column Descriptors

Each supported column maps to one `_mylite_catalog_columns` row:

| MySQL input | `logical_type` | `physical_type` | `is_nullable` |
| --- | --- | --- | --- |
| `INT` | `INT` | `INTEGER` | `1` unless `NOT NULL` |
| `INTEGER` | `INT` | `INTEGER` | `1` unless `NOT NULL` |
| `BIGINT` | `BIGINT` | `INTEGER` | `1` unless `NOT NULL` |
| `INT UNSIGNED` | `INT UNSIGNED` | `INTEGER` | `1` unless `NOT NULL` |
| `INTEGER UNSIGNED` | `INT UNSIGNED` | `INTEGER` | `1` unless `NOT NULL` |
| `BIGINT UNSIGNED` | `BIGINT UNSIGNED` | `INTEGER` | `1` unless `NOT NULL` |

If neither `NULL` nor `NOT NULL` is specified, the column is nullable, matching
MySQL's default column nullability for ordinary columns. Display width is not
accepted in this phase even though MySQL 8.4.9 still accepts it with a
deprecation warning.

Descriptor versions are initialized to `1`. All table and column rows created
by a single successful `CREATE TABLE` use the same catalog generation.

## Physical SQLite Schema

Physical tables use deterministic MyLite-owned names:

```text
_mylite_user_table_<table_id>
```

`table_id` is allocated inside the statement transaction before the catalog
table row is inserted. The generated name lives in
`_mylite_catalog_tables.physical_name` and is quoted as an SQLite identifier
when internal SQLite SQL is generated. User names cannot collide with this
namespace because `_mylite_` is reserved.

Physical SQLite columns use the unquoted MySQL logical column names quoted as
SQLite identifiers. Supported integer columns use SQLite declaration
`INTEGER`; `NOT NULL` is added only for MySQL `NOT NULL`. No SQLite primary
keys, unique constraints, defaults, checks, generated columns, or indexes are
generated in this phase.

`SHOW TABLES` reads MyLite table descriptors and returns logical table names
ordered by logical name, not by SQLite schema introspection.

## Transaction And Failure Unwinding

Each mutating DDL statement runs in one SQLite `BEGIN IMMEDIATE` transaction
owned by MyLite. Inside that transaction:

1. MyLite resolves and validates schema, table, and column descriptors.
2. `CREATE TABLE` inserts catalog table and column rows using one next catalog
   generation.
3. `CREATE TABLE` generates and executes one physical SQLite `CREATE TABLE`.
4. `DROP TABLE` deletes catalog column and table rows.
5. `DROP TABLE` generates and executes one physical SQLite `DROP TABLE`.
6. MyLite updates `_mylite_catalog_state.catalog_generation`.
7. MyLite commits.

If any step fails, MyLite rolls back the transaction and returns a diagnostic.
Rollback must undo catalog rows and physical SQLite schema changes together.
On successful commit, MyLite updates connection-local catalog generation,
session catalog generation, SQLite schema generation, and invalidates descriptor
caches.

This phase does not implement user transactions or implicit-commit boundaries.
All supported statements run as standalone MyLite statement transactions.

## Diagnostics

The public function return code indicates MyLite API status. SQL diagnostics
are stored on the database handle.

| Condition | Return | Diagnostic |
| --- | --- | --- |
| Success | `MYLITE_OK` | `0`, `00000`, `not an error` |
| API misuse | `MYLITE_MISUSE` | `MYLITE_MISUSE`, `HY000`, misuse message |
| Allocation failure | `MYLITE_NOMEM` | `MYLITE_NOMEM`, `HY001`, allocation message |
| Lexer or parser error | `MYLITE_ERROR` | `1064`, `42000`, MySQL-style syntax message |
| Unsupported parsed statement/scope | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| No selected schema | `MYLITE_ERROR` | `1046`, `3D000`, `No database selected` |
| Unknown schema | `MYLITE_ERROR` | `1049`, `42000`, `Unknown database '<schema>'` |
| Duplicate table | `MYLITE_ERROR` | `1050`, `42S01`, `Table '<table>' already exists` |
| Unknown table on drop | `MYLITE_ERROR` | `1051`, `42S02`, `Unknown table '<schema>.<table>'` |
| Duplicate column | `MYLITE_ERROR` | `1060`, `42S21`, `Duplicate column name '<column>'` |
| Reserved `_mylite_*` name | `MYLITE_ERROR` | `1103`, `42000`, `Incorrect table name '<name>'` or corresponding column/schema text |
| Physical SQLite failure | `MYLITE_ERROR` | `1105`, `HY000`, deterministic internal failure message |

Warnings are not generated by the supported subset. Unsupported MySQL syntax
that would produce MySQL warnings, such as integer display width, is rejected in
this phase instead of accepted with a warning.

## MySQL 8.4.9 Runtime Observations

The following behavior was checked on 2026-05-07 against the official
`mysql:8.4.9` Docker image with:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --force --show-warnings -vvv
```

Observed behavior used by this feature:

| SQL | MySQL 8.4.9 observation |
| --- | --- |
| `CREATE TABLE no_default_table (id INT)` without `USE` | Error `1046`, SQLSTATE `3D000`, `No database selected`. |
| `CREATE TABLE db.qualified_table (id INT NOT NULL)` when `db` exists | `Query OK, 0 rows affected`. |
| `CREATE TABLE db.missing_schema (id INT)` when `db` does not exist | Error `1049`, SQLSTATE `42000`, `Unknown database 'db'`. |
| `USE db` when `db` exists | Succeeds and makes `DATABASE()` return `db`. |
| `USE db` when `db` does not exist | Error `1049`, SQLSTATE `42000`. |
| `CREATE TABLE t (id INT, amount BIGINT NOT NULL, flags INT UNSIGNED NULL)` | `Query OK, 0 rows affected`, no warnings. |
| `INFORMATION_SCHEMA.COLUMNS` for that table | `id` is `int` nullable, `amount` is `bigint` not nullable, `flags` is `int unsigned` nullable. |
| `SHOW TABLES` after creating `a_table` and `z_table` | One column named `Tables_in_<db>` with rows `a_table`, then `z_table`. |
| `DROP TABLE t` when `t` exists | `Query OK, 0 rows affected`, no warnings. |
| `DROP TABLE t` when `t` does not exist | Error `1051`, SQLSTATE `42S02`, `Unknown table '<db>.t'`. |
| `DROP TABLE IF EXISTS t` when `t` does not exist | `Query OK, 0 rows affected, 1 warning`; warning note code `1051`. |
| `CREATE TABLE t (id INT)` when `t` exists | Error `1050`, SQLSTATE `42S01`. |
| `CREATE TABLE duplicate_column (id INT, id BIGINT)` | Error `1060`, SQLSTATE `42S21`. |
| `CREATE TABLE display_width (id INT(11))` | Accepted with warning `1681`; MyLite rejects display width in this phase. |
| `CREATE TABLE unsupported_varchar (name VARCHAR(10))` | Accepted by MySQL; MyLite rejects non-integer types in this phase. |
| `CREATE TABLE bad_table_option (id INT) ENGINE=InnoDB` | Accepted by MySQL; MyLite rejects table options in this phase. |

## Compatibility Status

This feature moves only the exact supported subset to partial support:

- base tables: persistent catalog-backed base-table descriptors and physical
  tables for this subset only;
- `CREATE TABLE`: limited persistent base-table creation with explicit integer
  columns only;
- `DROP TABLE`: limited single-table persistent base-table drop without
  `IF EXISTS`;
- column definition grammar: limited integer type and nullability descriptors;
- `SHOW TABLES`: limited descriptor-driven table listing;
- `USE`: limited selection of existing MyLite catalog schemas.

All other table DDL, schema DDL, type conversion, DML, information-schema, and
introspection behavior remains unsupported.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.basic_table_lifecycle`.

Coverage must include:

- public execution API misuse and zero-initialized result cleanup;
- parser/AST acceptance for supported `USE`, `CREATE TABLE`, `DROP TABLE`, and
  `SHOW TABLES`;
- parser rejection for unsupported `IF EXISTS`, table options, display width,
  defaults, keys, constraints, temporary tables, multi-table drop, and
  unsupported types;
- successful schema-qualified create for the supported integer subset;
- successful unqualified create after `USE` selects an existing catalog schema;
- catalog descriptor rows after create: schema/table/columns, logical type,
  physical type, nullability, descriptor versions, and generation changes;
- physical SQLite table creation inside the shifted payload;
- reopen persistence for descriptors and physical table;
- `SHOW TABLES` result rows and column name from MyLite descriptors;
- successful drop removing catalog descriptors and physical table atomically;
- duplicate table, unknown table, reserved name, duplicate column, no selected
  schema, unknown schema, unsupported type/syntax, and induced physical failure
  where practical;
- independent file-backed handles with independent lifecycle state;
- unchanged MyLite preamble bytes across DDL;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, client-data, and registration tests still pass.

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
ctest --preset dev -R '^libmylite\.runtime\.basic_table_lifecycle$' --output-on-failure
ctest --preset dev -R '^libmylite\.parser$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_basic_table_lifecycle_expectations.sh
cmake --workflow --preset check
```

Then review the diff for public ABI stability, parser independence,
statement-boundary atomicity, catalog authority, deterministic physical naming,
file-format safety, VFS preservation, zero-init safety, cleanup on failure,
scope control, compatibility-matrix accuracy, MySQL 8.4.9 evidence, and test
relevance.
