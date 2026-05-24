# Baseline ELT Function

## Summary

This phase adds a narrow `ELT()` scalar-function slice. `ELT(index, value1,
value2, ...)` returns the argument at the 1-based `index` position after the
index argument. The first implementation is deliberately limited to top-level
no-source, `FROM DUAL`, and `DO` expressions over scalar literals.

The slice is a companion to the existing `FIELD()` support, but it does not add
`ELT()` ordering, predicates, table-backed row-scalar expressions, DML
assignment expressions, or general expression nesting.

## Compatibility Authority

Authoritative inputs:

- MySQL 8.4 Reference Manual, "String Functions and Operators":
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- Observed MySQL 8.4.9 runtime behavior from the local `mysql:8.4.9`
  comparison container.

Runtime probes verified:

```sql
DO 0;
SELECT ELT(1,'Aa','Bb','Cc'), ELT(3,'Aa','Bb','Cc'),
       ELT(0,'Aa'), ELT(-1,'Aa'), ELT(4,'Aa','Bb'),
       ELT(NULL,'Aa'), ELT(TRUE,'no','yes'), ELT(FALSE,'zero'),
       ELT(1,10,TRUE,NULL), ELT(2,10,TRUE,NULL),
       ELT(3,10,TRUE,NULL), @@warning_count;
SHOW WARNINGS;
SELECT ELT();
SELECT ELT(1);
```

Observed MySQL 8.4.9 behavior shaping this subset:

- `ELT()` and `ELT(1)` fail with `1582 / 42000`, incorrect native-function
  parameter count.
- At least two arguments are required: the index followed by at least one
  selectable value.
- The first argument is a 1-based selector. `0`, negative values, `NULL`, and
  indexes beyond the value count return SQL `NULL`.
- `TRUE` and `FALSE` indexes behave as `1` and `0`.
- Selected integer and boolean value arguments are returned as their visible
  scalar string forms, for example `10`, `1`, and `0`.
- A selected `NULL` value returns SQL `NULL`.
- The admitted integer/boolean/NULL and ASCII string literal cases produce no
  warnings.

## Ownership Boundary

- Public API: no ABI change. `mylite_execute()` exposes result sets,
  diagnostics, warning counts, and cleanup through the existing result API.
- Lexer/parser/AST: add an `ELT` keyword token and AST nodes for the function
  and native argument-count diagnostics. No broad expression grammar changes.
- Analyzer/runtime: evaluate admitted scalar literal arguments in MyLite and
  format the selected result. Unsupported argument shapes are rejected before
  any SQLite SQL is generated.
- Catalog/storage/VFS: no catalog, descriptor, file format, preamble, or VFS
  change.
- SQLite physical execution: no SQLite function registration or fork hook is
  required for this slice.

## Supported Syntax

MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= ELT(T) LPAREN function_argument_list(B) RPAREN(R).
expression(A) ::= ELT(T) LPAREN RPAREN(R).
identifier(A) ::= ELT(T).
```

Supported examples:

```sql
SELECT ELT(1, 'a', 'b');
SELECT ELT(+2, 'a', 'b') FROM DUAL;
SELECT ELT(TRUE, 10, 20), ELT(FALSE, 'x');
DO ELT(2, 'a', 'b'), ELT(NULL, 'a');
```

## Semantics

The admitted selector argument is one of:

- signed decimal integer literal with optional unary `+` or `-`, inside the
  signed 64-bit range;
- `TRUE`;
- `FALSE`;
- `NULL`.

The admitted value arguments are:

- ASCII ordinary string literals without embedded NUL bytes;
- signed decimal integer literals with optional unary `+` or `-`, inside the
  signed 64-bit range;
- `TRUE`;
- `FALSE`;
- `NULL`.

Result rules:

- `index` is 1-based over the value arguments.
- `index < 1`, `index > value_count`, or `index IS NULL` returns SQL `NULL`.
- Selecting a string returns its decoded string bytes.
- Selecting an integer or boolean returns its visible integer text.
- Selecting `NULL` returns SQL `NULL`.
- Successful admitted evaluations produce `warning_count == 0`.
- `DO ELT(...)` returns no rows and leaves `ROW_COUNT() = 0`.

## Diagnostics

Supported diagnostics:

- wrong argument count: MySQL-compatible `1582 / 42000`;
- unsupported index argument: deterministic parse error,
  `ELT() index supports only signed integer, boolean, and NULL literals`;
- signed selector out of range: deterministic parse error,
  `ELT() index literals must fit the signed 64-bit range`;
- unsupported value argument: deterministic parse error,
  `ELT() supports only string, integer, boolean, and NULL value arguments`;
- signed value out of range: deterministic parse error,
  `ELT() value integer literals must fit the signed 64-bit range`;
- non-ASCII string value: deterministic parse error,
  `ELT() string literals support only ASCII values`;
- embedded-NUL string value: existing string decoding diagnostic with
  `ELT() string literals do not support NUL bytes`;
- allocation failures: existing MyLite allocation diagnostics.

## Non-Goals

This feature does not implement:

- string, decimal, float, hex, bit, parameter, function, subquery, or column
  selector conversion;
- non-ASCII collation behavior, binary string values, national string values,
  or embedded-NUL result delivery;
- table-backed row-scalar `ELT()` projection;
- `ELT()` in predicates, ordering, grouping, DML assignments, defaults, or
  generated columns;
- nested `ELT()` or arbitrary expression arguments;
- SQLite fork patches.

## Tests

Tests cover:

- no-source and `FROM DUAL` scalar projection;
- whitespace between function name and `(`;
- `DO` execution and `ROW_COUNT()` / warning count;
- selector values `1`, last position, `0`, negative, out-of-range, `NULL`,
  `TRUE`, and `FALSE`;
- selected string, integer, boolean, and `NULL` value arguments;
- argument-count errors;
- deterministic unsupported diagnostics for unsupported selector/value
  arguments, mixed column references, integer overflow, and non-ASCII strings;
- parser AST coverage for normal and argument-count forms;
- MySQL 8.4.9 expectation script for the admitted user-visible behavior.

## Compatibility Gaps

MyLite still does not claim full `ELT()` compatibility. The main deferred
areas are MySQL's broader first-argument coercion, table-backed expression
evaluation, binary and non-ASCII string behavior, nested expressions,
predicates, ordering, and metadata-level charset/collation parity.
