# INSERT ... SET

## Scope

This feature specifies MySQL's assignment-form `INSERT` for a single row. It is
the Task 14 companion to the implemented `INSERT ... VALUES` runtime and should
reuse the same target resolution, default handling, physical write path,
affected-row reporting, and last-insert-id state where possible.

In scope:

- `INSERT [INTO] table_name SET col_name = value [, col_name = value] ...`
- schema-qualified target table names and selected-schema resolution
- unqualified and target-qualified assignment column names
- a nonempty assignment list whose source order is preserved
- assignment values using the current expression grammar, `DEFAULT`, `NULL`,
  and `CURRENT_TIMESTAMP`
- omitted-column defaults, explicit defaults, required-column diagnostics, and
  nullable implicit `NULL`
- duplicate assignment diagnostics, including case and quoted identifier
  variants
- unknown assignment target diagnostics
- MySQL assignment-order behavior for column references in assignment
  expressions
- `AUTO_INCREMENT` allocation, explicit values, sequence advancement, affected
  rows, and session last insert id
- failure atomicity for the single inserted row

Out of scope for Task 14:

- `INSERT ... VALUES`, already covered by `docs/specs/insert-values/specs.md`
- `INSERT ... SELECT`, `INSERT ... TABLE`, and standalone `VALUES`
- `INSERT IGNORE`
- `LOW_PRIORITY`, `HIGH_PRIORITY`, and `DELAYED`
- `PARTITION (...)`
- row aliases and column aliases after the `SET` clause
- `ON DUPLICATE KEY UPDATE`
- `DEFAULT(col_name)` function syntax
- generated columns, triggers, foreign keys, privileges, warning records,
  information strings, and protocol OK packets
- full MySQL type conversion, range clipping, string truncation, temporal
  validation, and SQL-mode variants beyond the verified strict default mode
- arbitrary scalar function evaluation and the full expression operator
  surface planned for later expression tasks

## Sources

- MySQL 8.4 Reference Manual, `INSERT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `INSERT ... ON DUPLICATE KEY UPDATE`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html
- MySQL 8.4 Reference Manual, Data Type Default Values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, Using `AUTO_INCREMENT`:
  https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html
- Existing MyLite specs:
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/column-attributes/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`
  under the default strict SQL mode.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 behavior summary

### Syntax

`INTO` is optional:

```sql
INSERT t SET a = 1;
INSERT INTO t SET a = 1;
```

The assignment list is mandatory and must contain at least one assignment.
`INSERT INTO t SET`, `INSERT INTO t SET a`, `INSERT INTO t SET a =`, and a
trailing comma all fail with syntax error 1064 / SQLSTATE `42000`.

The target table may be schema-qualified. Assignment targets may be
unqualified, table-qualified, or schema-table-qualified when the qualifier
matches the insert target. A mismatched qualifier is reported as an unknown
column in the field list.

### Target and assignment diagnostics

Target resolution matches the existing `INSERT ... VALUES` behavior:

- no selected schema for an unqualified table: 1046 / `3D000`
- missing explicit schema: 1049 / `42000`
- missing table in an existing schema: 1146 / `42S02`
- system schema target: MySQL reports access denied; MyLite should use its
  existing system-schema rejection diagnostic

Assignment names are resolved case-insensitively against target-table columns.
Quoted identifiers do not create case-distinct assignment targets. Duplicate
assignments to the same column fail with 1110 / `42000`. Unknown assignment
targets fail with 1054 / `42S22`.

When a statement contains both an unknown assignment target and a duplicate
known target, MySQL reports the unknown column. MyLite should resolve all
assignment targets before duplicate detection to preserve that precedence.

### Defaults, NULL, and omitted columns

Columns omitted from the `SET` list receive the same value they would receive
when omitted from an `INSERT ... VALUES` column list:

- explicit default value when present
- `NULL` for nullable columns without an explicit default
- generated next value for an auto-increment column
- strict-mode error 1364 / `HY000` for a required non-auto column with no
  explicit default

The right-hand side `DEFAULT` requests the target column default. It fails with
1364 for a required non-auto column that has no default. Explicit `NULL` into a
required non-auto column fails with 1048 / `23000`. `CURRENT_TIMESTAMP` is
accepted as an assigned value for temporal columns and as a default expression
for columns whose metadata already supports it.

### Assignment order

MySQL evaluates assignment expressions from left to right against a candidate
insert row. That row starts with each column's default or implicit value.
Earlier assignments are visible to later assignment expressions. Later
assignments are not visible to earlier expressions.

Observed examples:

| Table shape | Statement | Stored row effect |
| --- | --- | --- |
| `a INT DEFAULT 3, b INT DEFAULT 4` | `SET b=a+1, a=5` | `a=5`, `b=4` |
| `a INT DEFAULT 3, b INT DEFAULT 4` | `SET a=DEFAULT, b=a+1` | `a=3`, `b=4` |
| `a INT, b INT` | `SET b=a+1, a=5` | `a=5`, `b=NULL` |
| `a INT NOT NULL, b INT DEFAULT 4` | `SET b=a+1, a=5` | `a=5`, `b=1` |

The last case shows that a required numeric column with no explicit default can
contribute its implicit type default to an earlier expression, while the final
row still must assign the required column before constraint validation.

For an `AUTO_INCREMENT` column, generated values are assigned after ordinary
assignment expression evaluation. References to an omitted, `NULL`, `0`, or
`DEFAULT` auto-increment target therefore see `0`. A prior explicit nonzero
assignment to the auto-increment column is visible to later expressions.

### AUTO_INCREMENT, affected rows, and insert ids

A successful `INSERT ... SET` inserts one row and reports affected rows `1`.
For an auto-increment column, omitted, `NULL`, `0`, and `DEFAULT` values
allocate the next sequence value. The session last insert id becomes that
generated value. Explicit nonzero auto-increment values are stored as written,
advance the next sequence when high enough, and do not replace the previous
session last insert id.

A failed duplicate-key insert can consume an auto-increment value before the
statement fails. The row is not inserted and the session last insert id remains
unchanged, but the next successful generated insert observes the consumed gap.

### Deferred modifiers

MySQL accepts more `INSERT ... SET` surface than this task implements:

- `INSERT IGNORE ... SET` demotes some errors to warnings. A duplicate unique
  key probe reported affected rows `0`, one warning, unchanged last insert id,
  and a consumed auto-increment value.
- `LOW_PRIORITY` and `HIGH_PRIORITY` are accepted without warnings in the
  probed InnoDB table.
- `DELAYED` is accepted and converted to a normal insert with warning 3005.
- `PARTITION (...)` is accepted for partitioned tables. A partition mismatch
  fails with 1748 / `HY000`.
- `AS row_alias[(col_alias,...)]` is accepted after the `SET` assignment list.
- `ON DUPLICATE KEY UPDATE` is accepted after the optional alias. Duplicate-key
  update probes reported affected rows `2`.

MyLite should intentionally reject these modifiers at parse time for Task 14
unless a narrower parser placeholder already exists. They remain separate
compatibility features because they affect warnings, affected rows, aliases,
conflict handling, or partition routing.

## MyLite behavior

### Parser and AST

Add an `insert_set_statement` AST node. Children are appended as:

1. target table name
2. assignment list

Each assignment node stores:

1. the assignment target identifier or qualified identifier
2. the assigned value expression or `DEFAULT`

The assignment list must preserve source order. Runtime assignment-order
semantics depend on the original order and must not sort by ordinal position.

The statement node records no priority, ignore, delayed, partition, alias, or
duplicate-key modifiers in Task 14. Those forms remain parse errors until their
own specs define MyLite behavior.

### Lemon grammar snippets

These snippets describe MyLite's intended Task 14 grammar:

```lemon
insert_set_statement ::= INSERT opt_into table_name SET insert_set_assignment_list.

opt_into ::= .
opt_into ::= INTO.

insert_set_assignment_list ::= insert_set_assignment.
insert_set_assignment_list ::= insert_set_assignment_list COMMA insert_set_assignment.

insert_set_assignment ::= insert_set_target EQ insert_set_value.

insert_set_target ::= qualified_identifier.

insert_set_value ::= expression.
insert_set_value ::= DEFAULT.
```

The following MySQL grammar surface is intentionally deferred and should not be
accepted by the Task 14 grammar:

```lemon
/* Deferred: priority and duplicate-warning demotion. */
insert_set_statement ::= INSERT insert_priority INSERT_IGNORE opt_into table_name
                         SET insert_set_assignment_list.

/* Deferred: partition routing. */
insert_set_statement ::= INSERT opt_into table_name insert_partition_clause
                         SET insert_set_assignment_list.

/* Deferred: row and column aliases for duplicate-key expressions. */
insert_set_statement ::= INSERT opt_into table_name SET insert_set_assignment_list
                         insert_row_alias.

/* Deferred: duplicate-key update semantics. */
insert_set_statement ::= INSERT opt_into table_name SET insert_set_assignment_list
                         insert_duplicate_key_update_clause.
```

### Runtime execution

Preparing a parsed `INSERT ... SET` creates a custom statement handle. The
first `mylite_step()` performs validation and side effects; later steps return
`MYLITE_DONE`.

Target resolution:

- Qualified `schema.table` targets the written schema.
- Unqualified table names use the selected default schema.
- A missing selected schema, missing explicit schema, missing table, and
  system schema target use the diagnostics documented above.

Assignment target resolution:

- Load column metadata from `__mylite_column_catalog` ordered by
  `ORDINAL_POSITION`.
- For an unqualified assignment target, match the identifier
  case-insensitively against target-table column names.
- For a two-part assignment target, require the first identifier to match the
  target table name or alias once aliases exist; Task 14 has no aliases, so it
  must match the table name.
- For a three-part assignment target, require schema and table qualifiers to
  match the resolved target table.
- Report 1054 for the first unknown assignment target before duplicate
  validation.
- Report 1110 when two known assignment targets resolve to the same catalog
  column.

Value resolution:

- Reuse the deterministic scalar handling from `INSERT ... VALUES` for
  literals, unary numeric values, strings, booleans, `NULL`, `DEFAULT`, and
  `CURRENT_TIMESTAMP`.
- Preserve the current expression AST for binary arithmetic and identifiers.
  Task 14 needs enough expression evaluation to support MySQL assignment-order
  tests over the existing parser's arithmetic and column-reference subset.
- Resolve column references in assignment expressions against the candidate row
  value at the point where the expression is evaluated.
- Return the existing deterministic unsupported-expression diagnostic for
  expression forms outside the implemented subset.

Candidate-row initialization and assignment:

- Initialize every column slot with the value MySQL would expose before
  explicit assignments: explicit defaults, nullable `NULL`, implicit type
  defaults for expression evaluation, and `0` for not-yet-generated
  auto-increment values.
- Track whether each required no-default column is explicitly assigned by the
  end of the list. A required no-default column that remains unassigned fails
  with 1364 even if its implicit type default was used by an earlier
  expression.
- Evaluate assignments in source order and write each result into the
  candidate row slot for the target column.
- `DEFAULT` on a required no-default column fails with 1364.
- Explicit `NULL` on a required non-auto column fails with 1048.

Storage and side effects:

- Insert exactly one physical row through the same catalog-derived SQLite table
  name and binding path used by `INSERT ... VALUES`.
- Run the insert in a SQLite transaction. On validation, duplicate-key, binding,
  or SQLite failure, roll back the physical row.
- Preserve consumed generated auto-increment values according to the Task 13
  sequence rules.
- Update `__mylite_table_catalog.AUTO_INCREMENT` when generated or explicit
  high values advance the next sequence value.
- Set statement affected rows to `1` after a successful insert.
- Update session last insert id to the generated auto-increment value only when
  the successful row used a generated value. Explicit nonzero auto values leave
  it unchanged.

### Explicit deferred behavior

MyLite intentionally documents these Task 14 boundaries:

- `INSERT IGNORE`, priority modifiers, `DELAYED`, partitions, aliases, and
  `ON DUPLICATE KEY UPDATE` remain parse/runtime deferred.
- Warning records and information strings are deferred.
- Full expression and type-conversion fidelity is deferred. The Task 14 runtime
  only needs the existing parser's small deterministic expression subset plus
  column-reference lookup required for assignment order.
- Generated-column validation and generated-column `DEFAULT` behavior are
  deferred until generated columns exist.
- SQL-mode variants such as `NO_AUTO_VALUE_ON_ZERO` are deferred.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `INSERT t SET v='without_into', nn=1` | Accepted without `INTO`; affected rows `1`. |
| `INSERT INTO t SET v='with_into', nn=2` | Accepted with `INTO`; affected rows `1`. |
| `INSERT INTO schema.qtab SET v=8` | Resolves the qualified table and inserts one row. |
| `INSERT INTO defaults_set SET must=1, v=DEFAULT, nn=DEFAULT, nul=NULL, ts=CURRENT_TIMESTAMP` | Applies explicit defaults, explicit `NULL`, current timestamp, generated id, affected rows `1`. |
| `INSERT INTO defaults_set SET must=2` | Omitted columns receive defaults or nullable `NULL`. |
| `INSERT INTO defaults_set SET v='missing_required'` | Fails with 1364; no row inserted. |
| `INSERT INTO defaults_set SET must=NULL` | Fails with 1048; no row inserted. |
| `INSERT INTO defaults_set SET must=DEFAULT` | Fails with 1364; no row inserted. |
| `INSERT INTO ao_default SET b=a+1, a=5` where `a DEFAULT 3` | Stores `a=5`, `b=4`. |
| `INSERT INTO ao_nullable SET b=a+1, a=5` where `a` is nullable | Stores `a=5`, `b=NULL`. |
| `INSERT INTO ao_required SET b=a+1, a=5` where `a INT NOT NULL` has no default | Stores `a=5`, `b=1`. |
| `INSERT INTO auto_ref SET id=NULL, a=id` | Stores generated id and `a=0`. |
| `INSERT INTO auto_ref SET id=0, a=id` | Stores generated id and `a=0`. |
| `INSERT INTO auto_ref SET id=DEFAULT, a=id` | Stores generated id and `a=0`. |
| `INSERT INTO auto_ref SET id=20, a=id` | Stores `id=20`, `a=20`; session last insert id unchanged. |
| `INSERT INTO auto_ref SET a=id, id=30` | Stores `id=30`, `a=0`; session last insert id unchanged. |
| `INSERT INTO diag_set SET a=1, A=2` | Fails with 1110 duplicate-column diagnostic. |
| ``INSERT INTO diag_set SET a=1, `a`=2`` | Fails with 1110 duplicate-column diagnostic. |
| ``INSERT INTO quoted_diag SET CamelCase=1, `camelcase`=2`` | Fails with 1110 duplicate-column diagnostic. |
| `INSERT INTO diag_set SET missing_col=1` | Fails with 1054 unknown-column diagnostic. |
| known duplicate plus any unknown target | Fails with 1054 unknown-column diagnostic. |
| no selected schema | Fails with 1046 `No database selected`. |
| qualified missing schema | Fails with 1049 `Unknown database`. |
| missing table in existing schema | Fails with 1146 table-does-not-exist diagnostic. |
| system schema target | Fails with a system-schema/access diagnostic before mutation. |
| `AUTO_INCREMENT=3`; generated, `NULL`, `0`, `DEFAULT`, explicit `10`, generated | Stores ids `3`, `4`, `5`, `6`, `10`, `11`; explicit `10` leaves previous last insert id unchanged. |
| duplicate unique insert after generated id `20` | Fails with 1062, consumes id `21`, leaves last insert id `20`, next generated id is `22`. |
| `INSERT IGNORE ... SET` duplicate | MySQL reports affected rows `0` and one warning; MyLite parse/runtime deferred. |
| `INSERT LOW_PRIORITY ... SET` | MySQL accepts; MyLite parse/runtime deferred. |
| `INSERT HIGH_PRIORITY ... SET` | MySQL accepts; MyLite parse/runtime deferred. |
| `INSERT DELAYED ... SET` | MySQL inserts with warning 3005; MyLite parse/runtime deferred. |
| `INSERT ... PARTITION (p0) SET ...` | MySQL accepts for matching partitioned rows and errors 1748 on mismatch; MyLite deferred. |
| `INSERT ... SET ... AS new` | MySQL accepts; MyLite deferred. |
| `INSERT ... SET ... ON DUPLICATE KEY UPDATE ...` | MySQL accepts and can report affected rows `2`; MyLite deferred. |
| empty `SET` list or missing `=` | Fails with syntax error 1064. |

## Test plan

Parser tests:

- optional `INTO`
- schema-qualified target table names
- unqualified, table-qualified, and schema-table-qualified assignment targets
- multiple assignments preserving source order
- `DEFAULT`, `NULL`, strings, numbers, booleans, unary numeric values,
  `CURRENT_TIMESTAMP`, identifier references, and arithmetic expressions
- malformed empty assignment list, missing `=`, missing value, missing target,
  and trailing comma
- parse rejection for deferred `IGNORE`, priorities, `DELAYED`, partition,
  alias, and `ON DUPLICATE KEY UPDATE` forms

Runtime tests:

- successful insert through default schema and schema-qualified targets
- optional `INTO`
- omitted-column defaults and nullable implicit `NULL`
- explicit `DEFAULT`, explicit `NULL`, and `CURRENT_TIMESTAMP`
- required-column missing/default/null diagnostics
- assignment-order expression behavior, including forward reference to a later
  assignment, nullable defaults, required implicit numeric default, and
  auto-increment references
- duplicate assignment names, case variants, quoted variants, unknown targets,
  and unknown-before-duplicate precedence
- qualified assignment target matching and mismatch diagnostics
- auto-increment generated, `NULL`, `0`, `DEFAULT`, explicit high values,
  sequence gaps after duplicate failure, affected rows, and session last insert
  id
- no selected schema, missing schema, missing table, and system schema target
  diagnostics
- failure atomicity: no physical row after validation, duplicate-key, or
  unsupported-expression failure
