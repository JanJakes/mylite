# Baseline LEFT/RIGHT Functions

## Summary

This phase adds a narrow, common MySQL string-slice family:

```sql
LEFT(str, len)
RIGHT(str, len)
```

The supported slice covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts. It fixes the
current `LEFT` keyword/function conflict without weakening `LEFT JOIN` parsing,
and adds the matching `RIGHT()` function token. The current extension also
admits direct descriptor-backed `WHERE` comparison, `IS [NOT] NULL`,
`[NOT] BETWEEN`, and non-grouped single-table `ORDER BY LEFT(...)` /
`ORDER BY RIGHT(...)` expression contexts. It deliberately does not add DML
assignment values, generated columns, defaults, grouping expressions, or
arbitrary nested expression planning.

Core supported behavior:

- `LEFT(str, len)` returns the leftmost `len` characters;
- `RIGHT(str, len)` returns the rightmost `len` characters;
- `NULL` in either argument returns `NULL`;
- `len <= 0` returns the empty string;
- `len` greater than the character length returns the whole string;
- utf8mb4 text is sliced on UTF-8 character boundaries;
- successful supported calls produce no warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-string-case-functions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_left_right_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `LEFT()` and `RIGHT()` require exactly two arguments; zero, one, or three
  arguments fail at parse time with `1064 / 42000`;
- function-name whitespace such as `LEFT ('abc', 1)` is accepted in default SQL
  mode;
- `LEFT('foobarbar', 5) = 'fooba'` and `RIGHT('foobarbar', 3) = 'bar'`;
- `LEFT('abc', 0)`, `RIGHT('abc', 0)`, `LEFT('abc', -1)`, and
  `RIGHT('abc', -1)` return the empty string;
- `LEFT('abc', +2) = 'ab'` and `RIGHT('abc', +2) = 'bc'`;
- `LEFT('abc', 9)` and `RIGHT('abc', 9)` return `abc`;
- `LEFT(NULL, 1)`, `LEFT('abc', NULL)`, `RIGHT(NULL, 1)`, and
  `RIGHT('abc', NULL)` return `NULL`;
- under an utf8mb4 client/session, `LEFT('é🙂', 1) = 'é'` and
  `RIGHT('é🙂', 1) = '🙂'`;
- numeric and boolean `str` arguments are converted to visible string form, so
  `LEFT(12345, 2) = '12'`, `RIGHT(-12345, 3) = '345'`,
  `LEFT(TRUE, 1) = '1'`, and `RIGHT(FALSE, 1) = '0'`;
- `CHAR` trailing spaces are stripped in the current default SQL mode before
  these functions see the value;
- successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

MySQL also accepts deferred behavior such as noninteger `len` conversion,
string `len` conversion, binary-string slicing, broad predicate contexts, and
arbitrary nested functions. Those forms remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits exact two-argument `LEFT()` and `RIGHT()`
  expressions while preserving source spans for labels and diagnostics. Wrong
  arity remains a syntax error to match MySQL's behavior for these names.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite uses SQLite's public expression execution and `substr()`
  function for row-backed text slicing. No SQLite fork patch is required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT string_slice_item[, string_slice_item ...]
SELECT string_slice_item[, string_slice_item ...] FROM DUAL
```

`DO` form:

```sql
DO string_slice_expr[, string_slice_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
string-slice function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column_or_direct_string_slice_expr [ASC | DESC]]
[LIMIT row_count]
```

Supported `WHERE` predicates additionally admit a direct `LEFT(...)` or
`RIGHT(...)` call as the left side of comparison, `IS [NOT] NULL`, and
`[NOT] BETWEEN` predicates when the function arguments fit the same row-scalar
value and length envelopes. Supported expression ordering is limited to
non-grouped single-table `SELECT ORDER BY LEFT(...)` / `RIGHT(...)` keys over
the same direct expression envelope.

The admitted expression shape is:

```sql
string_slice_expr:
    LEFT ( string_slice_value , string_slice_length )
  | RIGHT ( string_slice_value , string_slice_length )

string_slice_value:
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
  | ( string_slice_value )

string_slice_length:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | supported_integer_scalar_function
  | descriptor_integer_column_reference        -- table-backed SELECT only
  | supported_integer_row_scalar_expression    -- table-backed SELECT only
  | ( string_slice_length )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for the string argument are:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

The length argument admits signed-64 integer, boolean, and `NULL` scalar
values, direct supported integer-domain numeric and string-length scalar functions, descriptor integer
columns, and the existing supported table-backed integer row-scalar expression
subset. That row-scalar subset includes integer arithmetic over admitted
operands, supported integer-domain numeric functions, string-length functions,
`UNIX_TIMESTAMP()`, and numeric temporal extractors. Warning-producing
string/noninteger conversion, binary string values, `BIT`, approximate numeric
values, `ENUM`, `SET`, `JSON`, spatial values, parameters, and user variables
remain deferred.

The following remain outside this phase:

- bare truth predicates, `HAVING LEFT(...) ...`, grouped expression `ORDER BY`,
  grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = LEFT(v, 1)`;
- arbitrary nested row functions outside the supported row-scalar value and
  integer argument subsets;
- scalar subqueries, correlated subqueries, CTEs, joins beyond the already
  supported row-scalar source envelope, parameters, user variables, and stored
  functions;
- string introducers, national strings, arbitrary binary literals as scalar
  arguments, binary casts as arguments, and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= LEFT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_LEFT_FUNCTION, B, C, R);
}
expression(A) ::= RIGHT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_RIGHT_FUNCTION, B, C, R);
}
```

No wrong-arity productions are added for these functions because MySQL 8.4.9
reports wrong `LEFT()` / `RIGHT()` arity as a syntax error.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the first argument to a text value:
   - ordinary string literal: decoded UTF-8 bytes, provided they are NUL-free;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar value or system variable: its existing visible
     string value or SQL `NULL`.
3. Convert the second argument through the supported integer argument envelope,
   yielding a signed 64-bit integer value or SQL `NULL`.
4. Return SQL `NULL` if either argument is `NULL`.
5. Return `''` if `len <= 0`.
6. Validate the input as UTF-8 and slice on character boundaries.
7. Return the selected byte range through existing result APIs.

Table-backed projection planning lowers to standard SQLite expression shapes:

```sql
CASE
  WHEN value_expr IS NULL OR len_expr IS NULL THEN NULL
  WHEN len_expr <= 0 THEN ''
  ELSE substr(value_expr, 1, len_expr)
END

CASE
  WHEN value_expr IS NULL OR len_expr IS NULL THEN NULL
  WHEN len_expr <= 0 THEN ''
  ELSE substr(value_expr, -len_expr)
END
```

`value_expr` is either a quoted descriptor column, a generated expression from
the supported value subset, or a bound scalar value. `len_expr` is a bound
signed 64-bit integer/`NULL`, quoted integer descriptor column, or generated
expression from the supported integer subset. The builder emits the value and
length expressions separately for each occurrence and binds all scalar values
through prepared-statement parameters. Generated identifiers are always quoted.

SQLite's public `substr()` over TEXT is used for row-backed projection because
it slices UTF-8 text by character position, which matches the admitted MySQL
text subset. Binary and byte-slice semantics are deferred rather than routed
through this text-oriented expression path.

## Diagnostics

Supported calls succeed with `warning_count == 0`.

Diagnostics:

- Syntax errors and unsupported grammar: existing parse diagnostic, including
  `1064 / 42000` for wrong `LEFT()` / `RIGHT()` arity.
- Unknown descriptor columns: existing unknown-column diagnostic.
- Unsupported string argument expression: deterministic MyLite unsupported
  diagnostic.
- Unsupported length expression: deterministic MyLite unsupported diagnostic.
- String literals with decoded embedded NUL bytes: deterministic MyLite
  unsupported diagnostic.
- Invalid UTF-8 text in scalar evaluation: deterministic runtime diagnostic.
- Integer length literal outside the signed 64-bit range: deterministic MyLite
  unsupported diagnostic.
- Physical SQLite failures: existing physical row error path unless a more
  specific diagnostic is already set.
- Allocation failures: `MYLITE_NOMEM` with existing no-memory diagnostics.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/functions-string.md` mark `LEFT()`
and `RIGHT()` as limited support for no-source, `DUAL`, `DO`, and single-table
row-scalar `SELECT` projection plus documented direct predicate and
non-grouped ordering contexts. Broader predicate, DML, grouped ordering,
binary, coercion, and metadata behavior remains documented as unsupported.

## Performance And Storage Impact

No table data is materialized in MyLite for table-backed `LEFT()` / `RIGHT()`.
Generated SQLite projections evaluate the slice per row inside SQLite, with
ordinary parameter binding for scalar arguments. MyLite-owned scalar/`DO`
evaluation only handles single expression values and does not scan tables.

The feature changes no public ABI, catalog format, user table descriptors,
physical table naming, VFS behavior, or `.mylite` file preamble.
