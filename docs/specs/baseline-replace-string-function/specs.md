# Baseline REPLACE String Function

## Summary

This phase adds a narrow MySQL-compatible scalar string replacement function:

```sql
REPLACE(str, from_str, to_str)
```

The supported surface follows the existing row-scalar string-function envelope:
no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projection. It does not add expression predicates,
expression ordering, DML assignment values, generated columns, defaults, or a
general expression engine.

For this baseline, MyLite implements exact non-overlapping replacement over
NUL-free nonbinary text bytes. This matches MySQL's observed behavior for the
admitted text values: matching is case-sensitive, `NULL` in any argument yields
`NULL`, an empty `from_str` returns `str` unchanged, and successful supported
calls produce no warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-string-search-functions/specs.md`
  - `docs/specs/baseline-trim-string-functions/specs.md`
  - `docs/specs/baseline-concat-ws-function/specs.md`
- Official MySQL 8.4 Reference Manual, string functions and operators:
  - <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_replace_string_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `REPLACE(str, from_str, to_str)` requires exactly three arguments.
- The function name may be separated from `(` by whitespace in default SQL mode.
- Matching is case-sensitive in the default `utf8mb4_0900_ai_ci` context.
- Replacements are applied left-to-right without overlapping matches, so
  `REPLACE('aaaa', 'aa', 'b')` returns `bb`.
- `REPLACE('abc', '', 'x')` returns `abc`; an empty search string does not
  insert the replacement between characters.
- Any `NULL` argument returns `NULL`.
- Integer and boolean arguments are converted to their visible string form
  before replacement.
- Multibyte text is matched as the exact supplied text sequence.
- Successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.
- Wrong-arity forms such as `REPLACE()`, `REPLACE(1)`, `REPLACE(1, 2)`, and
  `REPLACE(1, 2, 3, 4)` fail as syntax errors in MySQL because `REPLACE` also
  introduces a DML statement form. MyLite keeps those malformed shapes as syntax
  errors in this baseline rather than synthesizing a native-function
  parameter-count diagnostic.

MySQL also supports broader expression inputs, binary-string result metadata,
collation-sensitive coercion, DML assignment expressions, nested row functions,
and use in predicates, grouping, ordering, and generated/default expressions.
Those forms remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported `REPLACE()` calls add no warnings.
- Lexer/parser/AST: admits exactly the three-argument function form and
  preserves source spans for result labels and diagnostics. Malformed arities
  remain syntax errors.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated SQLite
  SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections lower to generated SQLite
  expressions over stable physical table names and quoted physical column names.
  MyLite registers a narrow scalar helper through SQLite's public function API
  for MySQL-compatible text replacement. No SQLite fork patch is required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT replace_item[, replace_item ...]
SELECT replace_item[, replace_item ...] FROM DUAL
```

`DO` form:

```sql
DO replace_expr[, replace_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
`REPLACE()` call:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
replace_expr:
    REPLACE ( replace_value , replace_value , replace_value )

replace_value:
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
  | ( replace_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for arguments are:

- integer-family columns;
- exact `DECIMAL`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`.

The following remain outside this phase:

- `WHERE REPLACE(...) ...`, `HAVING REPLACE(...) ...`, expression `ORDER BY`,
  grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = REPLACE(c, 'a', 'b')`;
- nested row functions such as `REPLACE(LOWER(v), 'a', 'b')`;
- scalar subqueries, correlated subqueries, CTEs, parameters, user variables,
  stored functions, and arbitrary expressions;
- string introducers, national strings, arbitrary binary literals, binary
  casts as arguments, binary-string result metadata, and full collation
  metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= REPLACE(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) RPAREN(R).
```

The snippet describes MyLite's admitted subset, not MySQL's full grammar.
Wrong-arity `REPLACE()` forms are left to normal syntax-error handling for this
slice because MySQL reports those shapes as syntax errors.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert each admitted argument to text:
   - ordinary string literal: decoded NUL-free bytes;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar or system-variable value: current string value or
     SQL `NULL`.
3. If any argument is `NULL`, return SQL `NULL`.
4. If `from_str` is empty, return `str` unchanged.
5. Otherwise scan `str` from left to right. At each position, append
   `to_str` and advance by `from_str` length when `from_str` matches exactly;
   otherwise append the current byte and advance by one byte.
6. Return the resulting text. Supported calls produce no warnings.

Table-backed row-scalar evaluation uses the same internal replacement helper
through a SQLite scalar function. Descriptor-backed columns are passed to the
helper as SQLite values only after the planner has resolved the column against
MyLite catalog descriptors and verified the descriptor type family.

The implementation treats strings as byte sequences for matching and copying.
For valid UTF-8 text, this is equivalent to exact multibyte text replacement.
This baseline does not implement collation folding or binary-string result
metadata.

## SQLite Handling

Generated table-backed SQL uses a MyLite-owned helper:

```sql
_mylite_replace(<arg0>, <arg1>, <arg2>)
```

Generated SQL:

- references stable physical table names such as `_mylite_user_table_<id>`;
- quotes all generated SQLite identifiers;
- binds scalar literal/session arguments with prepared-statement parameters;
- passes descriptor columns as quoted physical column references;
- never interpolates SQL literal text into generated SQLite SQL;
- relies only on SQLite's public scalar-function registration API.

The helper receives text arguments from SQLite, returns `NULL` when any input
is `NULL`, reports `SQLITE_NOMEM` for allocation failure, and reports a
deterministic MyLite runtime error for unsupported callback state. It does not
mutate catalog or storage metadata.

## Diagnostics

Supported in-range calls produce `warning_count == 0`.

Diagnostics for this slice are:

- syntax error `1064 / 42000` for malformed `REPLACE()` arity or bare
  `REPLACE` expressions;
- unknown-column diagnostics for unqualified or qualified names outside a
  table-backed row-scalar context or names missing from the descriptor;
- MyLite unsupported diagnostics for unsupported argument shapes, nested row
  functions, scalar subqueries in table-backed rows, binary string or `BIT`
  descriptor columns, approximate numeric descriptor columns, and unsupported
  descriptor type families;
- MyLite unsupported diagnostics for embedded NUL bytes in scalar string
  literals;
- allocation diagnostics for result construction or planner allocation failure;
- physical SQLite/runtime diagnostics for callback owner lookup, SQLite
  registration failure, or unexpected SQLite execution failure.

## Tests

Fast C tests live under `packages/libmylite/tests/` and should use a focused
`runtime_replace_string_function` binary if that keeps the coverage clearer
than extending an existing string-function test.

Coverage must include:

- scalar values, empty search string, case-sensitive matching, non-overlap, and
  `NULL` propagation;
- integer and boolean argument conversion;
- `SELECT ... FROM DUAL`, function-name whitespace, labels, aliases, `DO`,
  `ROW_COUNT()`, and `@@warning_count`;
- table-backed projection over descriptor integer, exact decimal, nonbinary
  string, baseline `TEXT`, `YEAR`, and temporal columns;
- table-backed `WHERE`, descriptor `ORDER BY`, and `LIMIT` envelope reuse;
- unknown columns, unsupported descriptor families, nested row functions, and
  unsupported scalar subqueries in table-backed context;
- malformed arity and bare keyword syntax errors;
- close/reopen persistence for source rows and deterministic readback;
- no public API changes and no `.mylite` file-format mutation beyond ordinary
  test data.

The MySQL expectation script records every user-visible behavior introduced by
this phase and must be run against MySQL 8.4.9 before marking the feature done.
