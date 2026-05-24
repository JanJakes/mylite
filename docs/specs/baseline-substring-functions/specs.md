# Baseline SUBSTRING/SUBSTR/MID Functions

## Summary

This phase extends MyLite's existing string-slice family with the common MySQL
substring synonyms:

```sql
SUBSTRING(str, pos)
SUBSTRING(str, pos, len)
SUBSTRING(str FROM pos)
SUBSTRING(str FROM pos FOR len)
SUBSTR(...)
MID(...)
```

The supported surface mirrors the current `LEFT()` / `RIGHT()` boundary:
no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projection. A later `baseline-string-function-predicates`
slice reuses this row-scalar expression for narrow descriptor-backed `WHERE`
predicates. This phase does not add general expression predicates, expression
ordering, DML assignment values, generated columns, defaults, or a general
expression engine.

Core behavior:

- `pos` is one-based;
- positive `pos` counts from the start of the string;
- negative `pos` counts back from the end of the string;
- `pos = 0` returns the empty string;
- omitted `len` returns from `pos` through the end;
- `len <= 0` returns the empty string;
- `NULL` in any argument returns `NULL`;
- UTF-8 text is sliced on character boundaries;
- successful supported calls produce no warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing string-slice baseline:
  - `docs/specs/baseline-left-right-functions/specs.md`
  - `packages/libmylite/tests/mysql_baseline_left_right_functions_expectations.sh`
- Official MySQL 8.4 Reference Manual, string functions and operators:
  - <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_substring_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `SUBSTRING`, `SUBSTR`, and `MID` are synonyms for the admitted forms.
- Comma forms and standard `FROM` / `FOR` forms are accepted.
- One-argument and four-argument forms fail at parse time with `1064 / 42000`.
- Function-name whitespace such as `SUBSTRING ('abc', 1)` is not accepted as
  a built-in call in default SQL mode. MyLite therefore uses the existing
  no-space function parsing policy for this baseline, with `IGNORE_SPACE`
  left to the existing parser mode support. MySQL reports this default-mode
  form as `1630 / 42000 FUNCTION db.SUBSTRING does not exist`; MyLite
  currently reports the existing parser syntax diagnostic until generic
  function-name resolution exists for this shape.
- `SUBSTRING('abcdef', 2) = 'bcdef'`.
- `SUBSTRING('abcdef', 2, 3) = 'bcd'`.
- `SUBSTRING('abcdef' FROM 2 FOR 3) = 'bcd'`.
- `SUBSTRING('abcdef', -3) = 'def'`.
- `SUBSTRING('abcdef', -4, 2) = 'cd'`.
- `SUBSTRING('abc', 0)`, `SUBSTRING('abc', 1, 0)`, and
  `SUBSTRING('abc', 1, -1)` return the empty string.
- `SUBSTRING(NULL, 1)`, `SUBSTRING('abc', NULL)`, and
  `SUBSTRING('abc', 1, NULL)` return `NULL`.
- Under an utf8mb4 client/session, substring positions count characters, not
  bytes.
- Numeric and boolean `str` arguments are converted to visible string form, so
  `SUBSTRING(12345, 2, 3) = '234'`, `SUBSTRING(TRUE, 1) = '1'`, and
  `SUBSTRING(FALSE, 1) = '0'`.
- Successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

MySQL also accepts deferred behavior such as noninteger `pos` / `len`
conversion, string numeric positions, binary-string slicing, predicates over
substring expressions outside the later `baseline-string-function-predicates`
subset, nested functions, and very large out-of-range positions with conversion
warnings. Those forms remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits the exact comma and `FROM` / `FOR` forms for
  `SUBSTRING`, `SUBSTR`, and `MID`, preserving source spans for labels and
  diagnostics. Wrong arity remains a syntax error to match MySQL's behavior for
  these names.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite uses SQLite's public expression execution and `substr()`
  function only behind MySQL-compatible guards.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT substring_item[, substring_item ...]
SELECT substring_item[, substring_item ...] FROM DUAL
```

`DO` form:

```sql
DO substring_expr[, substring_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
substring function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
substring_expr:
    substring_name( substring_value , substring_position )
  | substring_name( substring_value , substring_position , substring_length )
  | substring_name( substring_value FROM substring_position )
  | substring_name( substring_value FROM substring_position FOR substring_length )

substring_name:
    SUBSTRING
  | SUBSTR
  | MID

substring_value:
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
  | ( substring_value )

substring_position:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ( substring_position )

substring_length:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ( substring_length )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for the string argument are the same as `LEFT()` /
`RIGHT()`:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

Position and length arguments are intentionally literal-only in this phase.
Descriptor position columns, session scalar position values, string numeric
position conversion, noninteger rounding, binary string values, `BIT`,
approximate numeric values, `ENUM`, `SET`, `JSON`, and spatial values are
deferred.

The following remain outside this phase:

- substring predicate shapes outside the later
  `baseline-string-function-predicates` subset, `HAVING SUBSTRING(...) ...`,
  expression `ORDER BY`, grouping, distinct expression rows, and aggregate
  arguments;
- DML assignment values such as `UPDATE t SET c = SUBSTRING(v, 1, 1)`;
- nested row functions such as `SUBSTRING(CONCAT(v, '-'), 2, 2)`;
- scalar subqueries, correlated subqueries, CTEs, parameters, user variables,
  and stored functions;
- string introducers, national strings, arbitrary binary literals as scalar
  arguments, binary casts as arguments, and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
substring_name(A) ::= SUBSTRING(T). { A = T; }
substring_name(A) ::= SUBSTR(T). { A = T; }
substring_name(A) ::= MID(T). { A = T; }

expression(A) ::= substring_name(T) LPAREN(L) expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, mylite_sql_parser_substring_ast_kind(T), B, C, R);
}
expression(A) ::= substring_name(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, mylite_sql_parser_substring_ast_kind(T), B, C, D, R);
}
expression(A) ::= substring_name(T) LPAREN(L) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, mylite_sql_parser_substring_ast_kind(T), B, C, R);
}
expression(A) ::= substring_name(T) LPAREN(L) expression(B) FROM expression(C) FOR expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, mylite_sql_parser_substring_ast_kind(T), B, C, D, R);
}
```

No wrong-arity productions are added for these functions because MySQL 8.4.9
reports wrong `SUBSTRING()` / `SUBSTR()` / `MID()` arity as a syntax error.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the first argument to a text value using the existing string-slice
   value conversion.
3. Convert the second argument to a signed 64-bit integer literal value, or
   SQL `NULL`.
4. If present, convert the third argument to a signed 64-bit integer literal
   value, or SQL `NULL`.
5. Return SQL `NULL` if any argument is `NULL`.
6. Return `''` if `pos = 0`.
7. Return `''` if a length is present and `len <= 0`.
8. Validate the input as UTF-8 and slice on character boundaries:
   - positive `pos` starts at character `pos`;
   - negative `pos` starts `abs(pos)` characters from the end;
   - start positions outside the string return `''`;
   - omitted length returns through the end of the string;
   - length beyond the remaining string returns through the end.

Table-backed projection planning lowers to standard SQLite expression shapes
with MySQL guards:

```sql
CASE
  WHEN value_expr IS NULL OR pos_expr IS NULL THEN NULL
  WHEN pos_expr = 0 THEN ''
  WHEN pos_expr < 0 AND length(value_expr) + pos_expr < 0 THEN ''
  ELSE substr(value_expr, pos_expr)
END

CASE
  WHEN value_expr IS NULL OR pos_expr IS NULL OR len_expr IS NULL THEN NULL
  WHEN pos_expr = 0 THEN ''
  WHEN len_expr <= 0 THEN ''
  WHEN pos_expr < 0 AND length(value_expr) + pos_expr < 0 THEN ''
  ELSE substr(value_expr, pos_expr, len_expr)
END
```

`value_expr` is either a quoted descriptor column or a bound scalar value.
`pos_expr` and `len_expr` are bound signed 64-bit integers or `NULL`.
Generated identifiers are always quoted and scalar values are bound through
prepared-statement parameters.

SQLite's public `substr()` over TEXT is used because it slices UTF-8 text by
character position for positive and in-range negative start positions. MyLite
wraps it with explicit `NULL`, `pos = 0`, `len <= 0`, and negative-start
beyond-length guards so visible behavior matches the admitted MySQL subset
instead of SQLite edge cases.

## Diagnostics

Supported calls succeed with `warning_count == 0`.

Diagnostics:

- Syntax errors and unsupported grammar: existing parse diagnostic, including
  `1064 / 42000` for wrong arity.
- Default-mode whitespace before `(` in `SUBSTRING`, `SUBSTR`, or `MID`:
  existing parser syntax diagnostic. MySQL reports `1630 / 42000` for this
  missing-function path; that exact diagnostic is deferred to the broader
  generic function-resolution work.
- Unknown descriptor columns: existing unknown-column diagnostic.
- Unsupported string argument expression: deterministic MyLite unsupported
  diagnostic shared with the string-slice family.
- Unsupported position or length argument: deterministic MyLite unsupported
  diagnostic requiring integer, boolean, or `NULL` literals.
- Out-of-range position or length literal: deterministic MyLite unsupported
  diagnostic requiring signed 64-bit range.
- Unsupported descriptor column type: deterministic MyLite unsupported
  diagnostic shared with `LEFT()` / `RIGHT()`.
- Allocation failure: existing `MYLITE_NOMEM` / handle diagnostic behavior.
- Physical SQLite failure: existing runtime SQLite failure diagnostic.

## Performance and Storage

No-source and `DUAL` calls materialize only the scalar output. Row-backed
projection stays close to the SQLite execution path: MyLite resolves
descriptors, emits guarded SQLite `substr()` expressions, and binds scalar
arguments. It does not materialize whole result sets in MyLite for substring
projection, does not query SQLite schema metadata, and does not require a
SQLite fork patch.

## Tests

The implementation must add:

- a MySQL expectation script for scalar, `DUAL`, `DO`, table-backed,
  `FROM` / `FOR`, synonym, `NULL`, UTF-8, signed position, zero-position,
  nonpositive-length, labels, row-count, and deferred/diagnostic behavior;
- parser tests for comma and `FROM` / `FOR` forms, aliases, wrong arity, and
  use as ordinary identifiers where MySQL permits it;
- runtime C tests extending the existing string-slice test coverage for
  no-source, `DUAL`, `DO`, table-backed projection, reopen/preamble safety,
  diagnostics, and unsupported binary/approximate/nested expression forms.
