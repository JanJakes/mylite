# Baseline String Padding Functions

## Summary

This phase adds a narrow MySQL string-construction slice:

```sql
LPAD(str, len, padstr)
RPAD(str, len, padstr)
REPEAT(str, count)
SPACE(count)
```

The supported surface covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts. It deliberately
does not add expression predicates, expression ordering, grouping expressions,
DML assignment expressions, generated columns, defaults, or arbitrary nested
expression planning.

Core supported behavior:

- `LPAD()` and `RPAD()` return `str` padded or shortened to `len` characters;
- `REPEAT()` repeats `str` `count` times;
- `SPACE()` returns `count` ASCII spaces;
- `NULL` inputs return `NULL` except non-`NULL` `SPACE()` count handling;
- nonpositive `REPEAT()` and `SPACE()` counts return the empty string;
- negative `LPAD()` / `RPAD()` target lengths return `NULL`;
- zero `LPAD()` / `RPAD()` target lengths return the empty string;
- empty pad strings follow MySQL's visible behavior: if padding is needed,
  `LPAD()` / `RPAD()` return the empty string; otherwise the input is shortened
  to `len` characters;
- utf8mb4 text is counted and truncated on UTF-8 character boundaries;
- successful supported calls produce no warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-scalar-expression-projection/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-left-right-functions/specs.md`
  - `docs/specs/baseline-substring-functions/specs.md`
  - `docs/specs/baseline-string-search-functions/specs.md`
  - `docs/specs/baseline-replace-string-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
  - character set and collation of function results:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_string_padding_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `LPAD()` and `RPAD()` require exactly three arguments and wrong arity reports
  `1582 / 42000`;
- `SPACE()` requires exactly one argument and wrong arity reports
  `1582 / 42000`;
- `REPEAT()` accepts exactly two arguments in this grammar position; zero, one,
  or three arguments fail as syntax errors with `1064 / 42000`;
- function-name whitespace such as `LPAD ('hi',4,'?')`, `RPAD ('hi',4,'?')`,
  `REPEAT ('x',2)`, and `SPACE (2)` is accepted in default SQL mode;
- `LPAD('hi',4,'??') = '??hi'`, `LPAD('hi',1,'??') = 'h'`,
  `RPAD('hi',5,'?') = 'hi???'`, and `RPAD('hi',1,'?') = 'h'`;
- `REPEAT('MySQL',3) = 'MySQLMySQLMySQL'`;
- `SPACE(3)` returns three ASCII spaces;
- `REPEAT('x',0)`, `REPEAT('x',-1)`, `SPACE(0)`, and `SPACE(-1)` return the
  empty string;
- `LPAD('hi',0,'?')` and `RPAD('hi',0,'?')` return the empty string;
- `LPAD('hi',-1,'?')` and `RPAD('hi',-1,'?')` return `NULL`;
- any `NULL` `LPAD()` / `RPAD()` argument returns `NULL`; any `NULL`
  `REPEAT()` argument returns `NULL`; `SPACE(NULL)` returns `NULL`;
- `LPAD('hi',4,'')` and `RPAD('hi',4,'')` return the empty string, while
  `LPAD('hi',2,'')` and `RPAD('hi',2,'')` return `hi`;
- `LPAD('hello',2,'?')` and `RPAD('hello',2,'?')` return `he`;
- `LPAD('hi',5,'abc') = 'abchi'` and `RPAD('hi',5,'abc') = 'hiabc'`;
- under an utf8mb4 client/session, `LPAD('é',3,'🙂') = '🙂🙂é'`,
  `RPAD('é',3,'🙂') = 'é🙂🙂'`, and `REPEAT('é🙂',2) = 'é🙂é🙂'`;
- numeric and boolean string arguments are converted to visible string form,
  so `LPAD(123,5,'0') = '00123'`, `RPAD(-7,4,'x') = '-7xx'`,
  `REPEAT(TRUE,3) = '111'`, and `REPEAT(FALSE,2) = '00'`;
- boolean count/length values are admitted as `1` and `0`;
- successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

MySQL also accepts deferred behavior such as decimal or string length/count
conversion, binary-string padding/repetition, expression counts, predicates
over these functions, nested functions, and non-ASCII collation-sensitive result
typing. Those forms remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits exact `LPAD()`, `RPAD()`, `REPEAT()`, and `SPACE()`
  expressions while preserving source spans for labels and diagnostics.
  `LPAD()` / `RPAD()` / `SPACE()` wrong-arity nodes are used where MySQL reports
  native-function parameter-count diagnostics; `REPEAT()` wrong arity remains a
  syntax error.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite registers private scalar helpers through SQLite's public
  function API for row-backed `LPAD()`, `RPAD()`, `REPEAT()`, and `SPACE()`
  semantics. No SQLite fork patch is required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT string_padding_item[, string_padding_item ...]
SELECT string_padding_item[, string_padding_item ...] FROM DUAL
```

`DO` form:

```sql
DO string_padding_expr[, string_padding_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
string-padding function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
string_padding_expr:
    LPAD ( string_padding_value , string_padding_count , string_padding_value )
  | RPAD ( string_padding_value , string_padding_count , string_padding_value )
  | REPEAT ( string_padding_value , string_padding_count )
  | SPACE ( string_padding_count )

string_padding_value:
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
  | ( string_padding_value )

string_padding_count:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ( string_padding_count )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for value and pad-string arguments are:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

Length and count arguments are intentionally literal-only in this phase.
Descriptor count columns, session scalar count values, string count values,
noninteger rounding, binary string values, `BIT`, approximate numeric values,
`ENUM`, `SET`, `JSON`, and spatial values are deferred.

The following remain outside this phase:

- `WHERE LPAD(column, 3, '0') ...`, `HAVING REPEAT(...) ...`, expression
  `ORDER BY`, grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = LPAD(v, 5, '0')`;
- nested row functions such as `LPAD(CONCAT(v, '-'), 5, '0')`;
- scalar subqueries, correlated subqueries, CTEs, joins beyond the already
  supported row-scalar source envelope, parameters, user variables, and stored
  functions;
- string introducers, national strings, arbitrary binary literals as scalar
  arguments, binary casts as arguments, and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::=
    LPAD(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_LPAD_FUNCTION, B, C, D, R);
}
expression(A) ::=
    RPAD(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_RPAD_FUNCTION, B, C, D, R);
}
expression(A) ::= REPEAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_REPEAT_FUNCTION, B, C, R);
}
expression(A) ::= SPACE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SPACE_FUNCTION, B, R);
}
```

Wrong-arity `LPAD()` / `RPAD()` / `SPACE()` forms produce function-argument-count
AST nodes. Wrong-arity `REPEAT()` forms are left as syntax errors to match the
observed MySQL 8.4.9 behavior for this reserved keyword/function name.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert string arguments:
   - ordinary string literal: decoded UTF-8 bytes, provided they are NUL-free;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar value or system variable: its existing visible
     string value or SQL `NULL`.
3. Convert length/count arguments to signed 64-bit integer literal values, or
   SQL `NULL`.
4. For `LPAD()` / `RPAD()`, return SQL `NULL` when any argument is `NULL` or
   `len < 0`; return `''` when `len == 0`; validate `str` and `padstr` as
   UTF-8; if `str` has at least `len` characters, return its leftmost `len`
   characters; if `padstr` is empty and padding is needed, return `''`;
   otherwise repeat and truncate `padstr` on character boundaries on the
   selected side.
5. For `REPEAT()`, return SQL `NULL` when either argument is `NULL`; return
   `''` when `count <= 0`; validate `str` as UTF-8 and concatenate `count`
   copies, checking for allocation overflow.
6. For `SPACE()`, return SQL `NULL` when `count` is `NULL`; return `''` when
   `count <= 0`; allocate `count` ASCII spaces, checking for allocation
   overflow.

Table-backed projection planning lowers to private SQLite helper functions:

```sql
_mylite_lpad(value_expr, len_expr, pad_expr)
_mylite_rpad(value_expr, len_expr, pad_expr)
_mylite_repeat(value_expr, count_expr)
_mylite_space(count_expr)
```

Each generated expression is built only from descriptors, quoted identifiers,
and bound scalar parameters. Literal values from user SQL are not interpolated
into generated SQLite SQL.

## Diagnostics

Supported successful calls return no warnings.

| Condition | Diagnostic |
| --- | --- |
| unsupported argument shape | deterministic MyLite unsupported error |
| `LPAD()` / `RPAD()` / `SPACE()` wrong arity | `1582 / 42000` native-function parameter count |
| `REPEAT()` wrong arity | `1064 / 42000` syntax error |
| unsupported string literal with embedded NUL | deterministic MyLite unsupported error |
| invalid UTF-8 text in admitted text value | deterministic MyLite runtime error |
| integer count/length literal outside signed 64-bit range | deterministic MyLite unsupported error |
| SQLite private helper allocation failure | `MYLITE_NOMEM` / SQLite `SQLITE_NOMEM` propagation |
| SQLite private helper misuse | physical SQLite failure mapped through existing execution diagnostics |

Unsupported but syntactically parsed forms must be rejected before generated
SQLite SQL is prepared when possible. Row-backed helper failures still map to
the existing physical SQLite failure path.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/functions-string.md` to mark
`LPAD()`, `RPAD()`, `REPEAT()`, and `SPACE()` as limited. Do not claim support
for binary string result typing, full collation coercion, expression predicates,
ordering/grouping expressions, DML assignments, parameters, general expression
counts, or noninteger rounding.

## Test Plan

- MySQL expectation script verifies MySQL 8.4.9 behavior for scalar values,
  wrong arity, `NULL`, nonpositive counts/lengths, empty pad strings,
  multibyte values, numeric/boolean visible string conversion, accepted
  deferred forms, row-backed source values, `ROW_COUNT()`, and warning count.
- Parser tests cover function AST nodes, spans, spaced function calls, wrong
  arity, `REPEAT()` syntax errors, `DO` admission, and keyword identifier
  fallback where allowed.
- Runtime C tests cover:
  - no-source, `DUAL`, and `DO` evaluation;
  - row-backed projection over descriptor integer, `DECIMAL`, nonbinary string,
    baseline `TEXT`, `YEAR`, and temporal columns;
  - `NULL`, zero, negative, empty pad string, multibyte, numeric, boolean, and
    alias/label behavior;
  - unsupported binary, `BIT`, approximate, expression, count-column, nested,
    predicate, and DML-assignment forms;
  - reopen persistence and `.mylite` preamble preservation;
  - independent file-backed handles;
  - zero-initialized cleanup through existing result and planner deinit paths.

## Tasks

See `tasks.md`.
