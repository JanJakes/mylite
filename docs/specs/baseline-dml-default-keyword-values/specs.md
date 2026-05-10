# Baseline DML DEFAULT Keyword Values

## Summary

This phase admits the MySQL DML `DEFAULT` keyword as a value in the currently
supported descriptor-backed integer DML paths:

```sql
INSERT [IGNORE] [INTO] table_name [(column_name[, ...])]
    VALUES (value_or_default[, ...])[, ...]

INSERT [IGNORE] [INTO] table_name
    SET column_name = value_or_default[, ...]

REPLACE [INTO] table_name [(column_name[, ...])]
    VALUES (value_or_default[, ...])[, ...]

REPLACE [INTO] table_name
    SET column_name = value_or_default[, ...]

UPDATE table_name
    SET column_name = DEFAULT
    [WHERE ...] [ORDER BY column_name [ASC | DESC]] [LIMIT row_count]
```

`DEFAULT` is resolved by MyLite from the target column descriptor. The effective
value is then bound to generated SQLite statements as an integer or SQL `NULL`.
SQLite physical default clauses are not the compatibility authority.

This is not general default expression support. The slice does not add
`DEFAULT(col_name)`, default expressions in scalar expressions, expression
assignments, `ON DUPLICATE KEY UPDATE`, generated columns, key behavior,
arbitrary SQLite pass-through, or wider MySQL DML syntax.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - `REPLACE`: <https://dev.mysql.com/doc/refman/8.4/en/replace.html>
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - Data type default values:
    <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_dml_default_keyword_values_expectations.sh`
  and verified against MySQL 8.4.9.

The official manual describes DML values as either an expression or `DEFAULT`
for `INSERT`, `REPLACE`, and single-table `UPDATE`. It also documents that a
nullable column without an explicit default is treated as having `DEFAULT NULL`,
while a `NOT NULL` column without an explicit default has no default in strict
mode.

Runtime probes against MySQL 8.4.9 confirm the following behavior for this
slice:

- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`, and
  `REPLACE ... SET` accept `DEFAULT` anywhere a supported explicit value is
  admitted by MyLite.
- `UPDATE ... SET column = DEFAULT` changes only rows whose stored value differs
  from the descriptor default, and `ROW_COUNT()` reports changed rows.
- Explicit descriptor integer defaults store the descriptor integer.
- Explicit or implicit nullable `DEFAULT NULL` stores SQL `NULL`.
- A nullable column whose default was removed with
  `ALTER TABLE ... ALTER column DROP DEFAULT` has no default; strict
  `INSERT` / `REPLACE` `DEFAULT` and matched `UPDATE ... SET column = DEFAULT`
  fail with error `1364`, SQLSTATE `HY000`, message
  `Field '<column>' doesn't have a default value`.
- A `NOT NULL` column with no explicit default fails the same way when a row is
  actually inserted, replaced, or matched for update.
- `UPDATE ... SET no_default_column = DEFAULT` with no matched rows, or with
  `LIMIT 0`, succeeds with zero affected rows and no warnings.
- `INSERT IGNORE ... DEFAULT` demotes missing-default errors into warning
  `1364`, stores `0` for numeric `NOT NULL` columns, and stores `NULL` for
  nullable dropped-default columns, matching the current baseline
  `INSERT IGNORE` adjustment policy.
- Supported in-range statements record `warning_count == 0`.
- MySQL also accepts broader expression forms such as `DEFAULT(col_name)`;
  those remain outside this slice and are recorded only to justify an explicit
  unsupported MyLite diagnostic.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  return existing non-row DML result objects and diagnostics.
- Statement context: diagnostics reset, warning storage, affected rows, and
  result ownership continue through existing statement-context and result
  helpers.
- Lexer/parser/AST: recognize `DEFAULT` as a DML value node only in the
  admitted `insert_value` and `update_value` positions. Source spans remain
  owned by the AST for diagnostics and parser tests.
- Analyzer/planner: resolves each `DEFAULT` against the already resolved target
  descriptor column. It converts the descriptor state into a `planned_value`
  integer or `NULL`, or emits the MySQL-compatible missing-default diagnostic.
- Catalog: owns durable default metadata. This feature must not mutate catalog
  rows, descriptor versions, catalog generation, descriptor caches, or
  `sqlite_schema_generation`.
- Result builder: reports affected rows and warnings using existing DML result
  conventions. Successful DML `DEFAULT` statements return no result rows.
- Storage/VFS/file format: no file-format or VFS changes. The `.mylite`
  preamble and shifted SQLite payload invariants remain unchanged.
- SQLite physical execution: MyLite generates descriptor-derived SQLite SQL
  over stable physical table names and binds effective default values through
  prepared statements. No public SQLite extension function or SQLite fork patch
  is required.

## Supported SQL

`value_or_default` in this phase is:

```sql
value_or_default:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | NULL
  | TRUE
  | FALSE
  | DEFAULT
```

The surrounding statement shapes remain exactly the currently supported
baseline shapes:

- persistent base tables only;
- existing unqualified and schema-qualified table-name resolution;
- existing descriptor column-list and assignment-column resolution;
- one-row `INSERT ... SET` and `REPLACE ... SET`;
- single- and multi-row `INSERT ... VALUES` and `REPLACE ... VALUES`;
- current `INSERT IGNORE` `VALUES` and `SET` warning demotion;
- current single-table `UPDATE` subset with exactly one unqualified assignment
  target, optional baseline `WHERE`, optional one unqualified descriptor
  `ORDER BY` column, optional `ASC` / `DESC`, and optional `LIMIT row_count`.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar, not MySQL's full grammar:

```lemon
insert_value(A) ::= INTEGER(T).
insert_value(A) ::= PLUS(P) INTEGER(T).
insert_value(A) ::= MINUS(M) INTEGER(T).
insert_value(A) ::= NULL(T).
insert_value(A) ::= TRUE(T).
insert_value(A) ::= FALSE(T).
insert_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}

update_value(A) ::= INTEGER(T).
update_value(A) ::= PLUS(P) INTEGER(T).
update_value(A) ::= MINUS(M) INTEGER(T).
update_value(A) ::= NULL(T).
update_value(A) ::= TRUE(T).
update_value(A) ::= FALSE(T).
update_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}
```

`REPLACE ... VALUES` and `REPLACE ... SET` continue to reuse the insert value
nonterminals because their currently supported row-building path is
insert-equivalent until primary and unique key descriptors exist.

## Descriptor Default Semantics

`DEFAULT` is resolved per target column:

| Descriptor state | Effective DML `DEFAULT` value |
| --- | --- |
| integer default | That signed 64-bit descriptor integer |
| nullable with implicit or explicit `DEFAULT NULL` | SQL `NULL` |
| nullable with dropped default | Missing-default diagnostic |
| `NOT NULL` with no explicit default | Missing-default diagnostic |

Descriptor default integers were already validated when the descriptor was
created or altered. DML `DEFAULT` resolution therefore does not reparse SQL
literals or consult SQLite metadata. If a catalog descriptor contains a default
kind outside the admitted baseline states, the planner reports a MyLite
internal unsupported/default diagnostic rather than passing unknown semantics
to SQLite.

## Conversion And Nullability

Explicit integer, `TRUE`, `FALSE`, and `NULL` values continue to use the
existing row-values and update conversion policies. `DEFAULT` itself performs
no literal conversion; it materializes the descriptor value:

- descriptor integer defaults are already within MyLite's current physical
  signed-64 storage range for `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`,
  `INTEGER`, `BIGINT`, and their currently supported `UNSIGNED` forms;
- descriptor `NULL` defaults bind SQL `NULL`;
- `DEFAULT` for a missing default fails like MySQL in strict `INSERT`,
  `REPLACE`, and matched `UPDATE`;
- `INSERT IGNORE` demotes missing-default `DEFAULT` errors to warning `1364`
  and stores the current MySQL-compatible adjusted value: `0` for numeric
  `NOT NULL`, `NULL` for nullable dropped-default columns.

`NULL` into `NOT NULL` remains governed by the existing explicit-`NULL`
diagnostic `1048`. `DEFAULT` into `NOT NULL` with no default uses missing
default diagnostic `1364`, not the bad-null diagnostic.

## UPDATE Ordering, Limits, And No-Match Behavior

`UPDATE ... SET column = DEFAULT` uses the existing update planning surface:

- optional `WHERE` is the current baseline descriptor predicate subset;
- optional `ORDER BY` is one unqualified descriptor column with default
  ascending order and optional `ASC` or `DESC`;
- `NULL` ordering, duplicate ordering keys, and no tie-order guarantee are
  inherited from the update lifecycle slice;
- optional `LIMIT` remains only nonnegative unsigned decimal `row_count` in
  signed-64 range;
- offset forms remain unsupported.

MySQL validates missing-default update assignments only when a row would be
updated. MyLite must therefore keep the existing update preflight shape: first
check whether the predicate and limit can match at least one row, then resolve
the assignment value. This preserves:

```sql
UPDATE t SET no_default_column = DEFAULT WHERE id = 999; -- 0 rows, no error
UPDATE t SET no_default_column = DEFAULT LIMIT 0;        -- 0 rows, no error
```

For matched rows, no-op assignment is filtered by the existing changed-row
condition. `affected_rows` reports changed rows, not matched rows.

## Physical SQLite Handling

Generated SQLite continues to use stable physical table names such as
`_mylite_user_table_<table_id>` and quoted descriptor column names. `DEFAULT`
does not appear in generated SQLite:

```sql
INSERT INTO "_mylite_user_table_1" ("id", "i") VALUES (?1, ?2)
UPDATE "_mylite_user_table_1" SET "i" = ?1 WHERE ...
```

The bound parameter is the effective descriptor default value. This preserves
MyLite's catalog authority and avoids dependency on SQLite default-expression
syntax or SQLite's physical schema text.

No rows are materialized in MyLite for ordinary DML. `INSERT` and `REPLACE`
already stage one planned value array per submitted row so parameters can be
bound safely. `UPDATE` keeps predicate, ordering, limiting, changed-row tests,
and mutation inside SQLite using descriptor-built SQL. This feature adds no
new full-table scan or client-side row filtering layer.

## Diagnostics

This phase reuses existing diagnostics where possible:

- syntax errors and unsupported grammar: MySQL parse error `1064` / `42000`;
- missing default schema: `1046` / `3D000`;
- unknown schema: `1049` / `42000`;
- unknown table: `1146` / `42S02`;
- reserved `_mylite_*` schema/table names: existing MyLite reserved-name
  diagnostic before generated SQLite;
- unsupported object kind: existing descriptor object-kind diagnostic;
- unknown insert/replace/update assignment column: `1054` / `42S22`,
  `Unknown column ... in 'field list'`;
- unknown update predicate or order column: existing where/order diagnostics;
- duplicate insert/replace target column: `1110` / `42000`;
- column-count mismatch: `1136` / `21S01`;
- `NULL` into `NOT NULL`: `1048` / `23000`;
- `DEFAULT` for a `NOT NULL` no-default column or nullable dropped-default
  column in strict insert/replace or matched update: `1364` / `HY000`;
- `INSERT IGNORE ... DEFAULT` missing-default adjustment warning:
  warning `1364` with the same message;
- unsupported assignment expression, unsupported order expression, unsupported
  limit expression, parameters, functions, string/decimal/float/hex/bit
  values, joins, aliases, partitions, CTEs, subqueries, query modifiers, and
  broader MySQL forms: deterministic existing unsupported or parse
  diagnostics;
- allocation failures: existing out-of-memory diagnostic;
- physical SQLite failures: existing physical-row or SQLite status mapping.

Supported in-range `DEFAULT` statements produce `warning_count == 0`.

## Unsupported SQL

This phase deliberately does not admit:

- `DEFAULT(col_name)`;
- bare `DEFAULT` in scalar `SELECT`, predicates, `ORDER BY`, `LIMIT`, or
  arbitrary expressions;
- `INSERT ... SELECT` / `REPLACE ... SELECT` source expressions involving
  `DEFAULT`;
- `INSERT ... ON DUPLICATE KEY UPDATE`;
- `UPDATE SET column = DEFAULT + 1` or any expression assignment;
- table-qualified assignment targets;
- multiple `UPDATE` assignments;
- aliases, partitions, `LOW_PRIORITY` / `IGNORE` for `UPDATE`, multi-table
  updates, joined updates, CTEs, subqueries, functions, parameters, string,
  decimal, float, hex, or bit default values as DML inputs;
- primary/unique key `REPLACE` delete-insert behavior;
- generated columns, auto-increment, triggers, cascades, foreign keys,
  defaults in protocol metadata beyond existing result conventions, privilege
  semantics, or SQLite fork patches.

## Tests

Add a focused C runtime test and MySQL expectation script covering:

- successful `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and `UPDATE ... SET column = DEFAULT`;
- explicit integer defaults for `INT`, `INTEGER`, `BIGINT`, and supported
  `UNSIGNED` forms within the current physical range;
- nullable implicit and explicit `DEFAULT NULL`;
- `ALTER TABLE ... ALTER column DROP DEFAULT` followed by strict DML
  `DEFAULT` diagnostics;
- `NOT NULL` no-default strict diagnostics;
- `INSERT IGNORE` warning demotion for explicit `DEFAULT`;
- update changed-row affected counts, no-op updates, no-match and `LIMIT 0`
  missing-default skips, `WHERE`, `ORDER BY`, and `LIMIT` interactions;
- schema-qualified and unqualified target resolution through existing tests or
  new focused assertions;
- unknown target columns, unsupported expressions, `DEFAULT(col_name)`, and
  unsupported wider syntax;
- reopen persistence, update after rename/drop where applicable, independent
  file-backed handles, and `.mylite` preamble preservation;
- unchanged existing lexer, parser, runtime lifecycle, catalog, storage, VFS,
  and registration tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-table-dml.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for this exact
limited DML `DEFAULT` keyword surface. Do not claim full default expressions,
`DEFAULT(col_name)`, general expression evaluation, generated columns,
auto-increment, key-aware replacement, or broader DML support.
