# Baseline String Function Predicates

## Goal

Allow a narrow, descriptor-backed `WHERE` predicate surface for string length
and substring functions that are already supported in row-scalar projection:

```sql
WHERE LENGTH(column) = 3
WHERE CHAR_LENGTH(column) IS NULL
WHERE SUBSTRING(column, 1, 1) = 'a'
```

This slice is intentionally not a general expression predicate engine. It
extends the existing descriptor predicate planner so SQLite still evaluates the
filter inside the generated `WHERE` clause; MyLite does not materialize table
rows and then filter them in C.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, string functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- Existing MyLite feature specifications:
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-substring-functions/specs.md`
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
  - `docs/specs/baseline-delete-lifecycle/specs.md`
  - `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL 8.4.9 runtime probes captured by:
  - `packages/libmylite/tests/mysql_baseline_string_length_functions_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_substring_functions_expectations.sh`

This specification is independently authored from project documentation,
official MySQL documentation, observed MySQL 8.4.9 behavior, public SQLite APIs,
and existing MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Observations

Runtime probes establish the behavior used by this slice:

- `LENGTH(v) = 1` matches byte-length values; under `utf8mb4`,
  `LENGTH('é') = 2`.
- `CHAR_LENGTH(v) = 1` counts characters, so a one-character multibyte string
  matches.
- bare `LENGTH(nullable_column)` follows normal truth conversion: `NULL` is
  not true, `0` is false, and nonzero integer lengths are true.
- `LENGTH(nullable_column) IS NULL` matches rows where the function input is
  `NULL`.
- `SUBSTRING(v, 1, 1) = 'a'` uses the connection/table default collation for
  nonbinary strings; in the current ASCII subset, `'A'` and `'a'` compare equal.
- `SUBSTRING(nullable_column, 1, 1) IS NULL` and `<=> NULL` match rows where
  the source string is `NULL`.
- Supported successful predicate statements produce `@@warning_count = 0`.

## Supported Surface

The feature applies to existing single descriptor source `WHERE` users:

- filtered single-table `SELECT`, including the existing optional source alias
  policy;
- filtered single-table aggregate `SELECT` source filters without extending
  aggregate arguments, grouping expressions, or `HAVING`;
- filtered single-table `DELETE`;
- filtered single-table `UPDATE`.

This slice does not extend joined, grouped-output, metadata-table, or
information-schema predicate surfaces.

Length predicates:

```sql
string_length_predicate:
    string_length_expr
  | string_length_expr comparison_operator integer_or_boolean_or_NULL
  | string_length_expr IS NULL
  | string_length_expr IS NOT NULL

string_length_expr:
    LENGTH(expression)
  | OCTET_LENGTH(expression)
  | BIT_LENGTH(expression)
  | CHAR_LENGTH(expression)
  | CHARACTER_LENGTH(expression)
```

Substring predicates:

```sql
substring_predicate:
    substring_expr comparison_operator string_literal_or_NULL
  | substring_expr IS NULL
  | substring_expr IS NOT NULL

substring_expr:
    SUBSTRING(expression, position)
  | SUBSTRING(expression, position, length)
  | SUBSTRING(expression FROM position)
  | SUBSTRING(expression FROM position FOR length)
  | SUBSTR(...)
  | MID(...)
```

`expression`, `position`, and `length` are limited by the already implemented
row-scalar string length and substring planners. Descriptor column references
are resolved from MyLite descriptors and use the current source alias policy.
Unknown predicate columns use `WHERE` diagnostics.

Supported comparison operators are the existing predicate comparison operators:
`=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`. Length comparison values are
limited to signed decimal integer literals, optional unary sign, `TRUE`,
`FALSE`, and `NULL`. Substring comparison values are limited to ASCII string
literals and `NULL`.

## Deferred Surface

This slice does not support:

- `LEFT()`, `RIGHT()`, `SUBSTRING_INDEX()`, `LOWER()`, `UPPER()`, `CONCAT()`,
  `IF()`, `GREATEST()`, `LEAST()`, or other function predicates;
- substring bare truth predicates;
- `LIKE`, `REGEXP`, `IN`, `BETWEEN`, or row-constructor predicates over these
  function results;
- expression `ORDER BY`, `GROUP BY`, `HAVING`, aggregate arguments, generated
  columns, defaults, DML assignment values, or index expressions;
- table-qualified function arguments beyond the existing descriptor alias
  policy;
- binary substring predicates, non-ASCII string predicate literals, full Unicode
  collation parity, parameters, user variables, subqueries inside the functions,
  nested row functions, or arbitrary expression predicates.

## MyLite Lemon-Syntax Snippets

The grammar is a MyLite-authored subset:

```lemon
expression(A) ::= string_length_expression(B). { A = B; }

string_length_expression(A) ::= LENGTH(T) LPAREN expression(B) RPAREN(R).
string_length_expression(A) ::= OCTET_LENGTH(T) LPAREN expression(B) RPAREN(R).
string_length_expression(A) ::= BIT_LENGTH(T) LPAREN expression(B) RPAREN(R).
string_length_expression(A) ::= CHAR_LENGTH(T) LPAREN expression(B) RPAREN(R).
string_length_expression(A) ::= CHARACTER_LENGTH(T) LPAREN expression(B) RPAREN(R).

expression(A) ::= substring_expression(B). { A = B; }

substring_expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) COMMA expression(C) RPAREN(R).
substring_expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R).
substring_expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) FROM expression(C) RPAREN(R).
substring_expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) FROM expression(C) FOR expression(D) RPAREN(R).
substring_expression(A) ::= SUBSTR(...).
substring_expression(A) ::= MID(...).

predicate_atom(A) ::= string_length_expression(C).
predicate_atom(A) ::= string_length_expression(C) predicate_comparison_operator(O)
    predicate_comparison_value(V).
predicate_atom(A) ::= string_length_expression(C) IS(I) NULL(N).
predicate_atom(A) ::= string_length_expression(C) IS(I) NOT NULL(N).

predicate_atom(A) ::= substring_expression(C) predicate_comparison_operator(O)
    predicate_comparison_value(V).
predicate_atom(A) ::= substring_expression(C) IS(I) NULL(N).
predicate_atom(A) ::= substring_expression(C) IS(I) NOT NULL(N).
```

These snippets describe MyLite's supported grammar, not MySQL's full grammar.

## Runtime Semantics

The predicate planner:

1. Recognizes admitted function predicate AST nodes before descriptor-column
   predicate fallback.
2. Rejects joined or DML source shapes that the existing predicate planner does
   not support for row-scalar function expressions.
3. Uses the existing row-scalar string length or substring planner with
   `WHERE` column diagnostics.
4. Converts length comparison values to planned integer or `NULL` values using
   the existing signed-64 literal policy.
5. Converts substring comparison values to planned ASCII text or `NULL` values
   using existing predicate string literal decoding.
6. Emits generated SQLite `WHERE` SQL using the existing row-scalar expression
   SQL generator and bound parameters.

String-valued substring comparisons append MyLite's registered limited Unicode
`utf8mb4_0900_ai_ci` collation to the generated left-hand expression before the
comparison operator. This preserves the current MySQL-compatible ASCII
case-insensitive behavior for supported string predicate literals. Length
function predicates are numeric and do not use string collation.

`NULL` behavior follows SQL predicate semantics through SQLite expressions:
ordinary comparisons with `NULL` do not match, `<=> NULL` is lowered through the
existing null-safe comparison operator, and `IS NULL` / `IS NOT NULL` test the
function result.

## Ownership Boundaries

- Public API: unchanged. Callers continue to use `mylite_execute()` and the
  existing result API.
- Statement context: owns diagnostics and warning counts. Supported predicates
  add no warnings.
- Parser/AST: admits only the new predicate forms while preserving existing
  projection AST nodes.
- Analyzer/planner: resolves descriptor columns and supported function
  arguments before any SQLite SQL is generated.
- Catalog: read-only. No descriptor rows, catalog generation counters, or
  SQLite schema generation values are mutated.
- SQLite: receives generated SQL over stable physical table/column names and
  bound comparison values. No SQLite fork patch is required.
- Storage/VFS/file format: unchanged; the `.mylite` preamble and shifted
  SQLite payload invariants are preserved.

## Diagnostics

Diagnostics use existing MyLite/MySQL-compatible paths:

- syntax outside the admitted grammar: existing parse diagnostic;
- unknown descriptor columns in function arguments: `1054 / 42S22` with
  `WHERE` context;
- unsupported function argument descriptor families: existing string length or
  substring unsupported diagnostics;
- length comparison values outside signed-64 or unsupported literal kinds:
  deterministic unsupported predicate diagnostics;
- substring comparison values outside ASCII string/`NULL`: deterministic
  unsupported predicate diagnostics;
- allocation, physical SQLite, or public API misuse failures: existing runtime
  diagnostics.

Supported in-range predicate statements report `warning_count == 0`.

## Performance And Storage

Generated predicates stay in SQLite `WHERE` SQL using descriptor-built
expressions and bound values. MyLite does not materialize candidate rows in
memory. These function predicates are generally not sargable with the current
descriptor index set and may scan the selected source, matching MySQL's visible
behavior but without future optimizer work. No file-format change or SQLite fork
patch is needed.

## Tests

Tests cover:

- parser acceptance for length truth/comparison/`IS NULL` predicates and
  substring comparison/`IS NULL` predicates;
- successful length predicates for byte length, character length, truth,
  `IS NULL`, and `IS NOT NULL`;
- successful substring predicates for equality, inequality, null-safe `NULL`,
  `IS NULL`, `IS NOT NULL`, comma and `FROM`/`FOR` forms, and ASCII
  case-insensitive comparison;
- reuse from filtered `SELECT`, `DELETE`, and `UPDATE` through the shared
  descriptor predicate planner;
- ungrouped aggregate source-filter reuse for the admitted length predicate
  subset;
- deterministic diagnostics for unknown function argument columns, unsupported
  approximate columns, unsupported comparison literals, unsupported substring
  position/length expressions, and still-deferred expression ordering and DML
  assignment values;
- MySQL 8.4.9 expectation scripts for the user-visible behavior.
