# Baseline SUBSTRING_INDEX Function

## Summary

This phase adds a narrow MySQL-compatible delimiter-count string slice:

```sql
SUBSTRING_INDEX(str, delim, count)
```

The supported surface matches the current row-scalar string-function envelope:
no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projection. It does not add expression predicates,
expression ordering, DML assignment values, generated columns, defaults, or a
general expression engine.

Core behavior:

- exactly three arguments are required;
- any `NULL` argument returns `NULL`;
- `count = 0` returns the empty string;
- empty `delim` returns the empty string;
- positive `count` returns bytes to the left of the `count`th non-overlapping
  delimiter occurrence from the left;
- negative `count` returns bytes to the right of the `abs(count)`th
  non-overlapping delimiter occurrence from the right;
- fewer delimiter occurrences than requested returns the original string;
- delimiter matching is case-sensitive;
- successful supported calls produce no warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing string-function baselines:
  - `docs/specs/baseline-substring-functions/specs.md`
  - `docs/specs/baseline-string-search-functions/specs.md`
  - `docs/specs/baseline-string-replace-function/specs.md`
- Official MySQL 8.4 Reference Manual, string functions and operators:
  - <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_substring_index_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `SUBSTRING_INDEX('www.mysql.com', '.', 2) = 'www.mysql'`.
- `SUBSTRING_INDEX('www.mysql.com', '.', -2) = 'mysql.com'`.
- Missing delimiters return the original value for positive and negative
  counts.
- `SUBSTRING_INDEX('abc', '.', 0)` and
  `SUBSTRING_INDEX('abc', '', 1)` return the empty string without warnings.
- `SUBSTRING_INDEX(NULL, '.', 1)`, `SUBSTRING_INDEX('abc', NULL, 1)`, and
  `SUBSTRING_INDEX('abc', '.', NULL)` return `NULL`.
- Matching is case-sensitive under the default `utf8mb4_0900_ai_ci` collation:
  `SUBSTRING_INDEX('AaA', 'a', 1) = 'A'`.
- Multibyte text is preserved when the delimiter boundary is found:
  `SUBSTRING_INDEX('é/🙂/x', '/', 2) = 'é/🙂'`.
- Multi-byte delimiters are counted as non-overlapping byte sequences:
  `SUBSTRING_INDEX('aaaa', 'aa', 2) = 'aa'` and
  `SUBSTRING_INDEX('aaaa', 'aa', -2) = 'aa'`.
- Numeric and boolean `str` / `delim` arguments are converted to visible string
  form, so `SUBSTRING_INDEX(12345, '2', 1) = '1'`.
- `TRUE` and `FALSE` count arguments behave as `1` and `0`.
- Whitespace before `(` is accepted for this function in default SQL mode.
- Wrong-arity calls fail with `1582 / 42000` and MySQL's native-function
  parameter-count diagnostic.
- Successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

MySQL also accepts deferred behavior such as decimal count rounding, string
numeric count conversion, out-of-range count conversion with warning `1292`,
binary-string result typing, predicates over `SUBSTRING_INDEX()` expressions,
nested functions, and arbitrary expression arguments. Those forms remain
outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits the exact three-argument form and preserves source
  spans for labels and diagnostics. Wrong arity is routed to native-function
  parameter-count diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections lower to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite registers a narrow scalar SQLite helper because SQLite has no
  built-in MySQL-compatible `SUBSTRING_INDEX()` equivalent.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT substring_index_item[, substring_index_item ...]
SELECT substring_index_item[, substring_index_item ...] FROM DUAL
```

`DO` form:

```sql
DO substring_index_expr[, substring_index_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
`SUBSTRING_INDEX()` function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
substring_index_expr:
    SUBSTRING_INDEX ( substring_index_value , substring_index_value , substring_index_count )

substring_index_value:
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
  | ( substring_index_value )

substring_index_count:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ( substring_index_count )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for string and delimiter arguments match the
current `REPLACE()` subset:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

The `count` argument is intentionally literal-only in this phase. Descriptor
count columns, session scalar count values, string numeric count conversion,
noninteger rounding, out-of-range count conversion warnings, binary string
values, `BIT`, approximate numeric values, `ENUM`, `SET`, `JSON`, spatial
values, and binary-string metadata are deferred.

The following remain outside this phase:

- `WHERE SUBSTRING_INDEX(column, '.', 1) ...`, `HAVING SUBSTRING_INDEX(...)`,
  expression `ORDER BY`, grouping, distinct expression rows, and aggregate
  arguments;
- DML assignment values such as
  `UPDATE t SET c = SUBSTRING_INDEX(v, '.', 1)`;
- nested row functions such as `SUBSTRING_INDEX(CONCAT(v, '-'), '-', 1)`;
- scalar subqueries, correlated subqueries, CTEs, parameters, user variables,
  and stored functions;
- string introducers, national strings, arbitrary binary literals as scalar
  arguments, binary casts as arguments, and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= SUBSTRING_INDEX(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION, B, C, D, R);
}

expression(A) ::= SUBSTRING_INDEX(T) LPAREN RPAREN(R).
expression(A) ::= SUBSTRING_INDEX(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= SUBSTRING_INDEX(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
expression(A) ::= SUBSTRING_INDEX(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) COMMA function_argument_list(E) RPAREN(R).
```

Wrong-arity forms produce a `SUBSTRING_INDEX` argument-count AST node so
runtime can emit MySQL's native-function parameter-count diagnostic. These
snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert `str` and `delim` to text:
   - ordinary string literal: decoded NUL-free bytes;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - supported session scalar value: its existing text result or `NULL`.
3. Convert `count` to a signed 64-bit integer literal value, or SQL `NULL`.
4. Return SQL `NULL` if any argument is `NULL`.
5. Return `''` for `count = 0`.
6. Return `''` for empty `delim`.
7. Search for exact, case-sensitive, non-overlapping delimiter byte sequences.
8. For positive `count`, return the prefix before the `count`th match or the
   original string when there are fewer matches.
9. For negative `count`, return the suffix after the `abs(count)`th match from
   the right or the original string when there are fewer matches.

Table-backed evaluation uses generated SQLite SQL around a MyLite scalar
helper:

```sql
_mylite_substring_index(str, delim, count)
```

Literal and session scalar values are bound parameters, not interpolated SQL.
Descriptor columns are quoted stable physical column references. The helper
uses public SQLite scalar-function APIs only; no SQLite fork hook is required.

Supported successful calls return zero warnings. Unsupported expression shapes
use the existing unsupported scalar/row-scalar expression diagnostics unless a
more specific native-function count diagnostic applies.

## Diagnostics

The phase preserves existing public API misuse handling. User-visible
diagnostics:

- wrong arity:
  `Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'`;
- unsupported scalar argument:
  `SUBSTRING_INDEX() supports only string, integer, boolean, NULL, session scalar, and system variable value arguments`;
- unsupported count argument:
  `SUBSTRING_INDEX() count supports only integer, boolean, and NULL literals`;
- out-of-range count literal:
  `SUBSTRING_INDEX() count literals must fit the signed 64-bit range`;
- embedded NUL in a scalar string literal:
  `SUBSTRING_INDEX() arguments do not support NUL bytes`;
- unknown descriptor column: existing MySQL-style unknown-column diagnostic
  with the correct clause context;
- unsupported descriptor kind: deterministic MyLite unsupported diagnostic
  naming the unsupported column family;
- physical SQLite, allocation, and internal failures: existing runtime,
  allocation, and SQLite diagnostics.

## Tests

Add a fast C runtime test by extending the existing string-slice runtime test,
plus parser coverage in `parser_test.c`. Coverage must include:

- parser coverage for the three-argument function, whitespace before `(`,
  `DO`, labels, and wrong-arity argument-count nodes;
- scalar `SELECT`, `FROM DUAL`, and `DO`;
- positive, negative, zero, missing delimiter, empty delimiter, `NULL`,
  boolean count, and signed count behavior;
- case-sensitive matching, multibyte preserved text, and non-overlapping
  multi-byte delimiter counting;
- table-backed row-scalar projection over descriptor integer, `DECIMAL`,
  temporal, `CHAR`, `VARCHAR`, and `TEXT` values;
- descriptor delimiter columns;
- existing row envelope behavior: schema/table resolution, `WHERE`, descriptor
  `ORDER BY`, and descriptor `LIMIT`;
- default labels and explicit aliases;
- reopen persistence and `.mylite` preamble preservation inherited from the
  string-slice runtime test;
- diagnostics for wrong arity, unknown columns, unsupported count expressions,
  unsupported nested row functions, binary and approximate descriptor columns,
  hex/binary/parameter scalar arguments, predicates, ordering expressions, and
  DML assignment use;
- MySQL-runtime expectation script coverage for every newly admitted
  user-visible behavior and for MySQL-accepted but deferred behavior.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-string.md`.

No `docs/compatibility/sql-table-dml.md`, `operators.md`, or literal/conversion
detail changes are needed because this phase does not add predicate, DML,
operator, or general literal-conversion surface.

