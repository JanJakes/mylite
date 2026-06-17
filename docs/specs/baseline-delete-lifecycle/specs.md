# Baseline Delete Lifecycle

## Status

This feature specifies the next narrow user-visible DML slice for file-backed
`.mylite` handles. It adds descriptor-driven single-table `DELETE` execution on
top of `mylite_execute()`, statement context, the MyLite parser scaffold,
shifted `.mylite` storage, durable catalog descriptors, create/drop/rename
table lifecycle, integer/`NULL` row values, descriptor-driven `SELECT ...
WHERE`, and descriptor-driven `SELECT ... ORDER BY ... LIMIT`.

The feature is intentionally not full MySQL `DELETE` support. It supports only
one persistent base table, the existing baseline predicate subset, one
descriptor order key, and a `LIMIT row_count` literal form that can be bound to
SQLite as a signed 64-bit integer.

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
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline select order limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `DELETE`:
  https://dev.mysql.com/doc/refman/8.4/en/delete.html
- MySQL 8.4 Reference Manual, comparison operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4 Reference Manual, `NULL` values:
  https://dev.mysql.com/doc/refman/8.4/en/null-values.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited single-table `DELETE FROM table_name`
  statement;
- unqualified and schema-qualified target table resolution using the existing
  selected-schema policy;
- one persistent MyLite base-table descriptor target only;
- optional baseline `WHERE` predicates exactly matching the supported
  descriptor-driven select predicate subset;
- optional `ORDER BY` with one unqualified descriptor column and optional
  `ASC` or `DESC`;
- optional `LIMIT row_count` using an unsigned decimal integer literal in the
  range `0` through `9223372036854775807`;
- descriptor-driven predicate and ordering column resolution;
- MyLite-owned predicate and limit literal conversion before SQLite binding;
- generated SQLite physical `DELETE` execution built only from descriptors and
  stable physical table names;
- exact affected-row reporting from the physical delete;
- result-handle behavior matching existing non-row statements;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- `LOW_PRIORITY`, `QUICK`, `IGNORE`, aliases, `PARTITION`, multi-table delete,
  joined delete, `USING`, `WITH`/CTEs, subqueries, joins, or query modifiers;
- table-qualified order columns, multiple sort keys, expression order keys,
  ordinal order keys, aliases, collations, functions, parameters, variables, or
  general expression ordering;
- `LIMIT` offset forms, signed limit literals, decimal/float/string/hex/bit
  limit literals, or unsigned 64-bit limit values above the signed 64-bit
  SQLite binding range;
- arbitrary SQLite SQL pass-through;
- triggers, cascades, foreign keys, auto-increment reset behavior, privilege
  semantics, locks, generated columns, indexes, constraints, temporary tables,
  or views;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns the top-level statement boundary: diagnostics reset,
  warning count, affected rows, backend status, and transaction completion.
  Successful deletes copy the exact physical row-change count to the public
  result.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves schemas, target tables, predicate columns, and
  ordering columns against MyLite catalog descriptors; rejects unsupported
  shapes; converts supported predicate and limit literals; and builds a
  descriptor-driven physical delete plan.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. `DELETE` does not
  mutate catalog rows, descriptor versions, generation, caches, or
  `sqlite_schema_generation`.
- The result builder owns the empty non-row result returned for supported
  deletes. Successful deletes have `column_count == 0`, `row_count == 0`,
  exact `affected_rows`, and `warning_count == 0`.
- SQLite owns physical b-tree row storage and transactional row deletion for
  generated internal SQL. SQLite schema text and `PRAGMA` output are not
  metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Deletes occur only inside the shifted SQLite payload and must not touch byte
  range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
DELETE FROM table_name [WHERE predicate] [ORDER BY order_key [ASC | DESC]] [LIMIT row_count]
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The supported predicate subset is exactly the subset from
`baseline-select-where-lifecycle`:

```sql
predicate:
    column_name comparison_operator signed_integer_literal
  | column_name IS NULL
  | column_name IS NOT NULL
  | ( predicate )
```

Supported ordering subset:

```sql
order_key:
    column_name
```

The parser may admit a qualified identifier as an order key so the analyzer can
reject it with a deterministic unsupported diagnostic. The supported semantic
subset requires one unqualified descriptor column.

Supported limit subset:

```sql
limit_clause:
    LIMIT row_count

row_count:
    unsigned_decimal_integer_literal
```

MySQL 8.4.9 rejects `DELETE ... LIMIT row_count OFFSET offset` and
`DELETE ... LIMIT offset, row_count` with syntax error `1064`; MyLite must not
admit those forms in this phase.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= delete_statement.

delete_statement ::=
    DELETE FROM table_name where_clause_opt order_clause_opt delete_limit_clause_opt.

where_clause_opt ::= .
where_clause_opt ::= WHERE predicate.

order_clause_opt ::= .
order_clause_opt ::= ORDER BY qualified_identifier order_direction_opt.

order_direction_opt ::= .
order_direction_opt ::= ASC.
order_direction_opt ::= DESC.

delete_limit_clause_opt ::= .
delete_limit_clause_opt ::= LIMIT limit_integer.

limit_integer ::= INTEGER.

table_name ::= identifier.
table_name ::= identifier DOT identifier.
```

The existing parser rule that treats a reserved word after `.` as an identifier
continues to apply.

## Schema And Table Resolution

`DELETE FROM t` uses the connection's selected schema. If no selected schema
exists, the statement fails with MySQL error `1046`, SQLSTATE `3D000`, and
message `No database selected`.

`DELETE FROM db.t` resolves `db` against existing MyLite schema descriptors
regardless of the selected schema. Unknown schemas fail with MySQL error
`1049`, SQLSTATE `42000`, and message `Unknown database '<schema>'`.

The target table descriptor must exist and must have
`kind == MYLITE_CATALOG_TABLE_KIND_BASE`. Unknown tables fail with MySQL error
`1146`, SQLSTATE `42S02`, and message
`Table '<schema>.<table>' doesn't exist`. Unsupported object kinds are rejected
with a deterministic unsupported diagnostic until views and temporary-table
descriptors are specified.

User-authored schema and table names beginning with `_mylite_`, using ASCII
case-insensitive comparison after identifier unquoting, are rejected before any
SQLite SQL is generated.

## Column Resolution

Predicate and ordering column resolution uses MyLite column descriptors loaded
in `ordinal_position` order. SQLite metadata is not consulted.

Predicate column resolution preserves the `baseline-select-where-lifecycle`
policy as later expanded by
[baseline qualified predicate columns](../baseline-qualified-predicate-columns/specs.md):

- the supported predicate column may be unqualified or matching table-,
  alias-, or schema.table-qualified;
- unknown predicate names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'where clause'`.

Ordering column resolution preserves the `baseline-select-order-limit-lifecycle`
policy:

- the supported order key is one unqualified descriptor column;
- unknown ordering names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'order clause'`;
- table-qualified order columns, multiple order keys, aliases, ordinals,
  expressions, and collation modifiers are rejected in this phase.

The current catalog stores `(table_id, name)` with SQLite binary uniqueness.
MyLite analysis compares the supported identifier subset with ASCII
case-insensitive folding, matching previous baseline column-resolution policy.

## Predicate Semantics

Supported delete predicates match the current filtered-select predicate
semantics. For comparison predicates, MyLite converts the right-hand integer
literal to the predicate column's logical integer domain before SQLite binding.
Rows are deleted when the predicate is true. Rows for which a comparison would
evaluate to SQL `NULL` are not deleted.

Supported predicate behavior:

| Predicate | Semantics in this slice |
| --- | --- |
| `col = n` | Deletes rows where non-`NULL` column values equal `n`. |
| `col <=> n` | With a non-`NULL` integer right operand, deletes the same rows as `col = n`; `NULL` column values do not match. |
| `col <> n`, `col != n` | Deletes rows where non-`NULL` column values are not equal to `n`. |
| `col < n`, `col <= n`, `col > n`, `col >= n` | Deletes rows where non-`NULL` column values satisfy the integer comparison. |
| `col IS NULL` | Deletes rows where the descriptor column is SQL `NULL`. |
| `col IS NOT NULL` | Deletes rows where the descriptor column is not SQL `NULL`. |

Predicate literal conversion and range diagnostics match the select-where
slice. Unsupported or out-of-range predicate literals fail before SQLite SQL is
prepared.

## Ordering Semantics

The admitted order column must be one of the baseline integer descriptor
families already stored as SQLite `INTEGER` or SQL `NULL`.

For supported `ORDER BY`:

- omitted direction and explicit `ASC` sort ascending;
- `DESC` sorts descending;
- SQL `NULL` values sort before non-`NULL` values for ascending order and after
  non-`NULL` values for descending order;
- duplicate sort-key values are ties. Without a second sort key, MyLite does
  not claim which tied row is deleted when `LIMIT` stops inside a tie group;
- `ORDER BY` without `LIMIT` is accepted. With no triggers, cascades, foreign
  keys, or visible per-row side effects in this slice, it has no additional
  visible row-set effect beyond MySQL acceptance.

SQLite's native ordering for `INTEGER` and `NULL` matches the supported MySQL
8.4.9 behavior for this descriptor-limited subset, so no SQLite fork hook or
custom collation is needed.

## Limit Semantics

For supported `LIMIT row_count`:

- `LIMIT 0` deletes no rows and reports `affected_rows == 0`;
- `LIMIT n` deletes at most `n` matched rows after filtering and ordering;
- a row count larger than the matched set deletes all matched rows;
- supported in-range deletes produce no warnings.

Observed MySQL 8.4.9 accepts `DELETE ... LIMIT` values up to
`18446744073709551615`. MyLite deliberately admits only
`0` through `9223372036854775807` in this phase because generated SQLite
statements bind the limit through `sqlite3_bind_int64()`. Values outside that
range fail before SQLite SQL is prepared with a deterministic MyLite-specific
syntax diagnostic. This prevents silent modulo, signed-wrap, or SQLite
`LIMIT -1` behavior from becoming part of the public contract.

Signed limit literals and non-decimal literal forms are not admitted. Observed
MySQL 8.4.9 rejects `LIMIT +1`, `LIMIT -1`, decimal, string, hex, bit, and
parameter forms outside prepared statements with syntax error `1064`, SQLSTATE
`42000`.

## Physical SQLite Handling

Generated SQL is built only from descriptors:

- target table names come from `_mylite_catalog_tables.physical_name`;
- predicate and ordering column names come from MyLite column descriptors;
- every SQLite identifier is double-quoted with embedded double quotes doubled;
- comparison predicate values and limit row counts are bound through SQLite
  prepared-statement parameters;
- generated SQL contains no interpolated user literals.

When no `ORDER BY` or `LIMIT` is present, the generated shape is:

```sql
DELETE FROM "<physical_name>"
[WHERE "<predicate_col>" <op> ?1 | WHERE "<predicate_col>" IS [NOT] NULL]
```

When `LIMIT` is present, MyLite must not rely on SQLite's optional
`DELETE ... ORDER BY ... LIMIT` syntax. It uses a descriptor-built rowid
subquery over generated MyLite user tables:

```sql
DELETE FROM "<physical_name>"
WHERE rowid IN (
    SELECT rowid
    FROM "<physical_name>"
    [WHERE "<predicate_col>" <op> ?1 | WHERE "<predicate_col>" IS [NOT] NULL]
    [ORDER BY "<order_col>" ASC|DESC]
    LIMIT ?n
)
```

Generated MyLite user tables are ordinary SQLite rowid tables in the current
baseline physical schema because `CREATE TABLE` does not generate
`WITHOUT ROWID`. The rowid use here is an internal physical storage invariant,
not MySQL-visible metadata. If future table DDL adds `WITHOUT ROWID` physical
tables or helper storage, this delete lowering must be revisited before those
descriptors can use ordered or limited delete.

SQLite exposes the hidden rowid through the unquoted aliases `rowid`,
`_rowid_`, and `oid` unless user columns shadow those names. MyLite chooses an
unshadowed alias for limited deletes. If all three aliases are shadowed by
descriptor columns, limited delete is rejected with a deterministic unsupported
diagnostic rather than risking deletion by a user column value.

`ORDER BY` without `LIMIT` may be lowered to the simple delete shape because
there is no visible row-order side effect in this slice. A future trigger,
cascade, lock, replication, or row-by-row diagnostics slice must re-evaluate
that decision.

Deletes run inside one MyLite-owned `BEGIN IMMEDIATE` transaction. Planning,
descriptor resolution, predicate conversion, ordering resolution, and limit
conversion happen before the transaction starts. On SQLite prepare, bind,
step, or commit failure, MyLite rolls back and reports a deterministic internal
row-operation diagnostic.

No catalog rows, descriptor versions, descriptor caches, catalog generation, or
`sqlite_schema_generation` change for successful or failed deletes. The
`.mylite` preamble remains untouched because row changes occur only through the
shifted SQLite payload.

## Result Behavior

Successful supported `DELETE` returns an empty DML result through the existing
public result API:

- `column_count == 0`;
- `row_count == 0`;
- `affected_rows` equals the number of rows actually deleted;
- `warning_count == 0`.

Failed deletes return no result handle and store diagnostics on `mylite_db`,
matching existing public execution conventions.

## Diagnostics

The public function return code indicates MyLite API status. SQL diagnostics
are stored on the database handle.

| Condition | Return | Diagnostic |
| --- | --- | --- |
| Success | `MYLITE_OK` | `0`, `00000`, `not an error` |
| Lexer or parser error | `MYLITE_ERROR` | `1064`, `42000`, MySQL-style syntax message |
| Unsupported delete, order, limit, or predicate shape | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| No selected schema | `MYLITE_ERROR` | `1046`, `3D000`, `No database selected` |
| Unknown schema | `MYLITE_ERROR` | `1049`, `42000`, `Unknown database '<schema>'` |
| Unknown table | `MYLITE_ERROR` | `1146`, `42S02`, `Table '<schema>.<table>' doesn't exist` |
| Reserved schema name | `MYLITE_ERROR` | `1102`, `42000`, `Incorrect database name '<name>'` |
| Reserved table name | `MYLITE_ERROR` | `1103`, `42000`, `Incorrect table name '<name>'` |
| Unknown predicate column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'where clause'` |
| Unknown ordering column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'order clause'` |
| Unsupported order expression | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| Unsupported limit expression or literal | `MYLITE_ERROR` | `1064`, `42000`, deterministic syntax or unsupported message |
| Limit outside the supported signed 64-bit binding range | `MYLITE_ERROR` | `1064`, `42000`, `LIMIT literal is outside the supported range` |
| All SQLite rowid aliases shadowed for limited delete | `MYLITE_ERROR` | `1064`, `42000`, `DELETE LIMIT requires an unshadowed SQLite rowid alias` |
| Predicate literal outside supported descriptor range | `MYLITE_ERROR` | `1264`, `22003`, `Out of range value for column '<column>' in WHERE` |
| Unsupported object kind | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| Physical SQLite failure | `MYLITE_ERROR` | `1105`, `HY000`, deterministic internal row-operation message |
| Allocation failure | `MYLITE_NOMEM` | `MYLITE_NOMEM`, `HY001`, allocation message |

No public ABI changes are planned, so existing public execution/result misuse
behavior is preserved.

## MySQL 8.4.9 Runtime Observations

The following behavior was checked on 2026-05-07 against the official
`mysql:8.4.9` Docker image with:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --force --show-warnings
```

Observed behavior used by this feature:

| SQL | MySQL 8.4.9 observation |
| --- | --- |
| `DELETE FROM numbers` | Deletes every row; `ROW_COUNT()` reports the deleted row count and `@@warning_count` is `0`. |
| `DELETE FROM numbers WHERE i < 0` | Deletes only matching rows and reports exact affected rows. |
| `DELETE FROM numbers WHERE i < -100` | Deletes no rows, reports `ROW_COUNT() == 0`, and leaves existing rows intact. |
| `DELETE FROM numbers ORDER BY i LIMIT 2` | Deletes the two rows with the lowest `i` values; no warnings. |
| `DELETE FROM numbers ORDER BY i DESC LIMIT 1` | Deletes the row with the highest `i` value; no warnings. |
| `DELETE FROM numbers ORDER BY n LIMIT 2` | Rows with `NULL` order keys sort first for ascending order. |
| `DELETE FROM numbers ORDER BY n DESC LIMIT 2` | Rows with `NULL` order keys sort last for descending order. |
| `DELETE FROM numbers ORDER BY i` | Accepted and deletes all rows; no warnings. |
| `DELETE FROM numbers LIMIT 0` | Deletes no rows and reports `ROW_COUNT() == 0`; no warnings. |
| `DELETE FROM numbers LIMIT 10` | Deletes all rows when the limit exceeds the matched row count; no warnings. |
| `DELETE FROM numbers WHERE n IS NULL ORDER BY id DESC LIMIT 1` | `WHERE`, `ORDER BY`, and `LIMIT` compose in that clause order. |
| `DELETE FROM numbers WHERE i <=> 1` | With non-`NULL` right operand, deletes the same row set as equality. |
| `DELETE FROM numbers` without a selected schema | Error `1046`, SQLSTATE `3D000`, `No database selected`. |
| `DELETE FROM db.numbers` without a selected schema | Succeeds when `db` exists. |
| Unknown unqualified or qualified table | Error `1146`, SQLSTATE `42S02`, `Table '<schema>.<table>' doesn't exist`. |
| Unknown schema | Error `1049`, SQLSTATE `42000`, `Unknown database '<schema>'`. |
| Unknown `WHERE` column | Error `1054`, SQLSTATE `42S22`, `Unknown column '<column>' in 'where clause'`. |
| Unknown `ORDER BY` column | Error `1054`, SQLSTATE `42S22`, `Unknown column '<column>' in 'order clause'`. |
| `DELETE ... LIMIT +1`, `LIMIT -1`, `LIMIT 1.0`, `LIMIT '1'`, `LIMIT 0x1`, `LIMIT b'1'`, `LIMIT ?` | Error `1064`, SQLSTATE `42000`. |
| `DELETE ... LIMIT 1 OFFSET 1` and `DELETE ... LIMIT 1, 1` | Error `1064`, SQLSTATE `42000`. |
| `DELETE ... LIMIT 9223372036854775807` | Accepted as a large row count. |
| `DELETE ... LIMIT 9223372036854775808` and `LIMIT 18446744073709551615` | Accepted by MySQL; intentionally rejected by MyLite in this phase because the value is outside SQLite signed-64 binding range. |
| `DELETE ... LIMIT 18446744073709551616` | Error `1064`, SQLSTATE `42000`. |
| Table aliases, `LOW_PRIORITY`, `QUICK`, `IGNORE`, table-qualified order columns, expression order keys, and multiple order keys | Accepted by MySQL for applicable ordinary single-table deletes; intentionally rejected by MyLite in this phase. |
| `ORDER BY 1` | MySQL reports unknown column `1` in `order clause`; MyLite rejects ordinal order keys in this phase before execution. |

The reproducible probe script for this phase is
`packages/libmylite/tests/mysql_baseline_delete_lifecycle_expectations.sh`.

## Compatibility Status

This feature moves only the exact supported subset to partial support after
implementation:

- `DELETE` single-table: persistent base tables only, with optional baseline
  `WHERE`, one supported `ORDER BY` key, and `LIMIT row_count`;
- `WHERE`: existing descriptor-driven integer/`NULL` predicate subset reused
  for delete targets;
- `ORDER BY`: one unqualified descriptor integer/`NULL` column only, with
  optional `ASC` or `DESC`;
- `LIMIT`: one unsigned decimal row-count literal in signed 64-bit range only.

Full `DELETE`, aliases, partitions, multi-table and joined deletes, `USING`,
modifiers, full `ORDER BY`, full `LIMIT/OFFSET`, expression ordering, ordinal
ordering, collations, triggers, cascades, foreign keys, privilege semantics,
protocol-grade metadata, and arbitrary SQLite pass-through remain unsupported.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.delete_lifecycle`.

Coverage must include:

- parser/AST acceptance for `DELETE FROM table`, optional `WHERE`, optional
  `ORDER BY`, `ASC`, `DESC`, and `LIMIT row_count`;
- successful full-table delete, no-match delete, and filtered deletes over
  `INT`, `INTEGER`, `BIGINT`, `INT UNSIGNED`, `INTEGER UNSIGNED`, and
  `BIGINT UNSIGNED` values in the supported physical range;
- `WHERE` predicate reuse, including comparisons, `<=>` with non-`NULL`
  integer right operands, `IS NULL`, and `IS NOT NULL`;
- `ORDER BY` plus `LIMIT` deletes for default direction, `ASC`, `DESC`,
  nullable integer columns, `NULL` ordering, and duplicate sort-key ties
  without overclaiming which tied row is deleted when the limit stops inside a
  tie group;
- `ORDER BY` without `LIMIT`, `LIMIT 0`, exact row counts, row counts larger
  than the matched set, maximum supported row count, unsupported offset forms,
  signed limit forms, non-decimal limit forms, and out-of-range limit values;
- schema-qualified and unqualified target resolution, no selected schema,
  unknown schema, unknown table, and reserved `_mylite_*` names;
- affected-row count, warning count, absence of result rows, and remaining rows
  after each delete;
- unknown predicate columns and unknown ordering columns;
- unsupported delete syntax rejected deterministically:
  table-qualified order columns, expression order keys, ordinal order keys,
  multiple sort keys, string/decimal/float/hex/bit limit literals, parameters,
  functions, joins, multi-table delete, `USING`, `PARTITION`, `LOW_PRIORITY`,
  `QUICK`, `IGNORE`, subqueries, CTEs, and query modifiers;
- reopen persistence: inserted rows survive close/reopen, deletes persist after
  close/reopen, and sorted/limited selects observe remaining rows;
- delete after table rename and after drop;
- physical SQLite payload behavior without touching the MyLite preamble;
- independent file-backed handles with independent deleted row state;
- zero-initialized cleanup for new planner/result objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, basic create/drop lifecycle, table rename lifecycle, row values
  lifecycle, select-where lifecycle, select-order-limit lifecycle, client-data,
  and registration tests still pass.

## Build Integration

Add any new runtime/analyzer/planner/catalog SQL execution sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.delete_lifecycle$' --output-on-failure
ctest --preset dev -R '^libmylite\.(parser|runtime\.basic_table_lifecycle|runtime\.table_rename_lifecycle|runtime\.row_values_lifecycle|runtime\.select_where_lifecycle|runtime\.select_order_limit_lifecycle)$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_delete_lifecycle_expectations.sh
cmake --workflow --preset check
```

Then review the final diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, descriptor-driven physical deletion, integer conversion correctness
for the admitted predicate/limit subset, `NULL` ordering correctness, exact
affected-row semantics, file-format safety, VFS preservation, zero-init safety,
cleanup on failure, scope control, compatibility-matrix accuracy, and test
relevance.
