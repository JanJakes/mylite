# Baseline CAST Binary

## Summary

This phase adds the first explicit cast expression slice:

```sql
SELECT CAST(value AS BINARY)[, ...]
SELECT CAST(value AS BINARY)[, ...] FROM DUAL
DO CAST(value AS BINARY)[, ...]
```

The admitted input `value` is deliberately limited to ordinary string
literals, decimal integer literals with optional unary sign in the existing
scalar-value envelope, `TRUE`, `FALSE`, and `NULL`, with optional
parenthesization. The cast target is exactly bare `BINARY`, without a length.

The supported result is a MyLite scalar text cell containing the same visible
bytes that MySQL returns for these NUL-free inputs. `NULL` remains `NULL`.
This phase does not add binary column types, binary protocol metadata, embedded
NUL result delivery, `BINARY(N)`, padding/truncation behavior, `VARBINARY`,
`BLOB`, `CAST(... AS CHAR)`, `CONVERT()`, the deprecated unary `BINARY`
operator, table-backed casts, predicates, DML assignments, expression ordering,
expression defaults, parameters, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Cast functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
  - Binary and varbinary types:
    <https://dev.mysql.com/doc/refman/8.4/en/binary-varbinary.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_cast_binary_expectations.sh`.

The MySQL 8.4 manual documents `CAST(expr AS BINARY)` as a binary-string cast
and states that `NULL` input returns `NULL`. It also documents optional
`BINARY(N)` length behavior. Runtime probes against MySQL 8.4.9 establish the
expectations used by this narrow MyLite slice:

- `SELECT CAST('ABC' AS BINARY)` returns visible bytes `ABC` with ordinary
  mysql client display and `0x414243` when the client uses
  `--binary-as-hex=1`;
- `CAST('' AS BINARY)` returns the empty string;
- `CAST(NULL AS BINARY)` returns SQL `NULL`;
- decimal integer and boolean inputs are first rendered to their MySQL visible
  string forms, so `123`, `TRUE`, and `FALSE` cast to `123`, `1`, and `0`;
- supported bare `BINARY` casts produce no warnings;
- default result labels preserve the source expression text, while explicit
  aliases override it;
- parenthesized casts label with the parenthesized source text;
- `SELECT CAST('ABC' AS BINARY) FROM DUAL` returns one row;
- `DO CAST('ABC' AS BINARY), CAST(NULL AS BINARY)` succeeds, returns no row
  result, sets the previous row count to `0`, and produces no warnings;
- MySQL accepts broader forms such as `BINARY(N)`, `CAST(... AS CHAR)`,
  `CONVERT(expr USING BINARY)`, the deprecated `BINARY expr` operator, and
  table-backed casts. Those remain deferred by this MyLite baseline.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Ownership Boundaries

- Public API: unchanged. Supported scalar `SELECT` statements return text or
  `NULL` values through the existing `mylite_result_value_text()` convention.
  Because that public convention is currently C-string text, this phase admits
  only NUL-free cast results.
- Statement context: owns statement-boundary diagnostics, affected-row
  behavior, warning count publication, previous row-count state, and result
  finalization. Supported `CAST(... AS BINARY)` evaluation adds no warnings.
- Lexer/parser/AST: admits only the bare `CAST(expr AS BINARY)` syntax and
  records source spans for labels and diagnostics. The parser may recognize
  length-bearing or other cast targets only if runtime rejects them
  deterministically.
- Analyzer/runtime: evaluates supported no-source, `DUAL`, and `DO` casts in
  MyLite-owned scalar expression code. It does not delegate to SQLite.
- Catalog: not involved. This feature must not read or mutate table
  descriptors, descriptor caches, catalog generation, selected schema, or
  `sqlite_schema_generation`.
- Result builder: appends source-span or explicit-alias labels and stores one
  text/`NULL` value per scalar result cell.
- Storage/VFS/file format: no storage writes, no physical table access, no
  `.mylite` preamble changes, and no shifted SQLite payload changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch. This is MyLite wrapper/runtime behavior.

## Supported SQL

```sql
SELECT cast_binary_item[, cast_binary_item ...]
SELECT cast_binary_item[, cast_binary_item ...] FROM DUAL
DO cast_binary_scalar[, cast_binary_scalar ...]

cast_binary_item:
    cast_binary_scalar
  | cast_binary_scalar AS alias
  | cast_binary_scalar alias

cast_binary_scalar:
    CAST ( cast_binary_value AS BINARY )
  | ( cast_binary_scalar )

cast_binary_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ( cast_binary_value )
```

String literals are decoded by the existing MyLite string-literal lexer and SQL
mode policy. Because embedded `NUL` string values are not admitted elsewhere in
the current public text-result surface, they are not admitted here.

`CAST` remains usable as an unquoted identifier in MyLite identifier positions
where the corresponding MySQL function name is not reserved. The reserved
`BINARY` cast-target keyword is admitted as a scalar-result alias for this
slice, but it is not made a supported unquoted schema, table, or column name.
The bare cast target keyword in `CAST(... AS BINARY)` is not a binary column
descriptor and does not make binary storage types supported.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= CAST(T) LPAREN expression(V) AS BINARY RPAREN(R). {
    A = mylite_sql_parser_make_cast_binary_expression(state, T, V, R);
}
```

Unsupported cast targets and `BINARY(N)` remain outside this grammar slice and
may be rejected by the parser before runtime. These snippets describe MyLite's
admitted subset, not MySQL's full grammar.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size:

1. Admit a `CAST(... AS BINARY)` expression only in existing no-source,
   `FROM DUAL`, and `DO` scalar expression contexts.
2. Unwrap supported parentheses around the cast and around the input value.
3. Convert the input value:
   - ordinary string literal: decoded literal bytes, provided they are NUL-free;
   - integer literal: canonical decimal text, preserving a leading `-` only for
     negative values;
   - `TRUE`: `1`;
   - `FALSE`: `0`;
   - `NULL`: SQL `NULL`.
4. Return the converted text bytes or SQL `NULL`.
5. Preserve existing source-span default labels and explicit alias behavior.
6. Preserve existing scalar `SELECT` and `DO` row-count behavior:
   successful scalar `SELECT` returns one row and makes a following
   `ROW_COUNT()` return `-1`; successful `DO` returns no rows and makes a
   following `ROW_COUNT()` return `0`.
7. Produce no warnings for supported casts.
8. Reject unsupported cast inputs, targets, and contexts before any SQLite SQL
   is generated.

This phase does not provide binary collation semantics. For example,
`CAST('a' AS BINARY) = 'A'` remains outside the supported expression subset
because scalar comparisons over binary-string operands are not yet part of
MyLite's expression engine.

## Diagnostics

Supported casts produce no warnings.

Diagnostics for this phase:

- syntax errors such as missing `AS` or malformed cast targets: existing parser
  syntax-error path;
- unsupported cast target, including `CHAR`, `SIGNED`, `UNSIGNED`, temporal
  targets, JSON, spatial, and other MySQL cast targets: deterministic
  unsupported-cast diagnostic;
- unsupported `BINARY(N)` length form: deterministic unsupported-cast
  diagnostic unless the parser rejects the syntax before runtime;
- unsupported input expression, including column references, string functions,
  arithmetic, scalar subqueries, parameters, user variables, hexadecimal and bit
  literals, decimal/floating literals, and temporal literals: deterministic
  unsupported-cast-input diagnostic;
- unsupported contexts, including table-backed projection, predicates,
  grouping, ordering, DML assignments, default expressions, and generated
  columns: deterministic unsupported scalar-expression diagnostic;
- embedded `NUL` in an input string literal if such a token reaches this
  evaluator: deterministic unsupported literal diagnostic;
- allocation failure: existing `MYLITE_NOMEM` / SQLSTATE `HY001` path;
- public API misuse: unchanged existing `mylite_execute()` misuse behavior.

The exact MyLite-specific unsupported messages should be stable enough for
tests, but they do not need to claim MySQL equivalence for features this phase
intentionally defers.

## Result And Metadata

Successful supported `SELECT CAST(... AS BINARY)`:

- returns one row;
- returns one text value or SQL `NULL`;
- uses source expression text as the default column name, preserving case,
  spacing, and parenthesization;
- honors explicit aliases through the existing scalar select-item alias path;
- reports `warning_count == 0`;
- returns no protocol-grade binary-string type metadata, charset metadata,
  collation metadata, byte-length metadata, or flags beyond the current public
  result object surface.

The metadata gap is intentional for this baseline. A future binary-type or wire
protocol phase must decide how MyLite represents arbitrary byte strings,
embedded NUL bytes, binary collations, client display policy, and protocol
field metadata.

## SQLite And Performance

This feature does not call SQLite for supported evaluation. It is a MyLite
scalar runtime branch that writes a borrowed or owned scalar text cell into the
existing result path. No SQLite SQL is generated, no table scan occurs for
admitted forms, no catalog descriptor is touched, and no `.mylite` file bytes
change.

No SQLite fork hook is needed. Per the fork policy, MyLite wrapper code is
sufficient until a later binary value/storage/wire-protocol slice proves that a
public SQLite API is missing.

## Tests

Add MySQL-runtime expectation coverage for:

- MySQL version guard;
- ordinary and binary-as-hex client display for `CAST('ABC' AS BINARY)`;
- empty string, `NULL`, decimal integer, `TRUE`, and `FALSE` inputs;
- aliases, parenthesized labels, `FROM DUAL`, `DO`, `ROW_COUNT()`, and
  `@@warning_count`;
- MySQL-accepted but deferred forms: `BINARY(N)`, `CAST(... AS CHAR)`,
  `CONVERT(expr USING BINARY)`, deprecated `BINARY expr`, table-backed casts,
  and binary-string comparisons.

Add plain C tests for:

- parser AST shape and source spans for `CAST('ABC' AS BINARY)`, aliases, and
  parenthesized casts;
- successful no-source and `FROM DUAL` scalar `SELECT` values for string,
  empty string, integer, boolean, and `NULL` inputs;
- `DO` execution, affected rows, warning count, and absence of row result;
- default labels and explicit aliases;
- zero-initialized cleanup of any new scalar cell or AST state;
- deterministic rejections for length-bearing targets, other cast targets,
  `CONVERT`, unary `BINARY`, columns/table-backed casts, scalar subqueries,
  arithmetic inputs, parameters, hex/bit literals, DML assignments, predicates,
  expression defaults, and ordering expressions;
- selected schema/catalog generation/file preamble invariants;
- independent handles;
- existing parser, scalar-expression, row-scalar, SQL mode, result, storage,
  and runtime lifecycle tests.

## Compatibility Documentation

Update only:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-casts.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/type-system-literals-conversion.md` if needed to mention
  the narrow explicit-cast literal surface.

Do not mark binary column types, `VARBINARY`, `BLOB`, `CONVERT()`, unary
`BINARY`, full `CAST()`, binary comparisons, table-backed casts, expression
defaults, generated columns, or protocol-grade metadata as supported.
