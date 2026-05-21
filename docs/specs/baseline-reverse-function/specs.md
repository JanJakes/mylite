# Baseline REVERSE String Function

## Summary

This phase adds a narrow MySQL-compatible scalar string reversal function:

```sql
REVERSE(str)
```

The supported surface follows the existing row-scalar string-function envelope:
no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projection. It does not add expression predicates,
expression ordering, DML assignment values, generated columns, defaults, or a
general expression engine.

For this baseline, MyLite reverses NUL-free nonbinary text by UTF-8 character
sequence, not by raw byte order. `NULL` returns `NULL`, integer and boolean
inputs use their visible string form, and successful supported calls produce no
warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-substring-functions/specs.md`
  - `docs/specs/baseline-replace-string-function/specs.md`
- Official MySQL 8.4 Reference Manual, string functions and operators:
  - <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_reverse_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `REVERSE(str)` requires exactly one argument.
- The function name may be separated from `(` by whitespace in default SQL
  mode.
- `REVERSE('abc')` returns `cba`.
- `REVERSE('')` returns the empty string.
- `REVERSE(NULL)` returns `NULL`.
- Integer and boolean arguments are converted to their visible string form
  before reversal, so `REVERSE(12345)` returns `54321`,
  `REVERSE(-7)` returns `7-`, `REVERSE(TRUE)` returns `1`, and
  `REVERSE(FALSE)` returns `0`.
- Multibyte UTF-8 text is reversed by character sequence. Observed examples:
  `REVERSE('éa')` returns `aé` and `REVERSE('🙂a')` returns `a🙂`.
- `REVERSE(CAST('AB' AS BINARY))` returns a binary-string result with bytes
  reversed. This phase does not admit binary-string inputs or preserve binary
  result metadata.
- Successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.
- Wrong-arity forms such as `REVERSE()` and `REVERSE('a', 'b')` fail as syntax
  errors in MySQL 8.4.9. MyLite keeps those malformed shapes as syntax errors
  in this baseline.

MySQL also supports broader expression inputs, binary-string result metadata,
collation metadata, DML assignment expressions, nested row functions, and use
in predicates, grouping, ordering, and generated/default expressions. Those
forms remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported `REVERSE()` calls add no warnings.
- Lexer/parser/AST: admits exactly the one-argument function form and preserves
  source spans for result labels and diagnostics. Malformed arities remain
  syntax errors.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated SQLite
  SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections lower to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite registers a narrow scalar helper through SQLite's public
  function API for MySQL-compatible UTF-8 text reversal. No SQLite fork patch is
  required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT reverse_item[, reverse_item ...]
SELECT reverse_item[, reverse_item ...] FROM DUAL
```

`DO` form:

```sql
DO reverse_expr[, reverse_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
`REVERSE()` call:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
reverse_expr:
    REVERSE ( reverse_value )

reverse_value:
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
  | ( reverse_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for arguments are:

- integer-family columns;
- exact `DECIMAL`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`.

The following remain outside this phase:

- `WHERE REVERSE(...) ...`, `HAVING REVERSE(...) ...`, expression `ORDER BY`,
  grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = REVERSE(c)`;
- nested row functions such as `REVERSE(LOWER(v))`;
- scalar subqueries, correlated subqueries, CTEs, parameters, user variables,
  stored functions, and arbitrary expressions;
- string introducers, national strings, arbitrary binary literals, binary
  casts as arguments, binary-string result metadata, and full collation
  metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= REVERSE(T) LPAREN expression(B) RPAREN(R).
```

The snippet describes MyLite's admitted subset, not MySQL's full grammar.
Wrong-arity `REVERSE()` forms are left to normal syntax-error handling for this
slice because MySQL reports those shapes as syntax errors.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the admitted argument to text:
   - ordinary string literal: decoded NUL-free bytes;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar or system-variable value: current string value or
     SQL `NULL`.
3. If the argument is `NULL`, return SQL `NULL`.
4. Validate the text as UTF-8 for this nonbinary baseline.
5. Copy complete UTF-8 byte sequences into the result in reverse character
   order.
6. Return the resulting text. Supported calls produce no warnings.

Table-backed row-scalar evaluation uses the same internal reversal helper
through a SQLite scalar function. Descriptor-backed columns are passed to the
helper as SQLite values only after the planner has resolved the column against
MyLite catalog descriptors and verified the descriptor type family.

This baseline does not implement byte-reversal for binary strings and does not
preserve binary-string result metadata. Binary strings and `BIT` descriptor
columns are rejected before execution.

## SQLite Handling

Generated table-backed SQL uses a MyLite-owned helper:

```sql
_mylite_reverse_utf8(<arg>)
```

Generated SQL:

- references stable physical table names such as `_mylite_user_table_<id>`;
- quotes all generated SQLite identifiers;
- binds scalar literal/session arguments with prepared-statement parameters;
- passes descriptor columns as quoted physical column references;
- never interpolates decoded string literal contents into generated SQL.

The helper is registered with `sqlite3_create_function_v2()` through MyLite's
existing runtime registration wrapper. This is a public SQLite extension API
use. No SQLite fork patch, virtual table, generated index, trigger, or storage
format change is needed.

## Diagnostics

Supported successful calls return no warnings.

Diagnostics follow existing MyLite statement-context policies:

- malformed `REVERSE()` syntax, including zero or two arguments: syntax error;
- bare `REVERSE` without `()` is not admitted by this baseline's scalar
  projection envelope and returns the existing MyLite unsupported no-source
  select diagnostic, although MySQL reports it as an unknown column;
- unsupported scalar argument kinds: deterministic MyLite unsupported-feature
  diagnostic;
- unknown no-source identifier argument: MySQL-compatible unknown-column
  diagnostic;
- unknown row-backed descriptor column: MySQL-compatible unknown-column
  diagnostic in the field-list context;
- unsupported row-backed descriptor type: deterministic MyLite
  unsupported-feature diagnostic;
- decoded string literals containing NUL bytes: deterministic MyLite
  unsupported-feature diagnostic;
- invalid UTF-8 in the admitted nonbinary text path: deterministic MyLite
  runtime diagnostic;
- physical SQLite helper registration or execution failure: existing physical
  SQLite failure path;
- allocation failure: existing out-of-memory diagnostic.

No new public API misuse cases are introduced.

## Performance And Storage

Scalar no-source calls allocate only the result text and any required decoded
literal/session argument text. Table-backed calls are pushed into SQLite as a
scalar function over the filtered, ordered, and limited row stream, so MyLite
does not materialize entire tables to reverse strings. The helper work is
linear in the input byte length for each emitted row.

The catalog, descriptor cache, file preamble, and SQLite payload are not
modified by this read-only feature.

## Test Plan

Add fast C tests under `packages/libmylite/tests/` and register them with a
dotted CTest name. Expected user-visible behavior is captured and verified by
the MySQL 8.4.9 expectation script.

Coverage:

- scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO` with string, empty string,
  integer, signed integer, boolean, `NULL`, session scalar, and system-variable
  inputs;
- UTF-8 multibyte reversal;
- table-backed projection over integer-family, exact `DECIMAL`, nonbinary
  string, `TEXT`, `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`
  descriptor columns;
- row-scalar envelope with alias labels, `WHERE`, `ORDER BY`, and `LIMIT`;
- reopen persistence around a table-backed query;
- wrong-arity syntax errors;
- unknown scalar and row-backed columns;
- unsupported binary-string, `BIT`, approximate numeric, and nested row
  function arguments;
- warning count, affected-row convention, and absence of write side effects.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/functions-string.md` from
unsupported to partial support. Do not claim full MySQL `REVERSE()` coverage,
binary-string metadata, use in every expression context, or generated/default
expression support.
