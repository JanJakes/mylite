# Baseline CONVERT Syntax Expansion

This phase extends the existing scalar cast surface with two narrow
`CONVERT()` forms:

```sql
SELECT CONVERT(value, BINARY)[, ...]
SELECT CONVERT(value, BINARY)[, ...] FROM DUAL
DO CONVERT(value, BINARY)[, ...]

SELECT CONVERT(value USING utf8mb4)[, ...]
SELECT CONVERT(value USING utf8mb4)[, ...] FROM DUAL
DO CONVERT(value USING utf8mb4)[, ...]
```

The goal is syntax compatibility for common MySQL cast/transcoding calls while
reusing the already implemented no-source scalar result conventions. This phase
does not add table-backed casts, predicate casts, assignment casts, binary
comparison semantics, result collation metadata, or broader expression
conversion.

## Compatibility Evidence

Primary references:

- MySQL 8.4 Reference Manual, "Cast Functions and Operators":
  <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
- Observed MySQL runtime: Docker container `mylite-mysql-849`, `SELECT
  VERSION()` = `8.4.9`.

The manual states that `CONVERT(expr, type)` is equivalent to `CAST(expr AS
type)`, that `CONVERT(expr USING transcoding_name)` converts between character
sets, and that `CONVERT(expr USING BINARY)`, `CAST(expr AS BINARY)`, and the
deprecated unary `BINARY expr` are equivalent binary-string conversions. It also
lists `BINARY[(N)]` and `CHAR[(N)] CHARACTER SET charset_name` among broader
cast targets. This phase admits only bare `BINARY` in the ODBC-style form and
only `utf8mb4` in the `USING` character-set form.

Runtime probes verify:

- `CONVERT('ABC', BINARY)` returns binary bytes `ABC`; the mysql CLI displays
  `0x414243` under `--binary-as-hex=1`;
- `CONVERT('', BINARY)` returns an empty binary string;
- `CONVERT(NULL, BINARY)` returns `NULL`;
- integer and boolean operands convert to their textual MySQL scalar form before
  becoming binary bytes;
- `CONVERT('ABC' USING utf8mb4)` and `CONVERT('é' USING utf8mb4)` return the
  corresponding nonbinary string values with no warnings;
- `CONVERT(NULL USING utf8mb4)` returns `NULL`, and integer/boolean operands
  return `123`, `1`, and `0` style text;
- `SELECT` leaves `ROW_COUNT() == -1` after successful projection evaluation
  and emits no warnings;
- `DO CONVERT(...)` succeeds, returns no result set, leaves `ROW_COUNT() == 0`,
  and emits no warnings;
- `CONVERT('ABC', BINARY, 1)` is a MySQL syntax error.

MySQL accepts wider forms such as `CONVERT('ABC', BINARY(5))`,
`CONVERT('ABC', CHAR)`, `CONVERT('ABC', SIGNED)`, and non-`utf8mb4`
transcodings. They remain deferred.

## Ownership Boundaries

- Public API: no ABI change. Results use existing `mylite_result` scalar
  projection behavior and public text/`NULL` accessors.
- Lexer/parser/AST: admit two new expression node kinds for the two syntax
  forms. The parser owns syntax recognition and source spans used for result
  labels.
- Runtime scalar evaluator: owns no-source, `DUAL`, and `DO` evaluation. It
  converts the supported value operand before result construction.
- Catalog/analyzer/planner: no descriptor or catalog metadata changes.
- SQLite/storage/VFS: no generated SQLite SQL, SQLite fork changes, physical
  storage changes, `.mylite` format changes, or VFS changes.

## Grammar

MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA BINARY RPAREN(R).
expression(A) ::= CONVERT(T) LPAREN expression(V) USING option_name(C) RPAREN(R).
```

The first production builds a `convert_binary_type_expression` node. The second
builds a `convert_using_charset_expression` node that preserves the parsed
character-set name child. Runtime supports only identifier `utf8mb4` for the
second form in this phase. `CONVERT(value USING BINARY)` remains represented by
the existing `convert_using_binary_expression` node.

The parser should continue rejecting malformed comma forms, extra arguments, and
unsupported target syntax such as `BINARY(5)` until a future cast-target grammar
phase specifies it.

## Semantics

`CONVERT(value, BINARY)`:

- participates only in no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- accepts ordinary string literals, decimal integer literals with optional unary
  sign, `TRUE`, `FALSE`, and `NULL`;
- returns `NULL` when the value is `NULL`;
- decodes ordinary string literals using the current MyLite string-literal rules
  and rejects embedded NUL bytes on the current public scalar text surface;
- normalizes integer and boolean operands through the existing MyLite scalar
  textual conversion used by bare `CAST(value AS BINARY)`;
- reports no warnings for supported inputs.

`CONVERT(value USING utf8mb4)`:

- participates only in the same no-source/`DUAL`/`DO` scalar envelopes;
- accepts ordinary string literals, decimal integer literals with optional unary
  sign, `TRUE`, `FALSE`, and `NULL`;
- returns `NULL` when the value is `NULL`;
- decodes ordinary string literals and rejects embedded NUL bytes on the current
  scalar text surface;
- returns the existing MyLite scalar text representation for integer and boolean
  operands;
- performs no actual character-set transcoding because MyLite currently admits
  only UTF-8 text on this surface;
- reports no warnings for supported inputs.

## Diagnostics

Supported execution failures are deterministic MyLite diagnostics unless MySQL
syntax itself rejects the statement before runtime:

| Case | Behavior |
| --- | --- |
| `CONVERT(value, BINARY, extra)` | MySQL-style syntax error `1064 / 42000` |
| `CONVERT(value, BINARY(5))` | syntax error in this phase |
| `CONVERT(value, CHAR)` / `SIGNED` / `UNSIGNED` / temporal targets | syntax error or deterministic unsupported grammar rejection in this phase |
| `CONVERT(value USING latin1)` / non-`utf8mb4` character set | deterministic unsupported runtime error |
| `CONVERT(value USING 'utf8mb4')` | deterministic unsupported runtime error; only identifier charset names are admitted |
| table-backed `SELECT CONVERT(... ) FROM t` | deterministic unsupported scalar/table-backed expression error |
| `UPDATE t SET col = CONVERT(...)` or predicates using `CONVERT(...)` | deterministic unsupported expression error |
| embedded NUL in scalar text operand | deterministic unsupported runtime error |
| allocation failure | `MYLITE_NOMEM` with the existing out-of-memory diagnostic |

## Tests

Add or extend fast plain C tests under `packages/libmylite/tests/` and a MySQL
runtime expectation script:

- parser AST/source-span coverage for `CONVERT('ABC', BINARY)`,
  parenthesized forms, `FROM DUAL`, `DO`, and `CONVERT('ABC' USING utf8mb4)`;
- no-source and `DUAL` scalar values for string, empty string, integer, signed
  integer, boolean, and `NULL` operands;
- labels and explicit aliases;
- `DO` result shape, affected rows, and warning count;
- deterministic rejection of unsupported target types, length-bearing binary
  casts, unsupported charset names, string charset names, table-backed
  projection, predicates, and DML assignment;
- compatibility docs limited wording;
- existing parser/scalar/cast/convert tests still pass.

Verification before marking done:

1. `cmake --build --preset dev`
2. Focused parser/runtime CTest entries for scalar expression projection.
3. `packages/libmylite/tests/mysql_baseline_convert_syntax_expansion_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-casts.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
supported no-source/`DUAL`/`DO` subset. Do not claim general cast targets,
length-bearing `BINARY(N)`, `CHAR CHARACTER SET`, collations, table-backed
casts, predicates, assignments, charset conversion, binary comparison semantics,
or protocol-grade metadata.
