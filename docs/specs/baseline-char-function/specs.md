# Baseline CHAR Function

## Summary

This phase adds a narrow MySQL-compatible scalar `CHAR()` function:

```sql
CHAR(integer_value[, integer_value ...])
CHAR(integer_value[, integer_value ...] USING charset_name)
```

The supported surface follows the existing scalar and row-scalar function
envelope: no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and
single-table row-scalar `SELECT` projection for the default binary form.
Explicit `USING` character-set forms are supported for no-source and `DUAL`
scalar evaluation, but not for table-backed row-scalar projections. This phase
does not add expression predicates, expression ordering, DML assignment values,
generated columns, defaults, or a general expression engine.

Each admitted non-`NULL` argument is converted to an unsigned 32-bit value and
written as the shortest big-endian byte sequence, with `0` represented as one
`0x00` byte. `NULL` arguments are skipped; if every argument is `NULL`, the
result is the empty non-`NULL` string. Without `USING`, the result has MySQL's
default binary charset metadata. With `USING`, MyLite applies the documented
scalar charset metadata and UTF-8 invalid-byte behavior for the admitted
charset subset.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-hex-function/specs.md`
  - `docs/specs/baseline-unhex-function/specs.md`
  - `docs/specs/baseline-ascii-ord-functions/specs.md`
- Official MySQL 8.4 Reference Manual, string functions and operators:
  - <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_char_function_expectations.sh` and
  verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `CHAR()` with zero arguments is a syntax error `1064 / 42000`.
- The function name may be separated from `(` by whitespace in default SQL
  mode.
- `HEX(CHAR(65)) = '41'`, `CHARSET(CHAR(65)) = 'binary'`, and
  `COLLATION(CHAR(65)) = 'binary'`.
- `HEX(CHAR(77,121,83,81,76)) = '4D7953514C'`.
- `CHAR(NULL)` returns an empty non-`NULL` binary string.
- `CHAR(65,NULL,66)` skips the `NULL` argument and returns bytes `X'4142'`.
- `CHAR(TRUE,FALSE)` returns bytes `X'0100'`.
- `CHAR(0)` returns one byte `X'00'`.
- `CHAR(256)` returns `X'0100'`, `CHAR(65536)` returns `X'010000'`, and
  `CHAR(4294967295)` returns `X'FFFFFFFF'`.
- Negative integer arguments use the low unsigned 32-bit value, so
  `CHAR(-1)` returns `X'FFFFFFFF'` and `CHAR(-256)` returns `X'FFFFFF00'`.
- `CHAR(... USING binary)` keeps binary charset metadata.
- `CHAR(... USING utf8mb4)` reports `utf8mb4_0900_ai_ci` metadata and
  `COERCIBILITY() = 4`.
- `CHAR(... USING utf8)` reports `utf8mb3` / `utf8mb3_general_ci` and emits
  warning `3719` for each expression occurrence.
- `CHAR(... USING utf8mb3)` reports `utf8mb3` / `utf8mb3_general_ci` and emits
  warning `1287` for each expression occurrence.
- `CHAR(... USING latin1)` reports `latin1` / `latin1_swedish_ci`.
- Invalid `utf8mb4` byte sequences return the valid prefix with warning `1300`
  in non-strict mode and `NULL` with warning `1300` in strict mode.
- String, decimal, float, and over-wide numeric arguments are accepted by MySQL
  through broader numeric coercion, sometimes with warnings. This baseline
  defers those conversions.
- Successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

## Ownership Boundaries

- Public API: unchanged. Binary results, including embedded `NUL` bytes, are
  exposed through the existing byte-safe result accessors. Callers that need
  exact bytes must use `mylite_result_value_bytes()` and
  `mylite_result_value_size()`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Default and non-deprecated charset calls add no warnings;
  `utf8`, `utf8mb3`, and invalid UTF-8 forms append the MySQL-shaped warnings
  recorded in the comparison script.
- Lexer/parser/AST: admits one or more `CHAR()` arguments, optional
  `USING charset_name`, and `USING binary` as a keyword charset name, preserving
  source spans for result labels and diagnostics. The zero-argument form remains
  a syntax error, matching observed MySQL behavior.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections lower to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite registers a private scalar helper through SQLite's public
  function API. No SQLite fork patch is required.
- Result builder: uses existing row result conventions and byte-valued cells
  for binary results.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT char_item[, char_item ...]
SELECT char_item[, char_item ...] FROM DUAL
```

`DO` form:

```sql
DO char_expr[, char_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
`CHAR()` call:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
char_expr:
    CHAR ( char_value [, char_value ...] )
  | CHAR ( char_value [, char_value ...] USING scalar_charset_name )

char_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | descriptor_integer_column_reference  -- table-backed SELECT only
  | ( char_value )

scalar_charset_name:
    BINARY
  | utf8mb4
  | utf8
  | utf8mb3
  | latin1
  | quoted_charset_name_matching_the_same_supported_names
```

`descriptor_integer_column_reference` follows the existing single-source table
alias policy and may explicitly name invisible descriptor columns. Supported
descriptor column families are integer-family columns stored in the current
signed 64-bit physical range. `NULL` descriptor values are skipped.

The following remain outside this phase:

- table-backed `CHAR(column USING charset_name)` row-scalar projections;
- explicit `USING` charset names outside `binary`, `utf8mb4`, `utf8`,
  `utf8mb3`, and `latin1`;
- string, hex, decimal, float, temporal, bit, JSON, spatial, and binary-string
  argument coercion;
- expression arguments such as arithmetic, column-to-column expressions,
  nested row functions, scalar subqueries, stored functions, parameters, user
  variables, CTEs, and arbitrary expressions;
- `WHERE CHAR(...) ...`, `HAVING CHAR(...) ...`, expression `ORDER BY`,
  grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = CHAR(n)`;
- complete protocol/result metadata for binary string width and character-set
  derivation beyond the existing MyLite result object.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= CHAR(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CHAR_FUNCTION, B, R);
}
expression(A) ::= CHAR(T) LPAREN function_argument_list(B) USING option_name(C) RPAREN(R). {
    A = mylite_sql_parser_make_char_using_charset_function(state, T, B, C, R);
}
expression(A) ::= CHAR(T) LPAREN function_argument_list(B) USING BINARY(C) RPAREN(R). {
    A = mylite_sql_parser_make_char_using_charset_function(
        state, T, B, mylite_sql_parser_make_identifier(state, C), R);
}
```

The zero-argument form is intentionally not admitted by this grammar slice.
`CHAR` remains a type keyword; the function production is added explicitly
instead of making `CHAR` a general identifier.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert each admitted argument:
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: skipped with no warning;
   - integer literal: parsed as a signed integer within the current admitted
     64-bit literal range, then converted to the low unsigned 32 bits.
3. For each non-`NULL` value, append the shortest big-endian byte sequence of
   the unsigned 32-bit value. The value `0` appends one byte `0x00`.
4. If no argument appended bytes, return the empty non-`NULL` binary string.
5. Apply optional scalar charset handling:
   - `binary`: leave bytes and metadata binary;
   - `latin1`: leave bytes and metadata latin1;
   - `utf8mb4`, `utf8`, `utf8mb3`: validate the byte sequence as UTF-8; on
     invalid input append warning `1300`, return the valid prefix in non-strict
     mode, and return SQL `NULL` in strict mode.
6. Return bytes with byte-safe result size metadata.

Table-backed row-scalar evaluation uses the same internal byte builder through
a SQLite scalar function. Descriptor-backed columns are passed to the helper as
SQLite values only after the planner has resolved the column against MyLite
catalog descriptors and verified the descriptor type family. Explicit `USING`
forms are rejected in table-backed row-scalar projections until the SQLite
helper can apply per-row charset warnings and strict-mode nulling.

## Diagnostics

Supported calls succeed without warnings. The baseline emits deterministic
MyLite diagnostics for unsupported shapes:

- zero-argument `CHAR()` is a syntax error;
- unknown descriptor columns use the existing MySQL-shaped unknown-column
  diagnostic;
- unsupported argument expressions produce a MyLite-specific unsupported
  diagnostic naming the current `CHAR()` subset;
- unsupported descriptor column type families produce a MyLite-specific
  unsupported diagnostic;
- unknown charset names use MyLite's existing MySQL-shaped unknown-character-set
  diagnostic;
- invalid scalar UTF-8 byte sequences append warning `1300` and either truncate
  to the valid prefix or return `NULL`, depending on strict mode;
- integer literals outside the admitted parser/runtime range produce the
  existing literal-conversion diagnostic for the relevant expression path;
- SQLite helper misuse or physical execution failure is mapped through the
  existing SQLite-to-MyLite status and diagnostics path;
- allocation failure reports the existing MyLite out-of-memory diagnostic.

MySQL-compatible warning-producing string, decimal, float, and out-of-range
numeric coercions are deferred until MyLite has a broader expression conversion
layer that can share behavior across functions and DML.

## Performance And Storage

Constant scalar calls are evaluated by MyLite without opening SQLite row
storage. Table-backed calls remain close to SQLite's execution path: MyLite
plans descriptor-resolved columns once, emits a private SQLite scalar helper
call, and lets SQLite scan, filter, order, and limit rows through the existing
row-scalar `SELECT` path. MyLite does not materialize the whole table to compute
`CHAR()` values.

No catalog, schema, file-format, VFS, or SQLite fork changes are required.

## Tests

Implementation tests must cover:

- parser acceptance for one and multiple arguments, whitespace before `(`,
  `DO`, table-backed projections, and scalar `USING` charset forms including
  keyword `binary`;
- parser rejection of zero arguments;
- no-source and `DUAL` byte results, including embedded `NUL`;
- scalar `USING binary`, `utf8mb4`, `utf8`, `utf8mb3`, and `latin1` behavior,
  metadata wrappers, charset warnings, unknown charset errors, and strict versus
  non-strict invalid UTF-8 handling;
- multiple arguments, `NULL` skipping, all-`NULL` empty result, booleans, zero,
  positive multibyte boundaries, and negative low-32-bit behavior;
- row-backed integer columns, nullable integer columns, filtering, ordering,
  limiting, and reopen persistence;
- deterministic rejections for string, decimal, float, hex, expression, nested
  row-function, table-backed explicit `USING`, unsupported descriptor type, and
  unknown-column arguments;
- no result rows and zero warnings for successful `DO`;
- byte-safe public result access and zero-initialized cleanup.

The MySQL comparison script records the observed MySQL 8.4.9 behavior for both
the supported subset and accepted upstream forms that this baseline
intentionally defers.
