# Baseline IP Address Functions

## Scope

This slice implements the MySQL 8.4.9 IPv4 scalar functions `INET_ATON(expr)`
and `INET_NTOA(expr)`.

The functions are documented in the MySQL 8.4 Reference Manual miscellaneous
functions section:

- <https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html>

MyLite supports these functions in:

- no-source `SELECT` scalar projections;
- `SELECT ... FROM DUAL`;
- `DO` expression statements;
- `SET @user_variable` assignments;
- source-free DML value contexts;
- source-backed row-scalar projection, `WHERE`, and `ORDER BY` contexts;
- nested row-scalar contexts already admitted by the current row-scalar planner.

`INET6_ATON()`, `INET6_NTOA()`, `IS_IPV4()`, `IS_IPV6()`, and related IPv6
helpers are out of scope for this slice.

## Syntax

`INET_ATON` and `INET_NTOA` are ordinary function names, not reserved words.
The parser recognizes the generic identifier-call form and specializes these
names to function-specific AST nodes:

```lemon
expression ::= IDENTIFIER LPAREN expression RPAREN.
expression ::= IDENTIFIER LPAREN RPAREN.
expression ::= IDENTIFIER LPAREN expression COMMA function_argument_list RPAREN.
```

When the identifier text is `INET_ATON` or `INET_NTOA`, exactly one argument
creates the corresponding function AST node. Zero arguments or more than one
argument create a native-function argument-count diagnostic node. Other
identifier calls continue to use the generic function placeholder path.

## Semantics

`INET_ATON(expr)` converts an IPv4 dotted address string to an unsigned integer
in network byte order. `NULL` returns `NULL`. Invalid input returns `NULL` and
appends warning `1411 HY000`.

MySQL 8.4.9 still accepts several legacy short forms even though the manual
warns that short-form behavior should not be relied on. Runtime probes define
this slice:

- one component is treated as `0.0.0.x`;
- two components are treated as `x.0.0.y`;
- three components are treated as `x.y.0.z`;
- four components are treated as `w.x.y.z`;
- empty non-trailing components are zero, so `.1`, `..1`, `1..2`, `1.2..4`,
  and `0..0` are accepted;
- a trailing dot, more than four components, signs, whitespace, nondigits, or
  a component greater than 255 are rejected.

`INET_NTOA(expr)` converts an integer in the inclusive range `0` through
`4294967295` to a dotted IPv4 string. `NULL` returns `NULL`. Out-of-range
values return `NULL` and append warning `1411 HY000`. Text arguments use
MySQL-style leading integer conversion: trailing noninteger characters append
warning `1292 22007` and the converted leading integer is used. If the
converted value is also out of range, MySQL emits truncation first and range
second, then returns `NULL`. Numeric fractional arguments round to an integer
before range validation.

## Result Metadata

`INET_ATON()` metadata:

- type: `LONGLONG`;
- charset/collation: binary;
- display length: `21`;
- decimals: `0`;
- flags: binary, numeric, unsigned;
- nullable: true.

`INET_NTOA()` metadata:

- type: `VAR_STRING`;
- charset/collation: connection collation;
- display length: `31`;
- decimals: `31`;
- flags: none;
- nullable: true.

The MySQL command-line metadata probe reports `INET_NTOA()` using
`latin1_swedish_ci`; MyLite uses the current connection collation like most
connection-string scalar functions until protocol-level character-set
compatibility is tightened.

## Runtime Architecture

The conversion algorithms live in a MyLite-owned runtime module and are exposed
to SQLite through public scalar UDF registration. No targeted SQLite fork hook
is needed: row-scalar queries lower the functions to private SQLite scalar
functions, while source-free scalar evaluation calls the same MyLite conversion
helpers directly.

## Diagnostics

Wrong argument counts fail with MySQL native function error `1582 42000`.
Invalid `INET_ATON()` string input appends warning `1411 HY000`. Invalid-range
`INET_NTOA()` input appends warning `1411 HY000`; text truncation appends
warning `1292 22007`.

Row-scalar warnings are generated from the runtime value available to the SQLite
UDF. MySQL formats some row warnings with the column reference text rather than
the value; MyLite keeps the MySQL warning code and function name while using the
available value text.

## Test Plan

Expected behavior is verified against MySQL 8.4.9 with
`packages/libmylite/tests/mysql_baseline_ip_address_functions_expectations.sh`.

The C runtime test covers:

- no-source and `DUAL` scalar results;
- legacy short forms and invalid-warning cases;
- `DO` execution and warning-count reporting;
- `SET @user_variable` and source-free DML value contexts;
- table-backed projection, filtering, and ordering;
- native function argument-count errors;
- result metadata for both functions.
