# Baseline UPDATE Arithmetic Assignment

## Summary

This phase extends the descriptor-driven single-table `UPDATE` slice with one
common row-relative assignment form:

```sql
UPDATE target
SET integer_column = integer_column + unsigned_integer_literal
[WHERE ...]
[ORDER BY ...]
[LIMIT ...]

UPDATE target
SET integer_column = integer_column - unsigned_integer_literal
[WHERE ...]
[ORDER BY ...]
[LIMIT ...]
```

The source column must be the same unqualified descriptor column as the target
column. The right operand is an unsigned decimal integer literal. This is not
general expression assignment, column-to-column assignment, key-column update
ordering, or arbitrary table-backed expression evaluation.

The implementation keeps the efficient physical row path: MyLite resolves and
checks names and ranges from descriptors, then generates one SQLite `UPDATE`
that mutates matching rows in SQLite storage without materializing the result
set in MyLite memory.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
  - `docs/specs/baseline-scalar-arithmetic-projection/specs.md`
  - `docs/specs/baseline-update-scalar-subquery-assignment/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - integer type ranges:
    <https://dev.mysql.com/doc/refman/8.4/en/integer-types.html>
  - out-of-range and overflow handling:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_update_arithmetic_assignment_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL Runtime Observations

MySQL 8.4.9 establishes these expectations for the admitted subset:

- `UPDATE t SET c = c + 1` and `UPDATE t SET c = c - 1` read the current row
  value of `c`.
- `NULL + n` and `NULL - n` evaluate to `NULL`. Assigning the resulting `NULL`
  back to a nullable same column is unchanged and is not counted as an affected
  row.
- `ROW_COUNT()` reports changed rows, not matched rows. `c = c + 0` and
  `c = c - 0` report `0` changed rows.
- `UPDATE ... WHERE` limits the matched rows before assignment evaluation.
- `ORDER BY ... LIMIT` chooses the row set with the existing MySQL ordering
  rules, including `NULL` before non-`NULL` for ascending order and after
  non-`NULL` for descending order. For ties in the single admitted order key,
  this slice does not claim a deterministic tie-breaker.
- `LIMIT 0` and no-match updates skip runtime assignment evaluation, including
  oversized numeric literals and arithmetic overflow.
- Unknown column names are resolved before row matching and still fail on
  no-match updates.
- Signed `INT`/`INTEGER` overflow outside the column range reports
  `1264 / 22003`, out-of-range value for the target column.
- `INT UNSIGNED` addition above `4294967295` reports `1264 / 22003`; unsigned
  subtraction below zero reports `1690 / 22003`, unsigned `BIGINT` expression
  overflow.
- Signed `BIGINT` addition/subtraction outside signed 64-bit range reports
  `1690 / 22003`, signed `BIGINT` expression overflow.
- MySQL can store `BIGINT UNSIGNED` values above signed 64-bit range. MyLite's
  current physical integer row-value envelope is signed 64-bit, so this slice
  admits only arithmetic results within that existing MyLite envelope.
- Successful supported updates report `@@warning_count = 0`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public call validation, result
  handle creation/freeing, diagnostics, and public misuse behavior.
- Statement context: owns diagnostics reset, warning count, affected rows, and
  transaction completion for the outer update statement. Arithmetic assignment
  does not publish a separate result.
- Lexer/parser/AST: admits only the narrow `identifier +/- unsigned_integer`
  update-value form and stores it as the existing binary-expression AST. The
  parser does not resolve names or decide type compatibility.
- Analyzer/planner: resolves the target table, assignment target, arithmetic
  source column, predicate columns, and order column against MyLite
  descriptors. It rejects unsupported expression shapes before SQLite SQL is
  generated.
- Catalog: remains descriptor authority for logical schemas, table identity,
  object kind, physical table names, columns, keys, and auto-increment
  metadata. Arithmetic update does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: successful supported updates return the existing non-row
  result shape: `column_count == 0`, `row_count == 0`, exact changed-row
  `affected_rows`, and `warning_count == 0`.
- SQLite physical storage: owns row storage and mutation inside MyLite-generated
  physical tables. MyLite builds the SQL from descriptors, quoted identifiers,
  and bound parameters only.
- Storage/VFS/file format: unchanged. Supported arithmetic updates write only
  the shifted SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL

The outer statement remains the existing limited single-table `UPDATE`:

```sql
UPDATE table_name
SET column_name = update_value
[WHERE baseline_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

This phase adds two `update_value` forms. The right operand is admitted only in
MyLite's current signed 64-bit SQLite binding range when at least one non-`NULL`
matched row would evaluate the expression. Oversized operands are skipped for
no-match, `LIMIT 0`, and all-`NULL` matched row sets because MySQL does not
raise arithmetic overflow for those cases.

```sql
update_value:
    column_name + unsigned_integer_literal
  | column_name - unsigned_integer_literal
```

MyLite Lemon-syntax sketch:

```lemon
update_value(A) ::= arithmetic_update_source_column(B) PLUS(T) INTEGER(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_INTEGER));
}

update_value(A) ::= arithmetic_update_source_column(B) MINUS(T) INTEGER(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_INTEGER));
}

arithmetic_update_source_column(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
arithmetic_update_source_column(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

Runtime narrows this parsed shape further:

- the source column must be unqualified;
- the source column name must resolve through the target table descriptor;
- the source column must be the same descriptor column as the assignment target;
- the target column must be an integer-family descriptor supported by the
  current MyLite physical integer storage envelope;
- the target column must not be auto-increment;
- the target column must not participate in a primary key or unique index.

Primary-key, unique-key, and auto-increment arithmetic updates are deferred
because MySQL's visible behavior depends on row update order, duplicate-key
diagnostics, and auto-increment counter interaction. This slice avoids a
partial implementation of that key-sensitive behavior.

## Semantics

Schema and object resolution follow the existing single-table `UPDATE` policy:

- unqualified target names use the selected schema;
- schema-qualified target names use the named schema without requiring a
  selected schema;
- reserved `_mylite_*` schema and table names are rejected before SQLite SQL is
  generated;
- unknown schemas, unknown tables, unsupported object kinds, and missing
  default schema use the existing verified diagnostics.

Column resolution is descriptor-driven:

- assignment target, arithmetic source, predicate columns, and ordering columns
  resolve against MyLite descriptors, not SQLite metadata;
- the current descriptor catalog's identifier matching policy remains the
  source of truth for case behavior;
- unknown assignment, arithmetic source, predicate, or ordering columns fail
  deterministically before physical mutation.

Assignment conversion:

- the right operand is parsed as an unsigned decimal integer literal within
  signed 64-bit range for evaluated non-`NULL` rows;
- oversized right operands are runtime-evaluated only when the update matches
  at least one non-`NULL` source row and `LIMIT` is not zero;
- if no row is matched, or `LIMIT 0` is used, right-operand conversion and
  overflow checks are skipped like MySQL runtime expression evaluation;
- for non-`NULL` source rows, MyLite verifies that `source +/- literal` stays
  inside the target descriptor's current supported range before executing the
  physical update;
- for `NULL` source rows, the result remains `NULL` and nullable same-column
  assignment is unchanged;
- `+ 0` and `- 0` are accepted and are no-op assignments for non-`NULL` rows.

The admitted integer result ranges are:

- signed `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`, `INTEGER`, and `BIGINT`
  within their MySQL signed ranges;
- unsigned `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`, and `INTEGER` within
  their MySQL unsigned ranges;
- `BIGINT UNSIGNED` only within MyLite's existing signed 64-bit physical
  storage range.

For `ORDER BY ... LIMIT`, MyLite reuses the current rowid-subquery shape from
the baseline `UPDATE` slice rather than SQLite's optional
`UPDATE ... ORDER BY ... LIMIT` syntax. The generated physical table invariant
remains that MyLite user tables are SQLite rowid tables unless an internal rowid
alias is shadowed, in which case the existing `UPDATE LIMIT` diagnostic is
used.

For `ORDER BY` without `LIMIT`, this slice documents only MySQL acceptance and
no additional visible row-set effect. The statement still updates all rows that
match the predicate and changed-row condition.

## Generated SQLite SQL Shape

The physical update uses quoted generated table and column identifiers:

```sql
UPDATE "_mylite_user_table_<table_id>"
SET "column" = "column" + ?1
WHERE <descriptor predicate or rowid limited subquery>
  AND ("column" IS NOT NULL AND "column" <> ("column" + ?N))
```

Subtraction uses `-` in both assignment and changed-row expressions. Automatic
`ON UPDATE CURRENT_TIMESTAMP` columns, when present and not themselves assigned,
continue to use existing bound parameters and are updated only when the primary
assignment changes a row.

Range validation uses a descriptor-built `SELECT 1 ... LIMIT 1` over the same
matched row set, with quoted identifiers and bound threshold values. It does not
scan rows into MyLite memory.

SQLite fork patches are not required. This phase uses public SQLite prepare,
bind, step, and transaction APIs plus MyLite-side SQL generation.

## Diagnostics

Existing diagnostics remain authoritative for:

- syntax errors and unsupported update grammar;
- missing default schema, unknown schema, unknown table, reserved target names,
  and unsupported object kinds;
- unknown assignment, predicate, and order columns;
- unsupported assignment values outside this narrow arithmetic form;
- unsupported limit literals and offset forms;
- physical SQLite failures, allocation failures, and public API misuse.

New deterministic diagnostics:

- unknown arithmetic source column: existing unknown-column diagnostic;
- arithmetic source column differs from target: `1064 / 42000`, unsupported
  arithmetic assignment message;
- qualified arithmetic source column: `1064 / 42000`, unsupported arithmetic
  assignment message;
- evaluated right operand outside the current signed 64-bit delta range:
  `1064 / 42000`, unsupported arithmetic assignment message;
- non-integer target: `1064 / 42000`, unsupported arithmetic assignment
  message;
- target participates in a primary key, unique index, or auto-increment:
  `1064 / 42000`, unsupported arithmetic assignment message;
- result outside non-`BIGINT` target range: `1264 / 22003`, out-of-range value
  for the target column at row 1;
- signed `BIGINT` arithmetic outside signed 64-bit range:
  `1690 / 22003`, signed `BIGINT` arithmetic out of range;
- unsigned subtraction below zero: `1690 / 22003`, unsigned `BIGINT` arithmetic
  out of range;
- `BIGINT UNSIGNED` results above MyLite's current signed 64-bit physical
  range: `1264 / 22003`, out-of-range value for the target column at row 1.

Successful in-range updates produce no warnings.

## Tests

Tests must cover:

- parser acceptance of `c = c + 1` and `c = c - 1`;
- parser/runtime rejection of constant arithmetic, column-to-column, qualified
  source columns, signed right operands, functions, strings, decimals, floats,
  hex, bit, parameters, and broader expressions;
- successful full-table, filtered, ordered-limited, and no-match arithmetic
  updates over signed and unsigned integer-family columns in the supported
  physical range;
- `NULL` source values, `+ 0` / `- 0`, changed-row affected counts,
  `warning_count == 0`, and no result rows;
- `LIMIT 0`, exact row counts, and row counts larger than the matched set;
- `ORDER BY` default direction, `ASC`, `DESC`, nullable order columns, `NULL`
  ordering, and duplicate-key ties without overclaiming tie order;
- range boundaries and overflow diagnostics for signed `INT`/`INTEGER`,
  unsigned `INT`/`INTEGER`, signed `BIGINT`, and current-range
  `BIGINT UNSIGNED`;
- schema-qualified and unqualified target resolution through existing update
  tests;
- reopen persistence, table rename interaction, and independent file-backed
  handles;
- file preamble preservation;
- existing parser, runtime lifecycle, scalar arithmetic, select, delete, and
  update tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-table-dml.md`,
`docs/compatibility/operators.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
supported row-relative `UPDATE` arithmetic assignment subset. Do not claim
general expression assignment, arbitrary table-backed expressions, key-column
arithmetic updates, or complete MySQL arithmetic semantics.
