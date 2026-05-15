# Baseline CONVERT Using Binary

## Summary

This phase adds a narrow `CONVERT()` character-set conversion slice:

```sql
SELECT CONVERT('string' USING BINARY)[, ...]
SELECT CONVERT('string' USING BINARY)[, ...] FROM DUAL
DO CONVERT('string' USING BINARY)[, ...]
```

The admitted input is an ordinary MyLite string literal in the current scalar
projection envelope. The admitted transcoding name is exactly `BINARY`.
Supported results use the same current public text cell convention as the
existing bare `CAST(value AS BINARY)` baseline, so this phase admits only
NUL-free decoded string bytes.

This phase does not add `CONVERT(expr, type)`, non-`BINARY` character sets,
length-bearing binary casts, numeric/boolean/`NULL` `CONVERT()` operands,
column operands, table-backed conversion, predicates, DML assignments,
expression defaults, character-set metadata, collation metadata, embedded-NUL
scalar result delivery, the deprecated unary `BINARY` operator, or arbitrary
SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Cast functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
  - Character set and collation of function results:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior already captured by
  `packages/libmylite/tests/mysql_baseline_cast_binary_expectations.sh`.
  That artifact includes the accepted deferred form
  `CONVERT('ABC' USING BINARY)` and records the `--binary-as-hex=1` mysql
  client output as `0x414243`.
- This phase adds
  `packages/libmylite/tests/mysql_baseline_convert_using_binary_expectations.sh`
  as the feature-specific expectation artifact for the same verified
  string-literal behavior. At implementation time Docker was unresponsive
  locally, so this slice deliberately uses only the previously recorded
  MySQL 8.4.9 observation rather than broadening into unprobed forms.

The MySQL 8.4 manual documents `CONVERT(expr USING transcoding_name)` as a
character-set conversion form and documents `CONVERT(expr USING BINARY)`,
`CAST(expr AS BINARY)`, and `BINARY expr` as equivalent ways to convert a
string expression to a binary string. It also documents that binary string
results may display as hexadecimal in the mysql client when
`--binary-as-hex` is enabled.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Ownership Boundaries

- Public API: unchanged. Supported scalar `SELECT` statements return a text
  value through the existing `mylite_result_value_text()` surface, and `DO`
  returns no row result.
- Statement context: owns diagnostics, warning count publication, previous
  row-count state, and result finalization. Supported conversions produce no
  warnings.
- Lexer/parser/AST: admits only `CONVERT(expression USING BINARY)` syntax and
  records an independent AST node so runtime can keep this first slice narrower
  than `CAST(value AS BINARY)`.
- Analyzer/runtime: evaluates supported no-source, `DUAL`, and `DO`
  conversions in MyLite-owned scalar expression code. It does not delegate to
  SQLite.
- Catalog: not involved. This feature must not read or mutate descriptors,
  descriptor caches, catalog generation, selected schema, or
  `sqlite_schema_generation`.
- Result builder: uses the existing scalar select-item label behavior. Explicit
  aliases override the source expression label.
- Storage/VFS/file format: no storage writes, no physical table access, no
  `.mylite` preamble changes, and no shifted SQLite payload changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch. This is MyLite wrapper/runtime behavior.

## Supported SQL

```sql
SELECT convert_using_binary_item[, convert_using_binary_item ...]
SELECT convert_using_binary_item[, convert_using_binary_item ...] FROM DUAL
DO convert_using_binary_scalar[, convert_using_binary_scalar ...]

convert_using_binary_item:
    convert_using_binary_scalar
  | convert_using_binary_scalar AS alias
  | convert_using_binary_scalar alias

convert_using_binary_scalar:
    CONVERT ( convert_using_binary_value USING BINARY )
  | ( convert_using_binary_scalar )

convert_using_binary_value:
    string_literal
```

String literals are decoded by the existing MyLite string-literal lexer and SQL
mode policy. `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES` continue to affect later
tokens exactly as in other admitted string-literal positions.

`CONVERT` and `USING` are reserved words in the existing lexer. This feature
does not make them available as unquoted identifiers in new positions.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= CONVERT(T) LPAREN expression(V) USING BINARY RPAREN(R). {
    A = mylite_sql_parser_make_convert_using_binary_expression(state, T, V, R);
}
```

Unsupported character-set names, `CONVERT(expr, type)`, and `BINARY(N)` remain
outside this grammar slice and may be rejected by the parser before runtime.
The snippet describes MyLite's admitted subset, not MySQL's full grammar.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size:

1. Admit a `CONVERT(... USING BINARY)` expression only in existing no-source,
   `FROM DUAL`, and `DO` scalar expression contexts.
2. Unwrap supported parentheses around the conversion expression.
3. Require the operand to unwrap to an ordinary string literal.
4. Decode the string literal with the current SQL-mode-aware string literal
   decoder.
5. Reject decoded strings containing embedded `NUL` bytes because the current
   public scalar text result convention cannot expose them safely.
6. Return the decoded bytes as the scalar text value.
7. Preserve existing scalar `SELECT` and `DO` row-count behavior:
   successful scalar `SELECT` returns one row and makes a following
   `ROW_COUNT()` return `-1`; successful `DO` returns no rows and makes a
   following `ROW_COUNT()` return `0`.
8. Produce no warnings for supported conversions.
9. Reject unsupported operands, character-set names, conversion forms, and
   contexts before any SQLite SQL is generated.

This phase does not provide binary collation semantics. For example,
`CONVERT('a' USING BINARY) = 'A'` remains outside the supported expression
subset because scalar comparisons over binary-string operands are not yet part
of MyLite's expression engine.

## Diagnostics

Supported conversions produce no warnings.

Diagnostics for this phase:

- syntax errors such as missing `USING`, non-`BINARY` transcoding names that
  the grammar cannot reduce, `CONVERT(expr, type)`, or malformed parentheses:
  existing parser syntax-error path;
- unsupported input expression, including integer literals, boolean literals,
  `NULL`, column references, string functions, arithmetic, scalar subqueries,
  parameters, user variables, hexadecimal and bit literals, decimal/floating
  literals, and temporal literals: deterministic unsupported-conversion-input
  diagnostic;
- embedded `NUL` in an input string literal if such a token reaches this
  evaluator: deterministic unsupported literal diagnostic;
- unsupported contexts, including table-backed projection, predicates,
  grouping, ordering, DML assignments, default expressions, and generated
  columns: deterministic unsupported scalar-expression diagnostic;
- allocation failure: existing `MYLITE_NOMEM` / SQLSTATE `HY001` path;
- public API misuse: unchanged existing `mylite_execute()` misuse behavior.

The exact MyLite-specific unsupported messages should be stable enough for
tests, but they do not need to claim MySQL equivalence for forms this phase
intentionally defers.

## Result And Metadata

Successful supported `SELECT CONVERT('string' USING BINARY)`:

- returns one row;
- returns one text value containing the decoded string bytes;
- uses source expression text as the default column name through the existing
  scalar label path, while explicit aliases override it;
- reports `warning_count == 0`;
- returns no protocol-grade binary-string type metadata, charset metadata,
  collation metadata, byte-length metadata, or flags beyond the current public
  result object surface.

The metadata gap is intentional for this baseline. A future binary-string or
wire-protocol phase must decide how MyLite represents arbitrary byte strings,
embedded NUL bytes, binary collations, client display policy, and protocol
metadata.

## Unsupported Forms

The following remain unsupported:

- `CONVERT(expr, type)`;
- `CONVERT(expr USING utf8mb4)` and all other non-`BINARY` transcodings;
- `CONVERT(NULL USING BINARY)`, numeric operands, boolean operands, and general
  expression operands until each form has MySQL 8.4.9 runtime evidence;
- table-backed conversion, including `SELECT CONVERT(column USING BINARY)`;
- conversion in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, aggregate
  arguments, DML assignments, generated/default expressions, and constraints;
- `CAST(... AS BINARY(N))`, `CAST(... AS CHAR)`, other cast targets, and unary
  `BINARY expr`;
- character-set introducers, collations inside or after the conversion,
  binary string comparison semantics, and protocol-level metadata.

## Performance And SQLite Integration

The conversion is evaluated directly in the existing scalar expression runtime.
It decodes one literal and appends one result cell. It does not materialize
table rows, scan SQLite tables, generate SQLite SQL, register SQLite functions,
or require SQLite fork changes.

## Verification Plan

- Feature-specific MySQL expectation artifact for the already verified
  `CONVERT('ABC' USING BINARY)` string-literal behavior.
- Parser tests for accepted `SELECT`, `FROM DUAL`, `DO`, aliases, source spans,
  and deterministic rejections for deferred forms.
- Runtime tests for no-source, `FROM DUAL`, `DO`, warning count, affected-row
  conventions, row-count state, unsupported operands/forms, unchanged catalog
  generation, unchanged SQLite schema generation, and unchanged `.mylite`
  preamble.
- Existing scalar expression, CAST binary, parser, runtime handle,
  statement-context, storage/VFS, and full check workflows must continue to
  pass.
