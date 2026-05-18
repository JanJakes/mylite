# Baseline Row Control-Flow Functions

## Summary

This phase extends the existing row-scalar `SELECT` projection path with a
narrow table-backed control-flow subset:

```sql
IF(condition, true_value, false_value)
IFNULL(value, fallback)
COALESCE(value[, ...])
NULLIF(left_value, right_value)
ISNULL(value)
```

The scope is intentionally limited to projection in the current row-scalar
`SELECT` envelope. It does not add control-flow functions to `WHERE`, `ORDER
BY`, `GROUP BY`, `HAVING`, DML assignment values, defaults, generated columns,
or arbitrary expression positions. It also does not make MyLite's expression
metadata general.

The main architectural goal is to move common application projection patterns
such as `IFNULL(option_value, '')` and `COALESCE(col, fallback)` onto the
descriptor-backed row expression path without introducing per-statement
materialization or SQLite schema authority.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-ifnull-function/specs.md`
  - `docs/specs/baseline-coalesce-function/specs.md`
  - `docs/specs/baseline-nullif-function/specs.md`
  - `docs/specs/baseline-isnull-function/specs.md`
  - `docs/specs/baseline-case-operator/specs.md`
- Official MySQL 8.4 Reference Manual:
  - Flow control functions:
    <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
  - Comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_row_control_flow_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `IF(expr1, expr2, expr3)` returns `expr2` when `expr1` is nonzero and not
  `NULL`; otherwise it returns `expr3`.
- `IFNULL(expr1, expr2)` returns `expr1` when it is not `NULL`, otherwise
  `expr2`.
- `COALESCE(value, ...)` returns the first non-`NULL` argument, or `NULL` when
  all arguments are `NULL`.
- `NULLIF(expr1, expr2)` returns `NULL` when `expr1 = expr2`; otherwise it
  returns `expr1`.
- `ISNULL(expr)` returns integer `1` for `NULL` and `0` otherwise.
- Table-backed projection evaluates these functions once for each matched row
  and preserves the existing `WHERE`, `ORDER BY`, and `LIMIT` row envelope.
- In the default `utf8mb4_0900_ai_ci` context, admitted string comparisons for
  `NULLIF()` are case-insensitive for ASCII text. For example, `NULLIF('A',
  'a')` returns `NULL`.
- Successful supported projections produce no warnings for the admitted value
  shapes and make a following `ROW_COUNT()` return `-1`.
- MySQL also supports these functions in predicates, ordering, DML assignments,
  grouping, subqueries, generated expressions, and broad expression trees. Those
  positions remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` returns the existing row result
  object, with labels from source spans or aliases.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: existing function nodes and wrong-arity diagnostic nodes are
  reused. No MySQL grammar text is copied.
- Analyzer/planner: detects supported row control-flow expressions in `SELECT`
  projection, resolves descriptor columns from MyLite catalog descriptors, and
  rejects unsupported expression shapes before SQLite SQL is generated.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite physical execution: supported expressions lower to generated SQLite
  scalar expressions over stable physical table names and quoted physical
  column names. MyLite-owned literal/session values are bound parameters. No
  SQLite fork patch is required.
- Result builder: uses the existing row-scalar result conventions. This phase
  does not add protocol-grade expression metadata.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms continue to be owned by the existing scalar path:

```sql
SELECT control_expr[, control_expr ...]
SELECT control_expr[, control_expr ...] FROM DUAL
DO control_expr[, control_expr ...]
```

This phase adds descriptor-backed table forms when the select list contains at
least one row control-flow expression:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted projection subset is:

```sql
row_scalar_expr:
    row_scalar_value
  | IF ( row_condition , row_scalar_value , row_scalar_value )
  | IFNULL ( row_scalar_value , row_scalar_value )
  | COALESCE ( row_scalar_value_list )
  | NULLIF ( row_scalar_value , row_scalar_value )
  | ISNULL ( row_scalar_value )
  | ( row_scalar_expr )

row_scalar_value:
    descriptor_column_reference
  | string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | nested_row_control_expr
  | ( row_scalar_value )

row_condition:
    descriptor_integer_column_reference
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ISNULL ( row_scalar_value )
  | ( row_condition )

nested_row_control_expr:
    IF ( nested_row_condition , row_scalar_leaf_value , row_scalar_leaf_value )
  | IFNULL ( row_scalar_leaf_value , row_scalar_leaf_value )
  | COALESCE ( row_scalar_leaf_value_list )
  | NULLIF ( row_scalar_leaf_value , row_scalar_leaf_value )
  | ISNULL ( row_scalar_leaf_value )
  | ( nested_row_control_expr )

nested_row_condition:
    descriptor_integer_column_reference
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ( nested_row_condition )

row_scalar_leaf_value:
    descriptor_column_reference
  | string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | ( row_scalar_leaf_value )

row_scalar_value_list:
    row_scalar_value
  | row_scalar_value_list , row_scalar_value

row_scalar_leaf_value_list:
    row_scalar_leaf_value
  | row_scalar_leaf_value_list , row_scalar_leaf_value
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may name invisible columns. Supported value descriptor families are:

- integer-family columns;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`.

The admitted table-backed expression tree is intentionally bounded: a
top-level row control-flow function may contain one nested row control-flow
function layer, and that nested layer may contain only leaf descriptor/literal,
session scalar, or system-variable values. Further nested control-flow
functions and nested non-control-flow row functions are rejected
deterministically.

`IF()` conditions are deliberately narrower: they accept integer-family
descriptor columns, integer/boolean/`NULL` literals, and row-backed `ISNULL()`
results. String truth conversion, decimal truth conversion, warning-producing
numeric conversion, and arbitrary expression truthiness are deferred.

The following remain outside this phase:

- functions in `WHERE`, `HAVING`, `ORDER BY`, `GROUP BY`, aggregate arguments,
  distinct keys, DML assignment values, generated/default expressions, and
  check constraints;
- joined sources, derived tables, CTEs, scalar/table subqueries in table-backed
  control-flow arguments, user variables, parameters, stored functions, and
  arbitrary expression operands;
- exact `DECIMAL`, binary-string, `BIT`, `ENUM`, `SET`, `JSON`, and
  approximate numeric column operands;
- MySQL's complete result type aggregation and expression metadata;
- non-ASCII/full-Unicode collation parity.

### MyLite Lemon-Syntax Snippet

The parser already admits the function nodes. The runtime acceptance grammar for
this phase is independently authored as:

```lemon
row_scalar_expr(A) ::= row_scalar_value(B).
row_scalar_expr(A) ::= IF(T) LPAREN row_condition(C) COMMA row_scalar_value(V)
                       COMMA row_scalar_value(F) RPAREN(R).
row_scalar_expr(A) ::= IFNULL(T) LPAREN row_scalar_value(V)
                       COMMA row_scalar_value(F) RPAREN(R).
row_scalar_expr(A) ::= COALESCE(T) LPAREN row_scalar_value_list(V) RPAREN(R).
row_scalar_expr(A) ::= NULLIF(T) LPAREN row_scalar_value(L)
                       COMMA row_scalar_value(RV) RPAREN(R).
row_scalar_expr(A) ::= ISNULL(T) LPAREN row_scalar_value(V) RPAREN(R).
row_scalar_expr(A) ::= LPAREN row_scalar_expr(B) RPAREN(R).
row_condition(A) ::= qualified_identifier(B).
row_condition(A) ::= INTEGER(T).
row_condition(A) ::= PLUS(P) INTEGER(T).
row_condition(A) ::= MINUS(M) INTEGER(T).
row_condition(A) ::= TRUE(T).
row_condition(A) ::= FALSE(T).
row_condition(A) ::= NULL(T).
row_condition(A) ::= ISNULL(T) LPAREN row_scalar_value(V) RPAREN(R).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Semantics

Planning proceeds as follows:

1. Detect row-scalar projection attempts when a supported `SELECT` list contains
   `IF()`, `IFNULL()`, `COALESCE()`, `NULLIF()`, or `ISNULL()`.
2. Resolve the table source through the existing selected/default schema policy.
3. Load MyLite table and column descriptors and resolve descriptor column
   references against those descriptors, not SQLite metadata.
4. Convert admitted literal and session scalar arguments into bound
   `planned_value` parameters.
5. Generate SQLite expression SQL and bind all MyLite-owned values before
   execution.
6. Reuse existing descriptor-driven `WHERE`, `ORDER BY`, and `LIMIT` planning
   for the row envelope.

Function semantics:

- `IF()` tests the condition with the admitted integer truth rule: nonzero and
  not `NULL` selects the true branch; zero or `NULL` selects the false branch.
- `IFNULL()` and `COALESCE()` use the first non-`NULL` admitted argument.
- `NULLIF()` compares the two admitted arguments. When the planned argument
  domain is nonbinary text, comparison uses MyLite's registered ASCII
  `utf8mb4_0900_ai_ci` SQLite collation; integer-family comparisons use SQLite
  numeric equality over the stored integer values. Mixed unsupported domains
  are rejected.
- `ISNULL()` returns integer `1` or `0`.

Table-backed evaluation stays inside SQLite's row scan. MyLite must not copy the
matched row set into memory to evaluate these functions. SQLite may evaluate
expressions over each row, while MyLite remains responsible for parsing,
descriptor resolution, diagnostics, and value binding.

## SQLite Lowering

Generated SQL uses standard SQLite expression shapes plus MyLite's registered
collation:

```sql
CASE WHEN <condition> IS NOT NULL AND <condition> <> 0 THEN <true> ELSE <false> END
ifnull(<value>, <fallback>)
coalesce(<value>, ...)
CASE WHEN <left> COLLATE "utf8mb4_0900_ai_ci" = <right> COLLATE "utf8mb4_0900_ai_ci"
     THEN NULL ELSE <left> END
(<value> IS NULL)
```

Generated SQL must:

- reference stable physical table names such as `_mylite_user_table_<id>`;
- quote all generated SQLite identifiers;
- bind every MyLite-owned scalar literal/session value with prepared-statement
  parameters;
- keep reserved `_mylite_*` user names rejected by existing table-resolution
  policy before SQL generation.

## Diagnostics

Supported diagnostics include:

- existing parser diagnostics and MySQL-compatible native-function arity errors
  for malformed function calls;
- missing default schema, unknown schema, unknown table, reserved table names,
  and unsupported source clauses through existing row-scalar planning;
- unknown descriptor columns using MySQL-compatible unknown-column diagnostics
  in field-list context;
- deterministic unsupported diagnostics for unsupported condition/value
  descriptor types, unsupported expression operands, scalar subqueries in
  table-backed arguments, functions in predicates/order/group/having, DML
  assignment expressions, parameters, user variables, joins, and broader
  expression trees;
- allocation failures through `MYLITE_NOMEM`;
- physical SQLite failures through existing physical row diagnostics.

## Tests

Add MySQL-runtime expectation coverage for:

- table-backed `IFNULL()`, `COALESCE()`, `NULLIF()`, `ISNULL()`, and `IF()`;
- string, integer, date, datetime, and `NULL` values;
- case-insensitive ASCII string comparison for `NULLIF()` under the default
  collation;
- `WHERE`, descriptor `ORDER BY`, and `LIMIT` row-envelope preservation;
- result labels and aliases;
- `ROW_COUNT()` and `@@warning_count` after successful projections.

Add fast C runtime coverage for:

- supported table-backed projection values and aliases;
- nullable and nonnullable descriptor values;
- qualified column references with table aliases;
- existing row-envelope reuse;
- unknown columns inside function arguments;
- unsupported expression positions: `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`,
  and `UPDATE` assignment;
- unsupported argument types: binary string, `BIT`, `ENUM`, `SET`, `JSON`,
  approximate numeric columns, scalar subqueries, parameters, user variables,
  and nested non-control-flow row functions where deferred;
- reopen persistence and `.mylite` preamble preservation for read-only
  projection.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` control-flow and query-expression rows for the exact
  limited row-backed projection subset;
- `docs/compatibility/functions-control-flow.md`;
- `docs/compatibility/functions-comparison.md` for row-backed `ISNULL()`;
- `docs/compatibility/sql-query-expressions.md` projection-list notes.

Do not overclaim predicates, assignments, order expressions, grouping,
subqueries, joins, broad truth conversion, full type aggregation, or expression
metadata.
