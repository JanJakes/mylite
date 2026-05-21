# Baseline UPDATE Constant Arithmetic Assignment

## Summary

This phase expands descriptor-driven single-table `UPDATE` with a narrow
constant integer arithmetic assignment form:

```sql
UPDATE target
SET integer_column = constant_integer_arithmetic_expression
[WHERE ...]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

The expression is source-free. It may contain integer literals, `TRUE`,
`FALSE`, `NULL`, unary signs, parentheses, and `+`, binary `-`, and `*`.
Descriptor columns, table-qualified names, functions, parameters, strings,
decimals, floats, hex, bit literals, division, modulo, subqueries, and
table-backed expression evaluation remain outside this slice.

This is a bridge between the older literal-only `UPDATE` path and future
general expression support. MyLite evaluates the admitted constant expression
itself, validates the result against the target descriptor, and still executes
one descriptor-built SQLite `UPDATE` with bound values.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-update-arithmetic-assignment/specs.md`
  - `docs/specs/baseline-update-multiple-assignments/specs.md`
  - `docs/specs/baseline-scalar-arithmetic-projection/specs.md`
  - `docs/specs/baseline-dml-string-numeric-coercion/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - integer type ranges:
    <https://dev.mysql.com/doc/refman/8.4/en/integer-types.html>
  - out-of-range and overflow handling:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_update_constant_arithmetic_assignment_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish these expectations for the admitted subset:

- `UPDATE t SET c = 1 + 2` stores `3` in matching rows.
- `TRUE` and `FALSE` participate as `1` and `0` in integer arithmetic.
- `NULL` in an arithmetic expression produces `NULL`.
- Assigning a `NULL` expression result into a nullable column succeeds; assigning
  it into a `NOT NULL` column fails in strict mode with `1048 / 23000`.
- `+`, binary `-`, and `*` use integer arithmetic for integer operands, with
  ordinary precedence and parenthesized grouping.
- Successful supported updates report changed rows through `ROW_COUNT()` and
  `@@warning_count = 0`.
- Reassigning a column to its current value reports `0` changed rows.
- Under `sql_mode = ''`, assigning a `NULL` expression result into a `NOT NULL`
  integer column stores the implicit integer value `0`, reports changed rows,
  and records one `1048` warning per matched row. Strict mode reports
  `1048 / 23000`.
- `WHERE`, `ORDER BY`, and `LIMIT` select the matched row set before the
  assignment is applied. `LIMIT 0` updates no rows.
- Target column range failures report `1264 / 22003`, out-of-range value for
  the target column.
- Signed 64-bit expression overflow reports `1690 / 22003`, `BIGINT` value out
  of range.
- MySQL accepts broader arithmetic operators such as `/`, `DIV`, `%`, and `MOD`.
  They remain deferred here because DML division-by-zero handling differs from
  scalar projection warning behavior and needs a focused slice.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public call validation, result
  handle creation/freeing, diagnostics, and public misuse behavior.
- Statement context: owns diagnostics reset, warning count, affected rows, and
  non-row result state for the outer update.
- Lexer/parser/AST: admits only a source-free constant arithmetic `update_value`
  grammar. The AST stays syntax-only and does not resolve names or target types.
- Analyzer/planner: resolves the update target and assignment column from
  descriptors, identifies admitted constant arithmetic expression shapes,
  evaluates the expression with MyLite-owned integer semantics, converts the
  result through the target descriptor, and rejects unsupported forms before
  physical SQL generation.
- Catalog: remains authoritative for logical schema/table identity, object
  kind, physical table names, descriptor type, nullability, unsigned state,
  defaults, keys, and auto-increment metadata. This phase does not mutate
  catalog rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful supported updates return the existing non-row
  result shape: no result columns, no result rows, exact changed-row
  `affected_rows`, and `warning_count == 0`.
- SQLite physical storage: owns row storage and mutation inside MyLite-generated
  physical tables. MyLite generates SQL only from descriptors, quoted
  identifiers, and bound parameters.
- Storage/VFS/file format: unchanged. Updates write only the shifted SQLite
  payload and must not touch the `.mylite` preamble.

## Supported SQL

The outer statement is the existing single-table `UPDATE` surface:

```sql
UPDATE table_name
SET column_name = update_value
[WHERE baseline_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

This phase adds a source-free `update_value` with at least one arithmetic
operator:

```sql
update_value:
    constant_arithmetic_expression

constant_arithmetic_expression:
    constant_arithmetic_expression + constant_arithmetic_term
  | constant_arithmetic_expression - constant_arithmetic_term
  | constant_arithmetic_term * constant_arithmetic_factor
  | constant_arithmetic_factor * constant_arithmetic_factor

constant_arithmetic_factor:
    integer_literal
  | TRUE
  | FALSE
  | NULL
  | + constant_arithmetic_factor
  | - constant_arithmetic_factor
  | ( constant_arithmetic_expression )
```

MyLite Lemon-syntax sketch:

```lemon
update_value(A) ::= update_constant_arithmetic_value(B). {
    A = B;
}

update_constant_arithmetic_value(A) ::=
    update_constant_arithmetic_expr(B). {
    A = B;
}

update_constant_arithmetic_expr(A) ::=
    update_constant_arithmetic_expr(B) PLUS(T) update_constant_arithmetic_term(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
update_constant_arithmetic_expr(A) ::=
    update_constant_arithmetic_term(B) PLUS(T) update_constant_arithmetic_term(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
update_constant_arithmetic_expr(A) ::=
    update_constant_arithmetic_expr(B) MINUS(T) update_constant_arithmetic_term(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}
update_constant_arithmetic_expr(A) ::=
    update_constant_arithmetic_term(B) MINUS(T) update_constant_arithmetic_term(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}

update_constant_arithmetic_term(A) ::=
    update_constant_arithmetic_term(B) STAR(T) update_constant_arithmetic_factor(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}
update_constant_arithmetic_term(A) ::=
    update_constant_arithmetic_factor(B) STAR(T) update_constant_arithmetic_factor(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}

update_constant_arithmetic_factor(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
update_constant_arithmetic_factor(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
update_constant_arithmetic_factor(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
update_constant_arithmetic_factor(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
update_constant_arithmetic_factor(A) ::=
    PLUS(P) update_constant_arithmetic_factor(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE, B);
}
update_constant_arithmetic_factor(A) ::=
    MINUS(M) update_constant_arithmetic_factor(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE, B);
}
update_constant_arithmetic_factor(A) ::=
    LPAREN(L) update_constant_arithmetic_expr(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
```

Runtime narrows this parsed shape further:

- only a single assignment uses constant arithmetic in this phase;
- the target column must be a supported integer-family descriptor;
- primary-key, unique-key, and `AUTO_INCREMENT` targets use the ordinary
  single-assignment update path because the source-free expression is
  materialized to one descriptor-compatible value before binding;
- integer results must fit the target descriptor range;
- `BIGINT UNSIGNED` results are limited to MyLite's current signed 64-bit
  physical storage envelope;
- `NULL` results require a nullable target column in strict mode, or use the
  current ordinary non-strict implicit integer adjustment for `NOT NULL`
  targets when neither strict mode is active;
- no generated SQLite SQL is created for unsupported grammar or target types.

## Semantics

Schema and object resolution follow the existing single-table `UPDATE` policy:

- unqualified target names use the selected schema;
- schema-qualified target names use the named schema without requiring a
  selected schema;
- reserved `_mylite_*` schema and table names are rejected before SQLite SQL is
  generated;
- unknown schemas, unknown tables, unsupported object kinds, and missing
  default schema use existing verified diagnostics.

Column resolution remains descriptor-driven:

- assignment target, predicate columns, and ordering columns resolve against
  MyLite descriptors, not SQLite metadata;
- current descriptor catalog identifier matching stays authoritative for case
  behavior;
- unknown assignment, predicate, or ordering columns fail before physical
  mutation.

Expression evaluation:

- MyLite evaluates the admitted constant expression once per statement before
  binding the physical update value;
- integer arithmetic uses the current signed 64-bit scalar arithmetic envelope;
- `TRUE` is `1`, `FALSE` is `0`, and `NULL` short-circuits the expression result
  to `NULL`;
- `+`, binary `-`, and `*` use checked signed 64-bit arithmetic;
- expression overflow reports a deterministic MySQL-compatible `BIGINT` range
  diagnostic;
- the computed non-`NULL` integer result is converted through the target
  descriptor using the same range and unsigned rules as literal update values;
- successful supported non-`NULL` conversions produce no warnings;
- supported `NULL` results into `NOT NULL` integer targets follow the existing
  strict versus non-strict update adjustment policy.

Assignment and result behavior:

- successful statements use the existing changed-row SQL predicate, so setting
  a column to its current value reports `affected_rows == 0`;
- successful statements return no row result set;
- `WHERE`, `ORDER BY`, and `LIMIT` use the existing descriptor-driven update
  implementation, including current `NULL` ordering behavior and `LIMIT 0`;
- this phase does not change automatic `ON UPDATE CURRENT_TIMESTAMP` handling,
  key maintenance, foreign keys, triggers, cascades, generated columns, or
  defaults.

The generated SQLite update remains the existing bound-value shape:

```sql
UPDATE "_mylite_user_table_<table_id>"
SET "column" = ?1
WHERE <descriptor predicate or rowid-limited subquery>
  AND ("column" IS NULL OR "column" <> ?N)
```

For a `NULL` expression result, the changed condition remains
`"column" IS NOT NULL`. Every identifier is quoted and every value, predicate,
and limit operand is bound.

SQLite fork patches are not required. This phase uses MyLite wrapper/planner
logic and public SQLite prepare, bind, step, and transaction APIs.

## Diagnostics

Existing diagnostics remain authoritative for:

- syntax errors and unsupported update grammar;
- missing default schema, unknown schema, unknown table, reserved target names,
  and unsupported object kinds;
- unknown assignment, predicate, and order columns;
- unsupported assignment values outside this narrow constant arithmetic form;
- unsupported limit literals and offset forms;
- physical SQLite failures, allocation failures, and public API misuse.

New or reused diagnostics for this slice:

- non-integer target: existing unsupported update assignment diagnostic for the
  target family;
- constant expression shape outside the admitted subset: deterministic
  unsupported update assignment diagnostic;
- descriptor target range failure: `1264 / 22003`, out-of-range value for the
  target column at row 1;
- signed 64-bit expression overflow: `1690 / 22003`, `BIGINT` value out of
  range in scalar arithmetic expression;
- `NULL` result into `NOT NULL`: existing strict `1048 / 23000` nullability
  diagnostic, or one non-strict `1048` warning per matched row plus implicit
  `0` for admitted integer-family targets;
- successful in-range non-adjusted updates: `warning_count == 0`.

MySQL accepts division and modulo expression assignments, but this phase rejects
`/`, `DIV`, `%`, and `MOD` in update assignment values rather than partially
implementing their DML division-by-zero behavior.

## Tests

Tests must cover:

- parser acceptance of `UPDATE t SET c = 1 + 2`, `2 + 3 * 4`,
  `(2 + 3) * 4`, unary signs, boolean operands, and `NULL + 1`;
- parser/runtime rejection of identifiers, table-qualified assignment targets,
  strings, decimals, floats, hex, bit literals, parameters, functions, division,
  modulo, source-column arithmetic mixed with constants outside existing support,
  and multiple constant arithmetic assignments;
- successful full-table, filtered, ordered-limited, and `LIMIT 0` updates over
  signed and unsigned integer-family columns within MyLite's physical range;
- `INT`, `INTEGER`, `BIGINT`, and their `UNSIGNED` forms for successful
  in-range assignment;
- primary-key, unique-key, and `AUTO_INCREMENT` single-assignment targets,
  including duplicate-key diagnostics and auto-increment counter advancement;
- `NULL` expression result into nullable and `NOT NULL` columns, including
  strict errors and non-strict implicit integer adjustment warnings per matched
  row;
- expression overflow and target range diagnostics;
- changed-row affected counts, warning counts, and no result rows;
- schema-qualified and unqualified target resolution through existing update
  tests;
- reopen persistence and independent file-backed handles for updated rows;
- `.mylite` preamble preservation through existing storage checks;
- existing parser, scalar arithmetic, update lifecycle, update arithmetic,
  update multiple assignment, select, delete, and file-backed tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-table-dml.md`,
`docs/compatibility/operators.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
constant integer arithmetic `UPDATE` assignment subset. Do not claim general
expression assignments, table-backed expressions, division/modulo DML,
function-valued assignments, expression ordering, or expression predicates.
