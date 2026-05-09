# Baseline Small Integer Types

## Status

This feature specifies the next narrow type-system slice for file-backed
`.mylite` handles. It extends the existing descriptor-backed integer pipeline
from `INT` / `INTEGER` and `BIGINT` to include `TINYINT`, `SMALLINT`, and
`MEDIUMINT`, each optionally `UNSIGNED`.

The feature intentionally does not add general expression evaluation, protocol
metadata, display widths, `ZEROFILL`, `SIGNED`, `BOOL` / `BOOLEAN`, aliases such
as `INT1`, or storage optimizations. It keeps the current physical SQLite
`INTEGER` row storage and adds MyLite-owned logical descriptors, rendering, and
range conversion for the new families.

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
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline select order limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline delete lifecycle:
  `docs/specs/baseline-delete-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline alter table add/drop/rename/modify/change column specs under
  `docs/specs/`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, integer type ranges:
  https://dev.mysql.com/doc/refman/8.4/en/integer-types.html
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for `TINYINT`, `SMALLINT`, and `MEDIUMINT`;
- optional `UNSIGNED` support for those three type names in the same grammar
  positions that currently accept `INT`, `INTEGER`, and `BIGINT`;
- logical catalog descriptor text for the exact spelling family selected by
  the parser: `TINYINT`, `TINYINT UNSIGNED`, `SMALLINT`,
  `SMALLINT UNSIGNED`, `MEDIUMINT`, and `MEDIUMINT UNSIGNED`;
- continued physical SQLite storage as `INTEGER`;
- descriptor-driven `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`,
  `ALTER TABLE ... MODIFY [COLUMN]`, and
  `ALTER TABLE ... CHANGE [COLUMN]` support for the new integer families;
- `SHOW COLUMNS`, `DESCRIBE` / `EXPLAIN table`, and `SHOW CREATE TABLE`
  rendering that matches MySQL's lower-case type text for the admitted subset;
- strict assignment conversion for `INSERT ... VALUES`, `INSERT ... SET`, and
  single-table `UPDATE` values;
- descriptor-driven predicate conversion for supported `SELECT`, `DELETE`, and
  `UPDATE` `WHERE` predicates;
- descriptor-driven single-column `ORDER BY` support for reads and ordered
  limited deletes/updates using the existing SQLite ordering path;
- existing-row range validation when `MODIFY` or `CHANGE` narrows a descriptor
  to one of the new families;
- all existing row-value, select, delete, update, alter, show, reopen, file
  format, and independent-handle invariants for the new families;
- MySQL 8.4.9 expectation coverage for supported behavior and explicitly
  deferred syntax.

## Non-Goals

This feature must not implement:

- display width syntax such as `TINYINT(3)`;
- `SIGNED` syntax, even though MySQL accepts it as a no-op for integer types;
- `ZEROFILL`, including its automatic unsigned implication;
- `BOOL`, `BOOLEAN`, `SERIAL`, `SERIAL DEFAULT VALUE`, `INT1`, `INT2`,
  `INT3`, `INT4`, `INT8`, or other vendor-type aliases;
- `BIT`, `DECIMAL`, `NUMERIC`, `FLOAT`, `DOUBLE`, `REAL`, string, temporal,
  blob, enum, set, json, or spatial types;
- expression arithmetic, truth-value semantics, casts, warnings-as-clipping
  behavior outside the current strict baseline, or non-strict SQL mode effects;
- protocol-grade type metadata, field flags, lengths, charsets, decimals, or
  wire-protocol column definitions;
- physical compact storage for smaller integers, SQLite column type changes, or
  SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and failure cleanup.
- Statement context owns per-statement diagnostics, warning count, affected
  rows, and transaction completion. This feature does not add new public
  statement state.
- Lexer/parser/AST own syntax admission and source spans for the new type
  keywords. They stay independent of catalog, runtime, storage, and SQLite.
- Analyzer/planner code maps AST integer families to durable logical
  descriptors, resolves table/column names against MyLite descriptors, and
  performs MyLite-owned range conversion before SQLite binding.
- The catalog module continues to store logical type text and physical type
  text. It does not infer type semantics from SQLite schema text.
- Result/introspection builders render logical descriptors to MySQL-shaped text
  for the supported subset. SQLite metadata is not user-visible authority.
- SQLite owns physical b-tree row storage, scans, sorting, and row mutation for
  generated prepared statements. All admitted integer families still bind as
  SQLite signed 64-bit integers.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not touch byte range `[0, 4096)`.

## Supported SQL Grammar

This feature expands the existing `integer_type` nonterminal used by current
table and column lifecycle statements. It admits only bare type names plus
optional `UNSIGNED`.

Supported type forms:

```sql
TINYINT
TINYINT UNSIGNED
SMALLINT
SMALLINT UNSIGNED
MEDIUMINT
MEDIUMINT UNSIGNED
```

The existing supported forms remain unchanged:

```sql
INT
INT UNSIGNED
INTEGER
INTEGER UNSIGNED
BIGINT
BIGINT UNSIGNED
```

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
integer_type ::= TINYINT.
integer_type ::= SMALLINT.
integer_type ::= MEDIUMINT.
integer_type ::= INT.
integer_type ::= INTEGER.
integer_type ::= BIGINT.

integer_type ::= TINYINT UNSIGNED.
integer_type ::= SMALLINT UNSIGNED.
integer_type ::= MEDIUMINT UNSIGNED.
integer_type ::= INT UNSIGNED.
integer_type ::= INTEGER UNSIGNED.
integer_type ::= BIGINT UNSIGNED.
```

The parser must continue to reject display width, `SIGNED`, `ZEROFILL`, and
alias forms in this phase. Rejection may be a syntax error when the grammar does
not admit the token sequence, or a deterministic unsupported diagnostic if the
parser already accepts enough structure to reach the analyzer.

## Logical Type Semantics

All six new logical descriptors use SQLite `INTEGER` as the physical type. The
logical descriptor remains authoritative for MySQL compatibility.

| Logical descriptor | Supported value range |
| --- | --- |
| `TINYINT` | `-128` through `127` |
| `TINYINT UNSIGNED` | `0` through `255` |
| `SMALLINT` | `-32768` through `32767` |
| `SMALLINT UNSIGNED` | `0` through `65535` |
| `MEDIUMINT` | `-8388608` through `8388607` |
| `MEDIUMINT UNSIGNED` | `0` through `16777215` |
| `INT` / `INTEGER` | `-2147483648` through `2147483647` |
| `INT UNSIGNED` / `INTEGER UNSIGNED` | `0` through `4294967295` |
| `BIGINT` | `-9223372036854775808` through `9223372036854775807` |
| `BIGINT UNSIGNED` | `0` through `9223372036854775807` in this baseline |

MySQL supports `BIGINT UNSIGNED` above `9223372036854775807`, but MyLite's
current physical row storage uses SQLite signed 64-bit integer bindings. This
feature does not change that cap.

`NULL` handling remains unchanged. Assigning `NULL` to a nullable column stores
`NULL`; assigning `NULL` to `NOT NULL` fails with the current MySQL-shaped
diagnostic.

## Runtime Semantics

`CREATE TABLE` and `ALTER TABLE ... ADD [COLUMN]` create descriptor columns
with the logical type text listed above and physical type `INTEGER`. Existing
rows for nullable added columns receive `NULL`; existing rows for `NOT NULL`
added columns receive the current integer zero backfill, which is in range for
all new families.

`ALTER TABLE ... MODIFY [COLUMN]` and `ALTER TABLE ... CHANGE [COLUMN]`
replace the descriptor type and nullability using the existing single-action
paths. When the replacement type differs or nullability tightens, MyLite must
validate every existing non-`NULL` value against the replacement range before
committing descriptor and physical-table rebuild work. Out-of-range existing
values fail with MySQL error `1264`, SQLSTATE `22003`, and a message naming the
replacement column and first failing row number, preserving the original table.

`INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` assignment conversion use
the descriptor range before binding to SQLite. Supported in-range assignments
return `warning_count == 0`. Out-of-range assignments fail with `1264` /
`22003`, name the target column, and do not partially mutate rows.

Supported `SELECT`, `DELETE`, and `UPDATE` predicates reuse the current
descriptor-driven integer predicate path. Right-hand decimal integer literals
must be in range for the predicate column in MyLite's current baseline even
though MySQL 8.4.9 accepts wider comparison literals; out-of-range predicate
literals fail deterministically with the current MyLite predicate diagnostic for
the statement family. No expression predicates are added.

Ordering reuses SQLite integer ordering over the physical column. Since all
new families fit inside SQLite's signed integer range, default, `ASC`, `DESC`,
duplicate-key, and `NULL` placement behavior remains the same as the existing
select/delete/update order-limit slices.

## Introspection

`SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` render the admitted logical
types in MySQL-style lower-case text:

| Logical descriptor | Rendered type |
| --- | --- |
| `TINYINT` | `tinyint` |
| `TINYINT UNSIGNED` | `tinyint unsigned` |
| `SMALLINT` | `smallint` |
| `SMALLINT UNSIGNED` | `smallint unsigned` |
| `MEDIUMINT` | `mediumint` |
| `MEDIUMINT UNSIGNED` | `mediumint unsigned` |

`SHOW CREATE TABLE` renders the same lower-case type text inside the generated
statement. It must continue to use MyLite descriptors rather than SQLite schema
text.

This feature does not update protocol metadata, field length, numeric flags, or
information schema rows beyond the currently implemented descriptor-backed
introspection surfaces.

## Generated SQLite Handling

No SQLite fork patch is required. The implementation uses MyLite
wrapper/translation code plus public SQLite prepared statements. Generated
SQLite SQL continues to:

- quote physical table and column identifiers;
- use stable physical table names such as `_mylite_user_table_<table_id>`;
- bind integer and `NULL` values as parameters;
- store all admitted integer families in physical SQLite `INTEGER` columns;
- rely on SQLite for physical scans, filtering, sorting, row updates, deletes,
  inserts, rebuild-copy operations, and transaction durability.

The implementation must not add indexes, compact integer storage, custom SQLite
type affinity, or fork hooks for this phase.

## Diagnostics

The implementation must preserve existing diagnostics for public API misuse,
syntax errors, missing selected schema, unknown schema, unknown table, reserved
names, unsupported object kinds, duplicate table names, duplicate column names,
unknown columns, unsupported grammar, allocation failure, and physical SQLite
failures.

New or expanded diagnostics:

- out-of-range insert, insert-set, and update assignments fail with MySQL error
  `1264`, SQLSTATE `22003`, and `Out of range value for column '<column>' at
  row <n>`;
- out-of-range existing rows during `MODIFY` / `CHANGE` fail with MySQL error
  `1264`, SQLSTATE `22003`, and the replacement column name;
- out-of-range predicate literals continue to use the existing deterministic
  predicate diagnostics for the relevant statement family;
- unsupported display width, `SIGNED`, `ZEROFILL`, alias, or non-integer type
  forms remain rejected and must not be documented as supported.

## MySQL 8.4.9 Runtime Expectations

The feature includes `packages/libmylite/tests/mysql_baseline_small_integer_types_expectations.sh`.
It verifies against a real MySQL 8.4.9 runtime:

- `@@sql_mode` includes MySQL 8.4.9's strict default;
- successful `CREATE TABLE` with all signed and unsigned small integer families;
- boundary inserts for every new family;
- `SHOW COLUMNS` and `SHOW CREATE TABLE` type rendering;
- assignment out-of-range errors for signed and unsigned boundaries;
- `UPDATE` range checking and changed-row count for the new families;
- predicate and ordering behavior over new integer families;
- MySQL acceptance of comparison literals outside the assignment range, which
  MyLite continues to reject as a documented baseline limitation;
- `ALTER TABLE ... MODIFY` and `CHANGE` replacement into and out of the new
  families, including range validation failures;
- MySQL acceptance of deferred `SIGNED`, display-width, `ZEROFILL`,
  `BOOL` / `BOOLEAN`, and vendor alias forms so MyLite's narrower scope is
  explicit.

If a local MySQL 8.4.9 runtime is unavailable, implementation is blocked.

## Test Plan

Add or extend fast C tests under `packages/libmylite/tests/` without adding a
new framework. Prefer extending existing lifecycle tests when that keeps the
coverage close to the behavior:

- parser tests for each new type keyword and unsigned variant in `CREATE TABLE`
  and `ALTER TABLE` column definitions;
- lexer/parser regression tests that unsupported width, `SIGNED`, `ZEROFILL`,
  and alias forms remain rejected;
- runtime create/table lifecycle tests for descriptors and physical `INTEGER`
  storage;
- row-values and insert-set tests for boundary inserts, `NULL`, not-null
  behavior, out-of-range assignments, atomic failure, persistence, and
  independent handles;
- select-where, select-order-limit, delete, and update tests proving predicate,
  ordering, limit, and assignment conversion reuse the new descriptor ranges;
- alter add/drop/rename/modify/change column tests proving descriptors,
  introspection, existing-row validation, rebuild safety, row preservation, and
  rollback on range failure;
- show columns, explain table, and show create table tests for type text;
- file-format and reopen checks through existing lifecycle coverage;
- existing lexer, parser, catalog, row values, select, delete, update, alter,
  show, VFS, file-format, and full check workflow regression coverage.

## Compatibility Documentation

Implementation must update:

- `COMPATIBILITY.md` rows for `TINYINT`, `SMALLINT`, `MEDIUMINT`, integer DDL
  descriptors, table DML, predicate/order surfaces, and type conversion;
- `docs/compatibility/type-system-literals-conversion.md` for the exact new
  limited support;
- table DDL/DML/query-expression docs only where their wording currently names
  the integer families explicitly.

Documentation must not overclaim display widths, `SIGNED`, `ZEROFILL`,
`BOOL` / `BOOLEAN`, vendor aliases, protocol metadata, compact storage, or
general expression semantics.
