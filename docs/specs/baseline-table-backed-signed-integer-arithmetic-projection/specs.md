# Baseline Table-Backed Signed Integer Arithmetic Projection

## Summary

This phase adds the first table-backed arithmetic projection slice for
descriptor-backed `SELECT` statements:

```sql
SELECT integer_arithmetic_item[, integer_arithmetic_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted arithmetic domain is signed 64-bit integer arithmetic over
descriptor columns whose logical type is a signed MySQL integer family type,
decimal integer literals with optional unary sign, `TRUE`, `FALSE`, and `NULL`.
The supported operators are binary `+`, binary `-`, and `*`, with parentheses
and MySQL precedence. Results are produced per matched source row without
materializing the row set in MyLite memory.

This is not a general expression engine. It deliberately excludes unsigned
descriptor columns, `/`, `DIV`, `%`, `MOD`, unary operators over columns or
compound expressions, strings, decimals, floats, hex, bit, temporal values,
casts, functions, parameters, user variables, subqueries, predicates or order
keys built from expressions, DML assignments, generated expressions, joins,
CTEs, and arbitrary SQLite pass-through.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing MyLite expression slices:
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-scalar-arithmetic-projection/specs.md`
  - `docs/specs/baseline-scalar-unary-arithmetic-projection/specs.md`
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
  - `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - expressions: <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - out-of-range and overflow handling:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_table_backed_signed_integer_arithmetic_projection_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL Runtime Observations

Runtime probes against MySQL 8.4.9 establish the expectations for the admitted
subset:

- `SELECT a+b`, `a-b`, and `a*b` from a table evaluate once per matched row.
- `*` binds tighter than `+` and `-`; same-precedence operators evaluate left
  to right; parentheses override precedence.
- Decimal integer literals may appear on either side of a supported operator,
  including optional unary `+` and unary `-` on the literal.
- `TRUE` behaves as `1`, `FALSE` behaves as `0`, and any arithmetic operation
  with a `NULL` operand returns `NULL`.
- The existing table row envelope still applies: `WHERE` filters rows before
  projection evaluation, `ORDER BY` orders the result rows, and `LIMIT` limits
  visible result rows.
- A filtered statement with no result rows does not evaluate a would-be
  overflowing projection expression.
- Supported in-range expressions produce `@@warning_count = 0` and a following
  `ROW_COUNT()` value of `-1`.
- Signed integer overflow raises `1690 / 22003`.

MySQL accepts broader arithmetic over unsigned integer columns, decimal and
string conversion, division, modulo, scalar functions, subqueries, and
expression use in other clauses. Those behaviors remain deferred.

## Ownership Boundaries

- Public API: no ABI changes. `mylite_execute()` continues to own result
  handles, diagnostics, public misuse validation, and statement lifetime.
- Statement context: successful supported `SELECT` statements use existing
  row-result conventions: one result object, zero affected rows, statement
  warning count, and following `ROW_COUNT()` state `-1`.
- Lexer/parser/AST: no new tokens are required. Existing binary-expression,
  parenthesized-expression, signed-literal, boolean, `NULL`, identifier, alias,
  and table-source AST nodes are reused. Parser acceptance remains wider than
  runtime support.
- Analyzer/planner: detects table-backed arithmetic projection attempts,
  resolves table and column names through MyLite descriptors, rejects
  unsupported expression shapes before SQLite SQL is generated, and creates a
  row-scalar expression plan.
- Catalog: read-only. The feature must not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or `sqlite_schema_generation`.
- SQLite physical execution: MyLite lowers supported arithmetic to generated
  SQLite projection SQL over stable physical table names and quoted descriptor
  column names. Literal operands are bound parameters. Checked arithmetic is
  implemented with MyLite-registered SQLite scalar functions through public
  SQLite extension APIs, not a SQLite fork patch.
- Result builder: existing row-scalar result building supplies aliases,
  descriptor column labels, or source-span expression labels and reads SQLite
  result values through the current result API.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

Supported table-backed statement shape:

```sql
SELECT integer_arithmetic_item[, integer_arithmetic_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

Each arithmetic item may use the existing alias surface:

```sql
integer_arithmetic_item:
    integer_arithmetic_expr
  | integer_arithmetic_expr AS alias
  | integer_arithmetic_expr alias
```

The admitted expression subset is:

```sql
integer_arithmetic_expr:
    integer_arithmetic_operand
  | integer_arithmetic_expr + integer_arithmetic_expr
  | integer_arithmetic_expr - integer_arithmetic_expr
  | integer_arithmetic_expr * integer_arithmetic_expr
  | ( integer_arithmetic_expr )

integer_arithmetic_operand:
    signed_integer_descriptor_column
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
```

`signed_integer_descriptor_column` may be unqualified, table-qualified,
schema-qualified, or source-alias-qualified according to the existing
single-source `SELECT` table alias policy. It must resolve to a descriptor
column with one of these logical types:

- `TINYINT`
- `TINYINT(1)`
- `SMALLINT`
- `MEDIUMINT`
- `INT`
- `INTEGER`
- `BIGINT`

Unsigned integer logical types are intentionally deferred even when their
stored values are inside MyLite's current signed physical range, because MySQL
uses unsigned expression result rules by default.

### MyLite Lemon-Syntax Snippet

No parser production is added for this phase. MyLite already parses the
relevant expression forms. Runtime admission for table-backed arithmetic uses
this independently authored subset:

```lemon
table_integer_arithmetic(A) ::= table_integer_additive(B).

table_integer_additive(A) ::= table_integer_additive(B)
                              PLUS table_integer_multiplicative(C).
table_integer_additive(A) ::= table_integer_additive(B)
                              MINUS table_integer_multiplicative(C).
table_integer_additive(A) ::= table_integer_multiplicative(B).

table_integer_multiplicative(A) ::= table_integer_multiplicative(B)
                                    STAR table_integer_primary(C).
table_integer_multiplicative(A) ::= table_integer_primary(B).

table_integer_primary(A) ::= qualified_identifier(B).
table_integer_primary(A) ::= INTEGER(T).
table_integer_primary(A) ::= PLUS INTEGER(T).
table_integer_primary(A) ::= MINUS INTEGER(T).
table_integer_primary(A) ::= TRUE(T).
table_integer_primary(A) ::= FALSE(T).
table_integer_primary(A) ::= NULL(T).
table_integer_primary(A) ::= LPAREN table_integer_arithmetic(B) RPAREN.
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Semantics

Planning proceeds as follows:

1. Detect table-backed row-scalar projection attempts when a select item
   contains a supported or unsupported arithmetic operator.
2. Resolve the table source through the existing selected/default schema and
   table alias policy.
3. Resolve arithmetic descriptor columns against MyLite column descriptors, not
   SQLite schema text.
4. Reject unsigned, non-integer, unknown, ambiguous, or unavailable descriptor
   columns before physical SQL generation.
5. Convert literal operands into MyLite-owned bound integer or `NULL` values.
6. Generate SQLite projection SQL that calls MyLite registered checked
   arithmetic functions for `+`, `-`, and `*`.
7. Reuse the existing descriptor-driven `WHERE`, `ORDER BY`, and `LIMIT`
   planning for the row envelope.

Arithmetic semantics:

- non-`NULL` operands are signed 64-bit integers;
- `TRUE` is `1` and `FALSE` is `0`;
- any binary arithmetic operation with a `NULL` operand returns `NULL`;
- binary `+`, binary `-`, and `*` use checked signed 64-bit arithmetic;
- results outside `[-9223372036854775808, 9223372036854775807]` return
  `1690 / 22003`;
- supported in-range expressions produce `warning_count == 0`;
- `ORDER BY` without expression keys and `LIMIT` semantics remain exactly the
  existing descriptor-backed `SELECT` behavior; and
- no catalog or descriptor metadata is changed.

## Generated SQLite SQL Shape

The physical SQL uses generated physical table names, quoted descriptor column
names, and bound parameters:

```sql
SELECT _mylite_i64_add("a", ?1), _mylite_i64_mul("b", ?2)
FROM "_mylite_user_table_<table_id>"
WHERE <descriptor predicate>
ORDER BY "id" ASC
LIMIT ?3
```

Exact generated function names are internal. They are registered through
`sqlite3_create_function_v2()` using MyLite's existing registration wrapper.
The functions return SQL `NULL` if either input is `NULL`, otherwise an SQLite
integer result. Overflow reports an SQLite scalar-function error string that
the row-scalar executor maps to MyLite's `1690 / 22003` diagnostic.

No SQLite fork patch is required.

## Diagnostics

Supported MySQL-compatible diagnostics:

- syntax errors retain existing parser diagnostics;
- missing default schema, unknown schema, unknown table, reserved `_mylite_*`
  names, unsupported object kinds, unknown columns, and ambiguous columns reuse
  existing descriptor-driven `SELECT` diagnostics;
- signed arithmetic overflow returns `1690 / 22003`;
- allocation failure returns existing `MYLITE_NOMEM` diagnostics; and
- physical SQLite prepare, bind, or unexpected row failures retain existing
  internal SQLite diagnostics.

MyLite-specific unsupported diagnostics are used for:

- unsigned integer descriptor columns;
- non-integer descriptor columns;
- unary operators over columns or compound expressions;
- `/`, `DIV`, `%`, `MOD`, bitwise, comparison, logical, JSON, interval, or
  other expression operators;
- string, decimal, float, hex, bit, temporal, JSON, function, cast, parameter,
  user-variable, system-variable, and subquery operands;
- expression predicates, order keys, grouping keys, aggregate arguments, DML
  assignments, defaults, generated expressions, joins, CTEs, and arbitrary
  SQLite pass-through.

## Tests

Fast C tests should cover:

- table-backed `+`, binary `-`, and `*` over signed `INT`, `INTEGER`, and
  `BIGINT` descriptor columns;
- literal-on-left and literal-on-right operands, signed decimal integer
  literals, booleans, and `NULL`;
- precedence, left-to-right same-precedence evaluation, and parentheses;
- `WHERE`, `ORDER BY`, and `LIMIT` interaction without changing their existing
  descriptor semantics;
- aliases, default source-span labels, warning count, row count, and result row
  shape;
- `NULL` propagation;
- signed 64-bit boundary success and overflow diagnostics;
- deterministic rejection of unsigned columns, non-integer columns, unsupported
  operators, unary column signs, function operands, column-to-string coercion,
  subqueries, parameters, expression order keys, and expression predicates;
- reopen persistence safety and unchanged file preamble behavior through
  read-only queries; and
- preservation of existing parser, row-scalar, scalar arithmetic, lifecycle,
  result, statement-context, storage, and VFS tests.

Focused verification:

1. `packages/libmylite/tests/mysql_baseline_table_backed_signed_integer_arithmetic_projection_expectations.sh`
2. `ctest --preset dev -R 'libmylite\\.(parser|runtime\\.row_scalar_expressions|runtime\\.scalar_arithmetic_projection|runtime\\.select_order_limit_lifecycle)' --output-on-failure`
3. `cmake --workflow --preset check`

## Compatibility Notes

This phase moves a narrow part of table-backed projection from unsupported to
limited support. It does not change no-source scalar arithmetic, row-scalar
function argument domains, predicates, ordering expressions, DML assignment
expressions, expression metadata, or unsigned arithmetic behavior.
