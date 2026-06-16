# Baseline Digest Functions

## Summary

MyLite supports a baseline MySQL 8.4.9-compatible surface for the ASCII-hex
digest functions `MD5()`, `SHA()`, `SHA1()`, and `SHA2()`. These functions are
implemented as MyLite-owned scalar functions and, for table-backed row-scalar
projections, are lowered to MyLite SQLite scalar callbacks.

This slice is based on the MySQL 8.4 Reference Manual encryption and compression
function descriptions and live MySQL 8.4.9 runtime probes.

References:

- <https://dev.mysql.com/doc/refman/8.4/en/encryption-functions.html>

## MySQL 8.4.9 Behavior

`MD5(str)` returns a 32-character lowercase hexadecimal MD5 digest. `SHA(str)`
and `SHA1(str)` are synonyms and return a 40-character lowercase hexadecimal
SHA-1 digest. `SHA2(str, hash_length)` returns a lowercase hexadecimal SHA-2
digest when `hash_length` is `224`, `256`, `384`, `512`, or `0`; `0` is
equivalent to `256`.

If the input string argument is `NULL`, the result is `NULL`. For `SHA2()`, a
`NULL` hash-length argument also returns `NULL`. Unsupported SHA-2 hash lengths
return `NULL`; MySQL emits warning 1583 for that case. Wrong argument counts for
all four function names fail with error 1582 / SQLSTATE `42000`.

The returned digest is a nonbinary string in the connection character set and
collation unless the connection character set is binary. Under the MyLite
baseline connection-character-set surface, metadata should therefore expose a
nullable `VAR_STRING` result using the active connection collation.

Observed MySQL 8.4.9 probes:

```sql
SELECT MD5(''), MD5('MySQL'), MD5(NULL), MD5(123), MD5(TRUE), MD5(FALSE),
       MD5(-1), MD5(X'616263');
SELECT SHA1(''), SHA('abc'), SHA1(NULL), SHA1(123), SHA1(TRUE), SHA1(FALSE),
       SHA1(-1), SHA1(X'616263');
SELECT SHA2('',224), SHA2('',256), SHA2('',384), SHA2('',512), SHA2('',0),
       SHA2('',1), SHA2('',NULL), SHA2(NULL,256), SHA2('abc','256'),
       SHA2('abc',TRUE), SHA2('abc',FALSE);
SELECT LENGTH(MD5('abc')), LENGTH(SHA1('abc')), LENGTH(SHA2('abc',224)),
       LENGTH(SHA2('abc',256)), LENGTH(SHA2('abc',384)), LENGTH(SHA2('abc',512));
SELECT CHARSET(MD5('abc')), COLLATION(MD5('abc')),
       CHARSET(SHA1('abc')), COLLATION(SHA1('abc')),
       CHARSET(SHA2('abc',256)), COLLATION(SHA2('abc',256));
```

Important observed values:

- `MD5('') = d41d8cd98f00b204e9800998ecf8427e`.
- `MD5('MySQL') = 62a004b95946bb97541afa471dcca73a`.
- `SHA1('') = da39a3ee5e6b4b0d3255bfef95601890afd80709`.
- `SHA('abc') = a9993e364706816aba3e25717850c26c9cd0d89d`.
- `SHA2('',224)` has 56 hex characters.
- `SHA2('',256)` and `SHA2('',0)` are identical and have 64 hex characters.
- `SHA2('',384)` has 96 hex characters.
- `SHA2('',512)` has 128 hex characters.
- `SHA2('',1)`, `SHA2('',NULL)`, `SHA2(NULL,256)`, and `SHA2('abc',TRUE)`
  return `NULL`; unsupported non-`NULL` lengths emit warning 1583.
- `SHA2('abc',FALSE)` is equivalent to `SHA2('abc',0)`.

## Supported Scope

MyLite supports:

- Parser and AST nodes for `MD5(expr)`, `SHA(expr)`, `SHA1(expr)`, and
  `SHA2(expr, expr)`.
- MySQL-shaped wrong-argument-count nodes and error 1582 for too few or too many
  arguments.
- No-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO` evaluation for
  string, hex, integer, boolean, `NULL`, and supported session-scalar arguments.
- Constant DML value conversion through the existing session-scalar path.
- Single-table row-scalar projection for descriptor columns and supported
  constant arguments:
  - `MD5(column)`
  - `SHA(column)`
  - `SHA1(column)`
  - `SHA2(column, 224|256|384|512|0|FALSE)`
- Descriptor-backed `WHERE` predicates where supported row-scalar digest
  expressions appear as the predicate subject for comparison, `IS [NOT] NULL`,
  and `[NOT] BETWEEN`. See
  [baseline binary and digest function predicates](../baseline-binary-digest-function-predicates/specs.md).
- `SHA2()` hash-length arguments as integer, signed integer, boolean, or `NULL`
  literal values in the baseline surface.
- MySQL-compatible lowercase hexadecimal digest text and `NULL` propagation.
- Warning 1583 for unsupported non-`NULL` numeric SHA-2 hash lengths in
  no-source scalar evaluation.

## Intentionally Unsupported

This slice does not implement:

- String-to-integer coercion and warning 1292 for `SHA2()` hash-length string
  arguments such as `'224x'`.
- Full expression arguments such as `MD5(1 + 2)` outside existing admitted
  session-scalar values.
- Row-scalar `IN`, ordering/grouping, broad DML assignment, generated-column, or
  default-expression contexts.
- FIPS-mode behavior that makes `MD5()` return `NULL`.
- Binary connection-character-set metadata differences beyond the current
  baseline connection-collation surface.
- AES, random byte, password validation, `STATEMENT_DIGEST()`, compression, or
  decompression functions.

## Grammar

Independently authored MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= MD5(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= SHA(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= SHA1(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= SHA2(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).

expression(A) ::= MD5(T) LPAREN RPAREN(R).
expression(A) ::= MD5(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= SHA(T) LPAREN RPAREN(R).
expression(A) ::= SHA(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= SHA1(T) LPAREN RPAREN(R).
expression(A) ::= SHA1(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= SHA2(T) LPAREN RPAREN(R).
expression(A) ::= SHA2(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= SHA2(T) LPAREN expression(B) COMMA expression(C)
                  COMMA function_argument_list(D) RPAREN(R).
```

## Runtime and Storage

Digest evaluation is pure and has no catalog, schema, or file-format side
effects. The hash algorithms live in a MyLite-owned runtime module with no new
third-party dependency. Table-backed row-scalar projection lowers to SQLite
through public `sqlite3_create_function_v2()` registration wrappers; no targeted
SQLite fork hook is required.

The functions are deterministic for supported inputs. Result strings are
allocated per result cell or returned by SQLite as transient text from the
callback-owned allocation.

## Test Plan

Tests must cover:

- MySQL-runtime-verified result values for MD5, SHA/SHA1, SHA2, `NULL`, integer,
  boolean, signed integer, and hex literal inputs.
- SHA2 hash lengths `224`, `256`, `384`, `512`, `0`, invalid numeric lengths,
  `NULL`, `TRUE`, and `FALSE`.
- Result lengths for every supported digest family.
- `SELECT`, `SELECT ... FROM DUAL`, `DO`, constant DML value conversion, and
  single-table row-scalar projection.
- Wrong argument count diagnostics for every supported function name.
- File/catalog generation unchanged after pure scalar evaluation.
