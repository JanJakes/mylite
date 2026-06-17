# Baseline Update Lifecycle

## Status

This feature specifies the next narrow user-visible DML slice for file-backed
`.mylite` handles. It adds descriptor-driven single-table `UPDATE` execution on
top of `mylite_execute()`, statement context, the MyLite parser scaffold,
shifted `.mylite` storage, durable catalog descriptors, create/drop/rename
table lifecycle, integer/`NULL` row values, descriptor-driven `SELECT ...
WHERE`, descriptor-driven `SELECT ... ORDER BY ... LIMIT`, and descriptor-
driven single-table `DELETE`.

The feature is intentionally not full MySQL `UPDATE` support. It supports only
one persistent base table, one assignment to one descriptor column, the existing
baseline predicate subset, one descriptor order key, and a `LIMIT row_count`
literal form that can be bound to SQLite as a signed 64-bit integer.

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
- Baseline delete lifecycle:
  `docs/specs/baseline-delete-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `UPDATE`:
  https://dev.mysql.com/doc/refman/8.4/en/update.html
- MySQL 8.4 Reference Manual, integer type ranges:
  https://dev.mysql.com/doc/refman/8.4/en/integer-types.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, sorting rows:
  https://dev.mysql.com/doc/refman/8.4/en/sorting-rows.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited single-table
  `UPDATE table_name SET assignment` statement;
- one assignment only, with analyzer rejection if more than one parsed
  assignment is present;
- unqualified and schema-qualified target table resolution using the existing
  selected-schema policy;
- one persistent MyLite base-table descriptor target only;
- assignment values limited to supported decimal integer literals with optional
  unary sign and `NULL`;
- optional baseline `WHERE` predicates exactly matching the supported
  descriptor-driven select/delete predicate subset;
- optional `ORDER BY` with one unqualified descriptor column and optional
  `ASC` or `DESC`;
- optional `LIMIT row_count` using an unsigned decimal integer literal in the
  range `0` through `9223372036854775807`;
- descriptor-driven assignment, predicate, and ordering column resolution;
- MyLite-owned assignment, predicate, and limit literal conversion before
  SQLite binding;
- generated SQLite physical `UPDATE` execution built only from descriptors and
  stable physical table names;
- MySQL-compatible changed-row affected-row reporting for the supported subset;
- result-handle behavior matching existing non-row statements;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- `LOW_PRIORITY`, `IGNORE`, aliases, `PARTITION`, multi-table update, joined
  update, CTEs, subqueries, joins, query modifiers, or arbitrary SQLite SQL
  pass-through;
- table-qualified assignment targets, table-qualified order columns, multiple
  assignments, multiple sort keys, expression order keys, ordinal order keys,
  aliases, collations, functions, parameters, variables, or general expression
  ordering;
- expression assignments, column-to-column assignments, arithmetic assignments,
  `DEFAULT` assignments, string/decimal/float/hex/bit assignment or limit
  literals, or unsigned 64-bit values above the signed 64-bit SQLite binding
  range;
- `LIMIT` offset forms, signed limit literals, non-decimal limit literals, or
  prepared-statement parameter limit forms;
- triggers, cascades, foreign keys, generated columns, defaults, check
  constraints, auto-increment behavior, changed-column protocol metadata,
  privilege semantics, locks, temporary tables, views, indexes, constraints, or
  SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns the top-level statement boundary: diagnostics reset,
  warning count, affected rows, backend status, and transaction completion.
  Successful updates copy the exact changed-row count to the public result.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves schemas, target tables, assignment columns,
  predicate columns, and ordering columns against MyLite catalog descriptors;
  rejects unsupported shapes; converts supported assignment, predicate, and
  limit literals; and builds a descriptor-driven physical update plan.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. `UPDATE` does not
  mutate catalog rows, descriptor versions, generation, caches, or
  `sqlite_schema_generation`.
- The result builder owns the empty non-row result returned for supported
  updates. Successful updates have `column_count == 0`, `row_count == 0`, exact
  changed-row `affected_rows`, and `warning_count == 0`.
- SQLite owns physical b-tree row storage and transactional row mutation for
  generated internal SQL. SQLite schema text and `sqlite_master` output are not
  metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Updates occur only inside the shifted SQLite payload and must not touch byte
  range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
UPDATE table_name
SET assignment[, assignment ...]
[WHERE predicate]
[ORDER BY order_key [ASC | DESC]]
[LIMIT row_count]

assignment:
    assignment_target = update_value

assignment_target:
    column_name
  | table_name.column_name

update_value:
    integer_literal
  | + integer_literal
  | - integer_literal
  | NULL
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The parser may admit multiple assignments and qualified assignment targets so
the analyzer can reject them with deterministic unsupported diagnostics. The
supported semantic subset requires exactly one unqualified descriptor column
assignment target.

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

MySQL 8.4.9 rejects `UPDATE ... LIMIT row_count OFFSET offset` and
`UPDATE ... LIMIT offset, row_count` with syntax error `1064`; MyLite must not
admit those forms in this phase.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= update_statement.

update_statement ::=
    UPDATE table_name SET update_assignment_list
    where_clause_opt order_clause_opt update_limit_clause_opt.

update_assignment_list ::= update_assignment.
update_assignment_list ::= update_assignment_list COMMA update_assignment.

update_assignment ::= qualified_identifier EQUAL update_value.

update_value ::= INTEGER.
update_value ::= PLUS INTEGER.
update_value ::= MINUS INTEGER.
update_value ::= NULL.

where_clause_opt ::= .
where_clause_opt ::= WHERE predicate.

order_clause_opt ::= .
order_clause_opt ::= ORDER BY qualified_identifier order_direction_opt.

order_direction_opt ::= .
order_direction_opt ::= ASC.
order_direction_opt ::= DESC.

update_limit_clause_opt ::= .
update_limit_clause_opt ::= LIMIT limit_integer.

limit_integer ::= INTEGER.

table_name ::= identifier.
table_name ::= identifier DOT identifier.
```

The existing parser rule that treats a reserved word after `.` as an identifier
continues to apply.

## Schema And Table Resolution

`UPDATE t SET ...` uses the connection's selected schema. If no selected schema
exists, the statement fails with MySQL error `1046`, SQLSTATE `3D000`, and
message `No database selected`.

`UPDATE db.t SET ...` resolves `db` against existing MyLite schema descriptors
regardless of selected schema. Unknown schemas fail with MySQL error `1049`,
SQLSTATE `42000`, and message `Unknown database '<schema>'`.

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

Assignment, predicate, and ordering column resolution uses MyLite column
descriptors loaded in `ordinal_position` order. SQLite metadata is not
consulted.

Assignment column resolution:

- the supported assignment target must be one unqualified descriptor column;
- unknown assignment names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'field list'`;
- table-qualified assignment targets are rejected with a deterministic
  unsupported diagnostic;
- more than one assignment is rejected with a deterministic unsupported
  diagnostic.

Predicate column resolution preserves the `baseline-select-where-lifecycle`
policy as later expanded by
[baseline qualified predicate columns](../baseline-qualified-predicate-columns/specs.md):

- the supported predicate column may be unqualified or matching table- or
  schema.table-qualified;
- unknown predicate names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'where clause'`.

Ordering column resolution preserves the
`baseline-select-order-limit-lifecycle` policy:

- the supported order key is one unqualified descriptor column;
- unknown ordering names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'order clause'`;
- table-qualified order columns, multiple order keys, aliases, ordinals,
  expressions, and collation modifiers are rejected in this phase.

The current catalog stores `(table_id, name)` with SQLite binary uniqueness.
MyLite analysis compares the supported identifier subset with ASCII
case-insensitive folding, matching previous baseline column-resolution policy.

## Assignment Conversion And Nullability

Supported assignment syntax is validated by MyLite during planning. Range and
nullability conversion for the assignment value is deferred until execution has
proved that the supported `WHERE`/`ORDER BY`/`LIMIT` target selection contains
at least one row. This matches MySQL's observed behavior for constant
assignments: invalid assignment values do not fail when `WHERE` matches no rows
or `LIMIT 0` selects no rows.

When at least one row is selected for update, supported assignment values are
converted by MyLite before SQLite binding:

- `NULL` becomes a SQL NULL binding;
- an unsigned magnitude is parsed from the integer token without locale;
- optional unary `+` keeps the value positive;
- optional unary `-` negates the magnitude when the target type permits it;
- unsupported literal or expression nodes fail before SQLite execution.

The supported descriptor type ranges are:

| Logical type | Supported assignment range in this phase |
| --- | --- |
| `INT` / `INTEGER` | `-2147483648` through `2147483647` |
| `INT UNSIGNED` / `INTEGER UNSIGNED` | `0` through `4294967295` |
| `BIGINT` | `-9223372036854775808` through `9223372036854775807` |
| `BIGINT UNSIGNED` | `0` through `9223372036854775807` |

MySQL supports `BIGINT UNSIGNED` assignments up to `18446744073709551615`, but
the current physical encoding stores user values in SQLite `INTEGER`, whose
public binding API is signed 64-bit. This phase therefore explicitly does not
support `BIGINT UNSIGNED` values above `9223372036854775807`; such assignments
fail with MySQL error `1264`, SQLSTATE `22003`, and message
`Out of range value for column '<column>' at row 1`.

This phase follows MySQL's strict-mode transactional behavior for the admitted
integer subset: out-of-range assignments fail only when at least one row is
selected for update, produce no supported result handle, and leave target rows
unchanged. If no row is selected for update, the statement succeeds with
`affected_rows == 0`, `warning_count == 0`, and unchanged rows.

Assigning `NULL` to a nullable descriptor column succeeds. If matching rows
already contain `NULL`, those rows are not changed and do not contribute to
`affected_rows`. Assigning `NULL` to a `NOT NULL` descriptor column fails with
MySQL error `1048`, SQLSTATE `23000`, and message `Column '<column>' cannot be
null` only when at least one row is selected for update. If no row is selected,
the statement succeeds with no warnings and no changed rows.

## Predicate Semantics

Supported update predicates match the current filtered-select and delete
predicate semantics. For comparison predicates, MyLite converts the right-hand
integer literal to the predicate column's logical integer domain before SQLite
binding. Rows are matched when the predicate is true. Rows for which a
comparison would evaluate to SQL `NULL` are not matched.

Supported predicate behavior:

| Predicate | Semantics in this slice |
| --- | --- |
| `col = n` | Matches rows where non-`NULL` column values equal `n`. |
| `col <=> n` | With a non-`NULL` integer right operand, matches the same rows as equality; `NULL` column values do not match. |
| `col <> n`, `col != n` | Matches rows where non-`NULL` column values are not equal to `n`. |
| `col < n`, `col <= n`, `col > n`, `col >= n` | Matches rows where non-`NULL` column values satisfy the integer comparison. |
| `col IS NULL` | Matches rows where the descriptor column is SQL `NULL`. |
| `col IS NOT NULL` | Matches rows where the descriptor column is not SQL `NULL`. |

Predicate literal conversion and range diagnostics match the select-where and
delete slices. Unsupported or out-of-range predicate literals fail before
SQLite SQL is prepared.

## Ordering Semantics

The admitted order column must be one of the baseline integer descriptor
families already stored as SQLite `INTEGER` or SQL `NULL`.

For supported `ORDER BY`:

- omitted direction and explicit `ASC` sort ascending;
- `DESC` sorts descending;
- SQL `NULL` values sort before non-`NULL` values for ascending order and after
  non-`NULL` values for descending order;
- duplicate sort-key values are ties. Without a second sort key, MyLite does
  not claim which tied rows are updated when `LIMIT` stops inside a tie group;
- `ORDER BY` without `LIMIT` is accepted. With no triggers, cascades, foreign
  keys, unique-key conflicts, or visible per-row side effects in this slice, it
  has no additional visible row-set effect beyond MySQL acceptance.

SQLite's native ordering for `INTEGER` and `NULL` matches the supported MySQL
8.4.9 behavior for this descriptor-limited subset, so no SQLite fork hook or
custom collation is needed.

## Limit Semantics

For supported `LIMIT row_count`:

- `LIMIT 0` updates no rows and reports `affected_rows == 0`;
- `LIMIT n` restricts the matched row set to at most `n` rows after filtering
  and ordering;
- a row count larger than the matched set considers all matched rows;
- the `LIMIT` row count is a rows-matched restriction, not a changed-row
  target. If the limited matched set contains rows that already have the
  assigned value, those no-op rows still consume the limit and do not
  contribute to `affected_rows`;
- supported in-range updates produce no warnings.

Observed MySQL 8.4.9 accepts `UPDATE ... LIMIT` values up to
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
- assignment, predicate, and ordering column names come from MyLite column
  descriptors;
- every SQLite identifier is double-quoted with embedded double quotes doubled;
- assignment values, comparison predicate values, and limit row counts are
  bound through SQLite prepared-statement parameters;
- generated SQL contains no interpolated user literals.

Before converting range-checked or nullability-checked assignment values, MyLite
executes an internal descriptor-built existence probe inside the update
transaction:

```sql
SELECT 1
FROM "<physical_name>"
[WHERE <predicate>]
LIMIT 1
```

`LIMIT 0` short-circuits this probe to "no selected rows." This probe is used
only to decide whether assignment conversion can be skipped for zero-selected
updates; it does not report matched rows publicly.

When no `LIMIT` is present, MyLite may lower to a descriptor-built ordinary
update with an additional changed-row predicate:

```sql
UPDATE "<physical_name>"
SET "<assignment_col>" = ?1
[WHERE <predicate> AND] <changed_condition>
```

The changed condition is generated from the assignment value:

```sql
"<assignment_col>" IS NOT NULL
```

for a `NULL` assignment, or:

```sql
("<assignment_col>" IS NULL OR "<assignment_col>" <> ?n)
```

for an integer assignment. This keeps SQLite's `sqlite3_changes64()` aligned
with MySQL's changed-row affected-row behavior for no-op assignments.

When `LIMIT` is present, MyLite must not rely on SQLite's optional
`UPDATE ... ORDER BY ... LIMIT` syntax. It uses a descriptor-built rowid
subquery over generated MyLite user tables, and applies the changed-row
predicate outside the rowid subquery so `LIMIT` remains a matched-row
restriction:

```sql
UPDATE "<physical_name>"
SET "<assignment_col>" = ?1
WHERE rowid IN (
    SELECT rowid
    FROM "<physical_name>"
    [WHERE "<predicate_col>" <op> ?2 | WHERE "<predicate_col>" IS [NOT] NULL]
    [ORDER BY "<order_col>" ASC|DESC]
    LIMIT ?n
)
AND <changed_condition>
```

Generated MyLite user tables are ordinary SQLite rowid tables in the current
baseline physical schema because `CREATE TABLE` does not generate
`WITHOUT ROWID`. The rowid use here is an internal physical storage invariant,
not MySQL-visible metadata. If future table DDL adds `WITHOUT ROWID` physical
tables or helper storage, this update lowering must be revisited before those
descriptors can use ordered or limited update.

SQLite exposes the hidden rowid through the unquoted aliases `rowid`,
`_rowid_`, and `oid` unless user columns shadow those names. MyLite chooses an
unshadowed alias for limited updates. If all three aliases are shadowed by
descriptor columns, limited update is rejected with a deterministic unsupported
diagnostic rather than risking mutation by a user column value.

`ORDER BY` without `LIMIT` may be lowered without an ordered rowid subquery
because there is no visible row-order side effect in this slice. A future
trigger, cascade, unique-key, lock, replication, or row-by-row diagnostics
slice must re-evaluate that decision.

Updates run inside one MyLite-owned `BEGIN IMMEDIATE` transaction. Planning,
descriptor resolution, assignment conversion, predicate conversion, ordering
resolution, and limit conversion happen before the transaction starts. On
SQLite prepare, bind, step, or commit failure, MyLite rolls back and reports a
deterministic internal row-operation diagnostic.

No catalog rows, descriptor versions, descriptor caches, catalog generation, or
`sqlite_schema_generation` change for successful or failed updates. The
`.mylite` preamble remains untouched because row changes occur only through the
shifted SQLite payload.

## Result Behavior

Successful supported `UPDATE` returns an empty DML result through the existing
public result API:

- `column_count == 0`;
- `row_count == 0`;
- `affected_rows` equals the number of rows actually changed;
- `warning_count == 0`.

Failed updates return no result handle and store diagnostics on `mylite_db`,
matching existing public execution conventions.

## Diagnostics

The public function return code indicates MyLite API status. SQL diagnostics
are stored on the database handle.

| Condition | Return | Diagnostic |
| --- | --- | --- |
| Success | `MYLITE_OK` | `0`, `00000`, `not an error` |
| Lexer or parser error | `MYLITE_ERROR` | `1064`, `42000`, MySQL-style syntax message |
| Unsupported update, assignment, order, limit, or predicate shape | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| More than one assignment | `MYLITE_ERROR` | `1064`, `42000`, `UPDATE supports exactly one assignment` |
| No selected schema | `MYLITE_ERROR` | `1046`, `3D000`, `No database selected` |
| Unknown schema | `MYLITE_ERROR` | `1049`, `42000`, `Unknown database '<schema>'` |
| Unknown table | `MYLITE_ERROR` | `1146`, `42S02`, `Table '<schema>.<table>' doesn't exist` |
| Reserved schema name | `MYLITE_ERROR` | `1102`, `42000`, `Incorrect database name '<name>'` |
| Reserved table name | `MYLITE_ERROR` | `1103`, `42000`, `Incorrect table name '<name>'` |
| Unknown assignment column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'field list'` |
| Unknown predicate column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'where clause'` |
| Unknown ordering column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'order clause'` |
| Unsupported assignment target | `MYLITE_ERROR` | `1064`, `42000`, `UPDATE supports only unqualified assignment columns` |
| Unsupported assignment value | `MYLITE_ERROR` | `1064`, `42000`, `UPDATE supports only integer and NULL assignment values` |
| Unsupported order expression | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| Unsupported limit expression or literal | `MYLITE_ERROR` | `1064`, `42000`, deterministic syntax or unsupported message |
| Limit outside the supported signed 64-bit binding range | `MYLITE_ERROR` | `1064`, `42000`, `LIMIT literal is outside the supported range` |
| All SQLite rowid aliases shadowed for limited update | `MYLITE_ERROR` | `1064`, `42000`, `UPDATE LIMIT requires an unshadowed SQLite rowid alias` |
| Assignment outside supported descriptor range | `MYLITE_ERROR` | `1264`, `22003`, `Out of range value for column '<column>' at row 1` |
| Predicate literal outside supported descriptor range | `MYLITE_ERROR` | `1264`, `22003`, `Out of range value for column '<column>' in WHERE` |
| `NULL` assigned to `NOT NULL` column | `MYLITE_ERROR` | `1048`, `23000`, `Column '<column>' cannot be null` |
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
| `UPDATE numbers SET i = 5` | Changes every row whose `i` was not already `5`; `ROW_COUNT()` reports the changed row count and `@@warning_count` is `0`. |
| Repeating the same `UPDATE numbers SET i = 5` | Reports `ROW_COUNT() == 0`; no warnings. |
| `UPDATE numbers SET n = NULL WHERE n IS NULL` | Matches rows but changes none; reports `ROW_COUNT() == 0`; no warnings. |
| `UPDATE numbers SET n = NULL WHERE n IS NOT NULL` | Changes only non-`NULL` rows; no warnings. |
| `UPDATE numbers SET nn = NULL` for a `NOT NULL` column | Error `1048`, SQLSTATE `23000`, `Column 'nn' cannot be null`. |
| `UPDATE numbers SET nn = NULL WHERE id = 999` and `UPDATE numbers SET nn = NULL LIMIT 0` | No row is selected; statement succeeds with `ROW_COUNT() == 0`, `@@warning_count == 0`, and rows unchanged. |
| `UPDATE numbers SET i = -10 WHERE id = 1` | Updates only matching rows and reports exact changed rows. |
| `UPDATE numbers SET i = 12 WHERE i <=> 1` | With non-`NULL` right operand, matches the same row set as equality. |
| `UPDATE numbers SET i = 100 ORDER BY i LIMIT 2` | Considers the two rows with the lowest `i` values; no warnings. |
| `UPDATE numbers SET i = 100 ORDER BY i DESC LIMIT 1` | Considers the row with the highest `i` value; no warnings. |
| `UPDATE numbers SET i = 100 ORDER BY n LIMIT 2` | Rows with `NULL` order keys sort first for ascending order. |
| `UPDATE numbers SET i = 100 ORDER BY n DESC LIMIT 2` | Rows with `NULL` order keys sort last for descending order. |
| `UPDATE numbers SET i = 100 ORDER BY i` | Accepted and updates all changed rows; no warnings. |
| `UPDATE numbers SET i = 100 LIMIT 0` | Updates no rows and reports `ROW_COUNT() == 0`; no warnings. |
| `UPDATE numbers SET i = 100 LIMIT 10` | Considers all rows when the limit exceeds the matched row count; no warnings. |
| `UPDATE numbers SET i = 100 LIMIT 1` where the first matched row is already `100` | The no-op row consumes the row-count limit and `ROW_COUNT()` reports `0`. |
| `UPDATE numbers` without a selected schema | Error `1046`, SQLSTATE `3D000`, `No database selected`. |
| `UPDATE db.numbers` without a selected schema | Succeeds when `db` exists. |
| Unknown unqualified or qualified table | Error `1146`, SQLSTATE `42S02`, `Table '<schema>.<table>' doesn't exist`. |
| Unknown schema | Error `1049`, SQLSTATE `42000`, `Unknown database '<schema>'`. |
| Unknown assignment column | Error `1054`, SQLSTATE `42S22`, `Unknown column '<column>' in 'field list'`. |
| Unknown `WHERE` column | Error `1054`, SQLSTATE `42S22`, `Unknown column '<column>' in 'where clause'`. |
| Unknown `ORDER BY` column | Error `1054`, SQLSTATE `42S22`, `Unknown column '<column>' in 'order clause'`. |
| `UPDATE ... LIMIT +1`, `LIMIT -1`, `LIMIT 1.0`, `LIMIT '1'`, `LIMIT 0x1`, `LIMIT b'1'`, `LIMIT ?` | Error `1064`, SQLSTATE `42000`. |
| `UPDATE ... LIMIT 1 OFFSET 1` and `UPDATE ... LIMIT 1, 1` | Error `1064`, SQLSTATE `42000`. |
| `UPDATE ... LIMIT 9223372036854775807` | Accepted as a large row count. |
| `UPDATE ... LIMIT 9223372036854775808` and `LIMIT 18446744073709551615` | Accepted by MySQL; intentionally rejected by MyLite in this phase because the value is outside SQLite signed-64 binding range. |
| `UPDATE ... LIMIT 18446744073709551616` | Error `1064`, SQLSTATE `42000`. |
| `UPDATE numbers SET i = 2147483648` and `UPDATE numbers SET iu = -1` | Error `1264`, SQLSTATE `22003`, with row `1` in the message. |
| `UPDATE numbers SET i = 2147483648 WHERE id = 999`, `UPDATE numbers SET i = 2147483648 LIMIT 0`, `UPDATE numbers SET iu = -1 WHERE id = 999`, and `UPDATE numbers SET iu = -1 LIMIT 0` | No row is selected; statement succeeds with `ROW_COUNT() == 0`, `@@warning_count == 0`, and rows unchanged. |
| `UPDATE numbers SET bu = 9223372036854775808` | Accepted by MySQL for `BIGINT UNSIGNED`; intentionally rejected by MyLite in this phase because it is outside the current physical range. |
| Table aliases, table-qualified assignment targets, multiple assignments, column-to-column assignments, arithmetic assignments, `LOW_PRIORITY`, and `IGNORE` | Accepted by MySQL for applicable ordinary single-table updates; intentionally rejected by MyLite in this phase. |
| Table-qualified order columns, expression order keys, ordinal order keys, and multiple order keys | Accepted or resolved by MySQL for applicable ordinary single-table updates; intentionally rejected by MyLite in this phase. |

The reproducible probe script for this phase is
`packages/libmylite/tests/mysql_baseline_update_lifecycle_expectations.sh`.

## Compatibility Status

This feature moves only the exact supported subset to partial support after
implementation:

- `UPDATE` single-table: persistent base tables only, with one unqualified
  descriptor assignment, optional baseline `WHERE` with the current qualified
  predicate-column subset, one supported `ORDER BY` key, and
  `LIMIT row_count`;
- `=` assignment operator: one descriptor-column assignment in the supported
  `UPDATE` subset only;
- `WHERE`: existing descriptor-driven integer/`NULL` predicate subset reused
  for update targets;
- `ORDER BY`: one unqualified descriptor integer/`NULL` column only, with
  optional `ASC` or `DESC`;
- `LIMIT`: one unsigned decimal row-count literal in signed 64-bit range only;
- integer/`NULL` assignment conversion: same baseline descriptor ranges as
  `INSERT ... VALUES`, now reused for `UPDATE`.

Full `UPDATE`, aliases, partitions, multi-table and joined updates, CTEs,
modifiers, full assignment expressions, defaults, table-qualified assignments,
multiple assignments, full `ORDER BY`, full `LIMIT/OFFSET`, expression
ordering, ordinal ordering, collations, triggers, cascades, foreign keys,
privilege semantics, protocol-grade metadata, and arbitrary SQLite pass-through
remain unsupported.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.update_lifecycle`.

Coverage must include:

- parser/AST acceptance for `UPDATE table SET column = value`, optional
  `WHERE`, optional `ORDER BY`, `ASC`, `DESC`, and `LIMIT row_count`;
- successful full-table, no-op, filtered, ordered, limited, and no-match
  updates over `INT`, `INTEGER`, `BIGINT`, `INT UNSIGNED`, `INTEGER UNSIGNED`,
  and `BIGINT UNSIGNED` values in the supported physical range;
- assignment of supported integer literals and `NULL`, including `NULL` into
  nullable columns, no-op nullable `NULL` assignments, and deterministic
  diagnostics for `NULL` into `NOT NULL` columns when at least one row is
  selected;
- assignment range boundaries and out-of-range diagnostics for signed and
  unsigned integer families when at least one row is selected, plus no-error
  behavior for no-match and `LIMIT 0` selections;
- `WHERE` predicate reuse, including comparisons, `<=>` with non-`NULL`
  integer right operands, `IS NULL`, and `IS NOT NULL`;
- `ORDER BY` plus `LIMIT` updates for default direction, `ASC`, `DESC`,
  nullable integer columns, `NULL` ordering, and duplicate sort-key ties
  without overclaiming which tied row is updated when the limit stops inside a
  tie group;
- `ORDER BY` without `LIMIT`, `LIMIT 0`, exact row counts, row counts larger
  than the matched set, maximum supported row count, unsupported offset forms,
  signed limit forms, non-decimal limit forms, and out-of-range limit values;
- a matched-row `LIMIT` case where a no-op row consumes the limit and changed
  `affected_rows` remains `0`;
- schema-qualified and unqualified target resolution, no selected schema,
  unknown schema, unknown table, and reserved `_mylite_*` names;
- affected-row count, warning count, absence of result rows, and remaining rows
  after each update;
- unknown assignment columns, unknown predicate columns, and unknown ordering
  columns;
- unsupported update syntax rejected deterministically: aliases,
  table-qualified assignment targets, table-qualified order columns, multiple
  assignments, expression assignments, column-to-column assignments,
  arithmetic assignments, expression order keys, ordinal order keys, multiple
  sort keys, string/decimal/float/hex/bit assignment or limit literals,
  parameters, functions, joins, multi-table update, `PARTITION`,
  `LOW_PRIORITY`, `IGNORE`, subqueries, CTEs, and query modifiers;
- reopen persistence: inserted rows survive close/reopen, updates persist
  after close/reopen, and sorted/limited selects observe updated rows;
- update after table rename and after drop;
- physical SQLite payload behavior without touching the MyLite preamble;
- independent file-backed handles with independent updated row state;
- zero-initialized cleanup for new planner/result objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, basic create/drop lifecycle, table rename lifecycle, row values
  lifecycle, select-where lifecycle, select-order-limit lifecycle, delete
  lifecycle, client-data, and registration tests still pass.

## Build Integration

Add any new runtime/analyzer/planner/catalog SQL execution sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.update_lifecycle$' --output-on-failure
ctest --preset dev -R '^libmylite\.(parser|runtime\.basic_table_lifecycle|runtime\.table_rename_lifecycle|runtime\.row_values_lifecycle|runtime\.select_where_lifecycle|runtime\.select_order_limit_lifecycle|runtime\.delete_lifecycle)$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_update_lifecycle_expectations.sh
cmake --workflow --preset check
```

Then review the final diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, descriptor-driven physical updates, integer/`NULL` assignment
conversion correctness for the admitted subset, integer conversion correctness
for the admitted predicate/limit subset, `NULL` ordering correctness, exact
changed-row affected-row semantics, matched-row `LIMIT` behavior, file-format
safety, VFS preservation, zero-init safety, cleanup on failure, scope control,
compatibility-matrix accuracy, and test relevance.
