# Baseline String Search Functions

## Summary

This phase adds a narrow MySQL string-position family:

```sql
LOCATE(substr, str)
LOCATE(substr, str, pos)
INSTR(str, substr)
POSITION(substr IN str)
```

The supported surface matches the current row-scalar string-function envelope:
no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projection. It also admits the same descriptor-backed
single-table `WHERE` truth/comparison/`IS [NOT] NULL`/`[NOT] BETWEEN`
predicates and non-grouped `ORDER BY` expression contexts used by the
row-scalar expression baseline. It does not add DML assignment values,
generated columns, defaults, or a general expression engine.

For this baseline, MyLite implements MySQL's one-based position behavior over
ASCII nonbinary text values using the existing ASCII `utf8mb4_0900_ai_ci`
collation approximation. Non-ASCII collation folding and binary-string
case-sensitive search are deferred.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-substring-functions/specs.md`
  - `docs/specs/baseline-string-case-functions/specs.md`
- Official MySQL 8.4 Reference Manual, string functions and operators:
  - <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_string_search_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `LOCATE(substr, str)`, `INSTR(str, substr)`, and
  `POSITION(substr IN str)` return the one-based position of the first match.
- `LOCATE(substr, str, pos)` starts searching at one-based `pos`.
- A missing substring returns `0`.
- Any `NULL` argument returns `NULL`.
- `LOCATE('', str)` returns `1`; `LOCATE('', str, pos)` returns `pos` when
  `1 <= pos <= CHAR_LENGTH(str) + 1` and `0` otherwise.
- `LOCATE(substr, str, pos)` returns `0` for `pos <= 0` or for `pos` beyond
  the end plus one.
- `INSTR(str, substr)` is the same operation as two-argument `LOCATE()` with
  the first two arguments reversed.
- `POSITION(substr IN str)` is a syntax synonym for two-argument `LOCATE()`;
  MySQL rejects `POSITION (` with whitespace between the function name and
  left parenthesis in default SQL mode.
- `LOCATE()` and `INSTR()` allow whitespace before `(` in default SQL mode.
- Default `utf8mb4_0900_ai_ci` matching is case-insensitive. MyLite admits
  ASCII values for this phase and applies ASCII case folding only.
- Numeric and boolean arguments are converted to visible string form, so
  `LOCATE(23, 12345)`, `INSTR(12345, 23)`, and
  `POSITION(23 IN 12345)` return `2`.
- `LOCATE('2', 12345, TRUE)` returns `2`; with `FALSE`, it returns `0`.
- Successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.
- Wrong-arity `LOCATE()` and `INSTR()` calls fail with `1582 / 42000`.
  Wrong-shape `POSITION()` calls are syntax errors.

MySQL also accepts broader behavior such as noninteger argument rounding,
string-to-number position conversion, binary-string case-sensitive matching,
full Unicode collation folding, grouped expression ordering, arbitrary nested
function arguments, and DML assignment expressions. Those forms remain outside
this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text integers or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits the exact function forms and preserves source spans
  for labels and diagnostics. `LOCATE()` and `INSTR()` wrong arity is routed to
  native-function parameter-count diagnostics. Unsupported `POSITION()` shapes
  remain syntax errors.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections lower to generated SQLite
  expressions over stable physical table names and quoted physical column names.
  MyLite registers a narrow scalar SQLite helper for ASCII, case-insensitive,
  one-based search because SQLite's built-in `instr()` is not a MySQL
  collation-equivalent `LOCATE()` replacement.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT string_search_item[, string_search_item ...]
SELECT string_search_item[, string_search_item ...] FROM DUAL
```

`DO` form:

```sql
DO string_search_expr[, string_search_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
string-search function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE descriptor_backed_row_scalar_predicate]
[ORDER BY descriptor_column_or_row_scalar_expression [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
string_search_expr:
    LOCATE ( string_search_value , string_search_value )
  | LOCATE ( string_search_value , string_search_value , string_search_position )
  | INSTR ( string_search_value , string_search_value )
  | POSITION ( string_search_value IN string_search_value )

string_search_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | descriptor_column_reference        -- table-backed SELECT only
  | ( string_search_value )

string_search_position:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | supported_integer_scalar_function
  | descriptor_integer_column_reference        -- table-backed SELECT only
  | supported_integer_row_scalar_expression    -- table-backed SELECT only
  | ( string_search_position )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for string arguments match the current string-slice
subset:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

The `pos` argument for three-argument `LOCATE()` admits signed-64 integer,
boolean, and `NULL` scalar values, direct supported numeric and string-length scalar
functions, descriptor integer columns, and the existing supported table-backed
integer row-scalar expression subset. That row-scalar subset includes integer
arithmetic over admitted operands, supported integer-domain numeric functions, string-length
functions, `UNIX_TIMESTAMP()`, and numeric temporal extractors.
Warning-producing string/noninteger conversion, binary string values, `BIT`,
approximate numeric values, `ENUM`, `SET`, `JSON`, spatial values, parameters,
user variables, and full Unicode collation behavior are deferred.

The following remain outside this phase:

- `HAVING LOCATE(...) ...`, grouped or aggregate expression `ORDER BY`,
  grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = LOCATE('x', v)`;
- arbitrary nested row functions outside the supported row-scalar value and
  integer argument subsets;
- scalar subqueries, correlated subqueries, CTEs, parameters, user variables,
  and stored functions;
- string introducers, national strings, arbitrary binary literals as scalar
  arguments, binary casts as arguments, and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= LOCATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
expression(A) ::= LOCATE(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) RPAREN(R).
expression(A) ::= INSTR(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
expression(A) ::= POSITION(T) LPAREN(L) expression(B) IN expression(C) RPAREN(R).
row_scalar_numeric_predicate_expression(A) ::= LOCATE(T) LPAREN expression(B)
                  COMMA expression(C) RPAREN(R).
row_scalar_numeric_predicate_expression(A) ::= LOCATE(T) LPAREN expression(B)
                  COMMA expression(C) COMMA expression(D) RPAREN(R).
row_scalar_numeric_predicate_expression(A) ::= INSTR(T) LPAREN expression(B)
                  COMMA expression(C) RPAREN(R).
row_scalar_numeric_predicate_expression(A) ::= POSITION(T) LPAREN(L)
                  expression(B) IN expression(C) RPAREN(R).
```

Wrong-arity `LOCATE()` and `INSTR()` forms produce function-argument-count AST
nodes so runtime can emit the native-function parameter-count diagnostic.
Unsupported `POSITION()` shapes remain syntax errors. These snippets describe
MyLite's admitted subset, not MySQL's full grammar.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the admitted `substr` and `str` arguments to text:
   - ordinary string literal: decoded NUL-free ASCII bytes;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - supported session scalar value: its existing text result or `NULL`.
3. Convert the admitted `pos` argument for three-argument `LOCATE()` through
   the supported integer argument envelope when non-`NULL`.
4. If any argument is `NULL`, return SQL `NULL`.
5. Reject NUL-containing or non-ASCII text values with a deterministic
   MyLite diagnostic for this slice.
6. Search using ASCII case-insensitive comparison.
7. Return a decimal text integer result.

Table-backed evaluation uses generated SQLite SQL around a MyLite scalar helper:

```sql
_mylite_locate_ascii_ci(substr, str, pos)
```

Two-argument `LOCATE()` binds `pos = 1`. `INSTR(str, substr)` and
`POSITION(substr IN str)` are lowered by swapping or preserving arguments into
the same helper. Literal and session scalar values are bound parameters, not
interpolated SQL. Descriptor columns are quoted stable physical column
references, and integer `pos` expressions use the supported row-scalar integer
SQL emitters.

Supported successful calls return zero warnings. Unsupported expression shapes
use the existing unsupported scalar/row-scalar expression diagnostics unless a
more specific native-function count or ASCII-only diagnostic applies.

## Tests

Add a fast C runtime test, preferably
`packages/libmylite/tests/runtime_string_search_functions_test.c`, registered
as `libmylite.runtime.string_search_functions`. Coverage must include:

- parser coverage for `LOCATE`, three-argument `LOCATE`, `INSTR`, and
  `POSITION(substr IN str)`;
- scalar `SELECT`, `FROM DUAL`, and `DO`;
- default labels and explicit aliases;
- successful one-based positions, misses, empty substring behavior, `pos`
  boundaries, signed positions, `NULL`, integer, and boolean conversion;
- table-backed row-scalar projection over descriptor integer, `DECIMAL`,
  temporal, `CHAR`, `VARCHAR`, and `TEXT` values;
- descriptor-backed `WHERE` truth, comparison, `IS [NOT] NULL`, and
  `[NOT] BETWEEN` predicates plus non-grouped single-table `ORDER BY`
  expression contexts;
- schema/table resolution and existing row envelope behavior inherited from
  the row-scalar path;
- wrong arity for `LOCATE()` and `INSTR()`;
- unsupported `POSITION` whitespace and wrong-shape syntax;
- supported integer `pos` expressions;
- unsupported nested functions outside the supported value/integer subsets,
  grouped expression ordering, DML assignment values, binary strings, non-ASCII
  values, unsupported `pos` operands, parameters, and general expressions;
- MySQL expectation script coverage for user-visible results, errors,
  `ROW_COUNT()`, and `@@warning_count`.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-string.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/type-system-literals-conversion.md` for exactly the
limited string-search subset.

Do not claim full Unicode collation behavior, binary-string matching,
predicates, DML assignment values, expression ordering, noninteger conversion,
parameters, general expression support, or optimizer behavior.

## Verification

Before marking the feature done:

1. `cmake --build --preset dev`
2. Focused parser/runtime CTests for parser and string-search functions.
3. `packages/libmylite/tests/mysql_baseline_string_search_functions_expectations.sh`
4. `cmake --workflow --preset check`
