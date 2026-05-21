# Baseline QUOTE Function

## Summary

This phase adds a narrow MySQL-compatible `QUOTE()` string function:

```sql
QUOTE(str)
```

The supported surface follows the current MyLite scalar and row-scalar
projection envelope: no-source `SELECT`, `SELECT ... FROM DUAL`, `DO`, and
single-table row-scalar `SELECT` projection. It does not add predicate,
ordering, grouping, DML assignment, generated-column, default-expression, or
general expression support.

`QUOTE()` returns text that can be embedded as a SQL string literal for the
supported input subset. Non-`NULL` arguments are enclosed in single quotes, and
the result escapes backslash, single quote, ASCII NUL, and Control+Z with a
preceding backslash. A SQL `NULL` argument returns the text `NULL`, not SQL
`NULL`.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, string functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- Existing string function designs:
  - `docs/specs/baseline-reverse-function/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-strcmp-function/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_quote_function_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `QUOTE(str)` requires exactly one argument. Zero- and two-argument calls fail
  with `1582 / 42000`.
- Whitespace between `QUOTE` and `(` is accepted in default SQL mode.
- `QUOTE('abc')` returns `'abc'`; `QUOTE('')` returns `''`.
- `QUOTE(NULL)` returns the text `NULL`. It is not SQL `NULL`.
- Numeric and boolean arguments are converted to visible string form first:
  `QUOTE(123)` returns `'123'`, `QUOTE(-7)` returns `'-7'`,
  `QUOTE(TRUE)` returns `'1'`, and `QUOTE(FALSE)` returns `'0'`.
- Exact fixed decimal arguments keep their visible scale, so
  `QUOTE(1.50)` returns `'1.50'`.
- The result escapes only the verified SQL-escaping bytes in this phase:
  backslash becomes `\\`, single quote becomes `\'`, ASCII NUL becomes `\0`,
  and Control+Z becomes `\Z`.
- Other control characters such as tab, newline, carriage return, and
  backspace are preserved as bytes inside the quoted result.
- With `NO_BACKSLASH_ESCAPES`, string-literal input parsing changes, but the
  `QUOTE()` result still uses backslash escaping.
- Table-backed `VARCHAR`, `TEXT`, integer, `DECIMAL`, `YEAR`, `DATE`, `TIME`,
  `DATETIME`, and `TIMESTAMP` values are converted to their visible value before
  quoting in the observed supported cases.
- Successful supported calls produce `@@warning_count = 0`; a preceding scalar
  `SELECT` makes `ROW_COUNT()` return `-1`, while `DO QUOTE(...)` makes
  `ROW_COUNT()` return `0`.

MySQL also supports broader expression inputs, binary-string inputs, approximate
numeric inputs, user variables, parameters, use inside predicates, DML
assignment expressions, grouping, ordering, and metadata behavior. Those forms
remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values. Supported `QUOTE(NULL)` is returned as text `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported `QUOTE()` calls add no warnings.
- Lexer/parser/AST: admits one-argument `QUOTE()` and explicit wrong-arity AST
  nodes so MyLite can return MySQL-compatible native-function arity errors.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated SQLite
  SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projection lowers to generated SQLite
  expressions over stable physical table names and quoted physical column names.
  MyLite registers a narrow scalar helper through SQLite's public function API
  for supported SQL quoting. No SQLite fork patch is required.
- Result builder: uses existing row result conventions and source-span labels or
  explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble and
  shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT quote_item[, quote_item ...]
SELECT quote_item[, quote_item ...] FROM DUAL
```

`DO` form:

```sql
DO quote_expr[, quote_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
`QUOTE()` call:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
quote_expr:
    QUOTE ( quote_value )

quote_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | fixed_decimal_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | descriptor_column_reference        -- table-backed SELECT only
  | ( quote_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families are:

- integer-family columns;
- exact `DECIMAL`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`.

The following remain outside this phase:

- binary string inputs and `BIT` columns;
- approximate numeric scalar or row-backed inputs;
- non-`utf8mb4` character-set conversion, introducers, explicit collations, and
  binary result metadata;
- nested row functions, scalar subqueries, correlated subqueries, CTEs,
  parameters, user variables, stored functions, and arbitrary expressions;
- `WHERE QUOTE(...) ...`, `HAVING QUOTE(...) ...`, expression `ORDER BY`,
  grouping, distinct expression rows, aggregate arguments, and DML assignments.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= QUOTE(T) LPAREN expression(B) RPAREN(R).

expression(A) ::= QUOTE(T) LPAREN RPAREN(R).
expression(A) ::= QUOTE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).

identifier(A) ::= QUOTE(T).
```

The snippet describes MyLite's admitted subset, not MySQL's full grammar.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the admitted argument to a byte string or SQL `NULL`:
   - ordinary string literal: decoded bytes using the current SQL mode, with
     NUL bytes admitted for this function;
   - integer literal: canonical signed decimal text;
   - fixed decimal literal: canonical fixed decimal text preserving the
     observed scale;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar or system-variable value: current text value or
     SQL `NULL`.
3. If the argument is SQL `NULL`, return text `NULL`.
4. Otherwise allocate the quoted result, prepend and append single quotes, and
   escape only backslash, single quote, ASCII NUL, and Control+Z.
5. Return the resulting text. Supported calls produce no warnings.

Table-backed row-scalar evaluation uses the same internal helper through a
SQLite scalar function. Descriptor-backed columns are passed to the helper only
after the planner resolves the column against MyLite catalog descriptors and
verifies the descriptor type family.

This baseline does not implement binary-string argument handling or binary
result metadata. Binary strings, `BIT` descriptor columns, and approximate
numeric columns are rejected before execution.

## SQLite Handling

Generated table-backed SQL uses a MyLite-owned helper:

```sql
_mylite_quote_sql(<arg>)
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

- wrong argument count: MySQL-compatible `1582 / 42000` native-function
  argument-count error;
- missing default schema, unknown schema/table, reserved table names, and
  unsupported object kinds through existing row-scalar source diagnostics;
- unknown no-source identifier argument and unknown row-backed descriptor
  columns: MySQL-compatible unknown-column diagnostics in field-list context;
- unsupported scalar argument kinds:
  `QUOTE() supports only string, integer, DECIMAL, boolean, NULL, session scalar, and system variable arguments`;
- unsupported row-backed descriptor type: deterministic MyLite
  unsupported-feature diagnostic;
- physical SQLite helper registration or execution failure: existing physical
  SQLite failure path;
- allocation failure: existing out-of-memory diagnostic.

No new public API misuse cases are introduced.

## Performance And Storage

Scalar no-source calls allocate only the decoded input, if needed, and the
quoted result. Table-backed calls are pushed into SQLite as a scalar function
over the filtered, ordered, and limited row stream, so MyLite does not
materialize entire tables to quote values. The helper work is linear in the
input byte length for each emitted row.

The feature is read-only: it does not mutate catalog rows, descriptor versions,
descriptor caches, physical tables, indexes, the `.mylite` preamble, or shifted
SQLite payload bytes.

## Test Plan

Tests must cover:

- no-source, `DUAL`, labels, whitespace before `(`, and `DO`;
- `QUOTE(NULL)` returning text `NULL`, not SQL `NULL`;
- empty strings, ordinary strings, quotes, backslashes, NUL, Control+Z, other
  control characters, and UTF-8 text;
- integer, signed integer, fixed decimal, and boolean conversion;
- current SQL-mode parsing interaction with `NO_BACKSLASH_ESCAPES`;
- row count, warning count, and no result rows for `DO`;
- zero- and two-argument native function count errors;
- table-backed descriptor arguments covering nonbinary strings, `TEXT`,
  integer, `DECIMAL`, `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`
  columns, including nullable rows returning text `NULL`;
- row envelope preservation with existing `WHERE`, descriptor `ORDER BY`, and
  `LIMIT`;
- unknown column diagnostics in row-scalar projection;
- deterministic rejection for binary columns, approximate columns, unsupported
  hex/binary literals, parameters, nested calls, predicate use, DML assignment
  use, and ordering expression use;
- reopen/file-format safety through a table-backed projection against a
  file-backed database;
- independent handles if a new runtime test binary is added.

Verification commands:

1. `packages/libmylite/tests/mysql_baseline_quote_function_expectations.sh`
2. focused parser/runtime CTest entries for `QUOTE()`
3. `cmake --workflow --preset check`

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/functions-string.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/type-system-literals-conversion.md`

Do not claim broad binary strings, approximate numbers, arbitrary expressions,
predicates, DML assignments, generated/default expressions, full metadata, or
collation behavior.
