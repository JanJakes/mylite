# Baseline JSON Construction Functions

## Summary

This phase adds a narrow MySQL-compatible JSON construction slice:

```sql
JSON_ARRAY([value[, value] ...])
JSON_OBJECT([key, value[, key, value] ...])
```

The goal is to make common application JSON projection shapes usable in
no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projections while preserving MyLite-owned JSON semantics.
This phase does not add JSON mutation, aggregation, `JSON_QUOTE()`, arbitrary
expression evaluation, JSON predicates, expression ordering, DML assignment
expressions, generated columns, or a SQLite JSON1 dependency.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing JSON and scalar expression slices:
  - `docs/specs/baseline-json-type/specs.md`
  - `docs/specs/baseline-json-valid-function/specs.md`
  - `docs/specs/baseline-json-extract-functions/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - JSON creation functions:
    <https://dev.mysql.com/doc/refman/8.4/en/json-creation-functions.html>
  - JSON function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html>
  - JSON data type:
    <https://dev.mysql.com/doc/refman/8.4/en/json.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_json_construction_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- `JSON_ARRAY()` accepts zero or more arguments. The empty form returns `[]`.
- SQL `NULL` array values become JSON `null`, not SQL `NULL`.
- SQL string values become JSON strings. SQL string decoding follows the active
  SQL mode before JSON construction.
- SQL integer values become JSON numbers. SQL `TRUE` and `FALSE` literals
  become JSON `true` and `false`; boolean-like table columns are MySQL integer
  columns and construct as `1` or `0`.
- JSON column values construct as JSON values, not quoted strings. SQL string
  columns containing JSON-looking text still construct as JSON strings.
- Successful scalar `SELECT JSON_ARRAY(...)` and `JSON_OBJECT(...)` set
  `ROW_COUNT()` to `-1` and emit no warnings in the verified subset. `DO`
  reports row count `0` and warning count `0`.
- `JSON_OBJECT()` accepts zero or an even number of arguments. The empty form
  returns `{}`. An odd argument count fails with `1582 / 42000`.
- Object keys are converted to member names. SQL `NULL` keys fail with
  `3158 / 22032`. Integer and boolean literal keys are admitted by MySQL and
  converted through their visible scalar values.
- Duplicate object keys retain the last value for that key.
- Object output order follows MySQL's observed binary JSON object display order
  for the currently admitted ASCII key subset: shorter keys sort before longer
  keys, with bytewise comparison for equal-length keys.
- `JSON_ARRAY(*)` is a syntax error. Nested JSON construction functions and
  JSON extraction results are accepted by MySQL, but this baseline slice
  defers nested construction unless a later expression slice broadens the
  supported row-scalar expression tree.

The official JSON creation function page describes `JSON_ARRAY()` as returning
a JSON array from a possibly empty value list and `JSON_OBJECT()` as returning a
JSON object from key/value pairs, with errors for `NULL` keys and odd argument
counts.

## Supported Scope

Supported:

- no-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projections over persistent and temporary
  base tables already supported by the row-scalar expression framework;
- `JSON_ARRAY()` with zero or more supported arguments;
- `JSON_OBJECT()` with zero or more supported key/value pairs;
- array value operands from:
  - SQL string literals;
  - decimal integer literals with optional unary sign in MyLite's current
    signed-64 scalar envelope;
  - SQL `TRUE` / `FALSE`;
  - SQL `NULL`;
  - descriptor columns for JSON, integer, nonbinary string, and boolean-like
    integer values;
- object keys from:
  - SQL string literals;
  - decimal integer literals with optional unary sign in MyLite's current
    signed-64 scalar envelope;
  - SQL `TRUE` / `FALSE`;
  - descriptor columns for integer, nonbinary string, and boolean-like integer
    values when no row contains SQL `NULL`;
- object values from the same operand set as array values;
- JSON descriptor columns as raw JSON constructor values;
- SQL string descriptor columns as JSON strings, even when the text is valid
  JSON;
- duplicate object keys with last-key-wins construction before canonical object
  display sorting;
- result labels, aliases, result ownership, affected-row state, and warning
  count through existing public result conventions.

Deferred:

- nested `JSON_ARRAY()` / `JSON_OBJECT()` calls and arbitrary nested function
  operands outside existing supported scalar envelopes;
- arbitrary expression arguments, arithmetic, comparisons, subqueries,
  parameters, user variables, stored functions, aggregate/window functions,
  generated columns, expression defaults, predicates, `ORDER BY`, grouping,
  distinct, `HAVING`, and DML assignment expressions;
- JSON constructor values from `JSON_EXTRACT()` in table-backed row-scalar
  paths if they require a broader recursive expression emitter;
- binary string, `BIT`, approximate, `DECIMAL`, temporal, enum, set, and
  spatial constructor operands unless a later type slice specifies their exact
  MySQL conversion;
- `JSON_QUOTE()`, JSON mutation functions, JSON aggregate functions,
  `CAST(... AS JSON)`, `JSON_TABLE()`, and `MEMBER OF()`;
- full Unicode key ordering beyond the current ASCII-key JSON storage slice,
  invalid UTF-8 diagnostics, and protocol-grade JSON expression metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse behavior, result
  lifetime, and cleanup. Constructor results are exposed as text or SQL `NULL`
  through the existing result object.
- Statement context: owns diagnostics, warning count, affected rows, row-count
  state, and result finalization. Supported successful invocations report
  `warning_count == 0`.
- Lexer/parser/AST: admits `JSON_ARRAY` and `JSON_OBJECT` as nonreserved
  function tokens, preserves source spans for labels and diagnostics, and
  records the parsed argument list. Parser code does not inspect table
  descriptors or construct JSON text.
- Analyzer/planner: admits the functions only in the current scalar and
  row-scalar expression envelopes, resolves descriptor columns through MyLite
  descriptors, rejects unsupported operands before generated SQLite SQL is
  executed, and keeps result metadata within existing expression metadata
  limits.
- JSON runtime module: owns MyLite-authored JSON construction, string escaping,
  integer/boolean/null emission, duplicate-key handling, and object display
  ordering. It may reuse the existing JSON value writer and object member
  ordering but must not depend on SQLite JSON1.
- SQLite physical execution: row-scalar projection is lowered to internal
  MyLite SQLite scalar functions registered through public SQLite APIs. SQLite
  scans and projects rows; MyLite does not materialize full tables in C for
  this feature.
- Catalog/storage/VFS: unchanged. No successful JSON constructor statement
  mutates catalog rows, descriptor versions, descriptor caches, catalog
  generation, `sqlite_schema_generation`, `.mylite` preamble bytes, or shifted
  SQLite payload invariants.

## Grammar

Supported SQL expression shapes:

```sql
JSON_ARRAY()
JSON_ARRAY(expression [, expression] ...)
JSON_OBJECT()
JSON_OBJECT(expression, expression [, expression, expression] ...)
```

MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= JSON_ARRAY(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_FUNCTION, R);
}

expression(A) ::= JSON_ARRAY(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_FUNCTION, B, R);
}

expression(A) ::= JSON_OBJECT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_JSON_OBJECT_FUNCTION, R);
}

expression(A) ::= JSON_OBJECT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_OBJECT_FUNCTION, B, R);
}

identifier(A) ::= JSON_ARRAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

identifier(A) ::= JSON_OBJECT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Semantics

For supported `JSON_ARRAY()` operands:

- SQL `NULL` emits JSON `null`.
- SQL string values emit JSON strings with JSON escaping.
- SQL integer values emit JSON numbers.
- SQL `TRUE` / `FALSE` literals emit JSON `true` / `false`.
- JSON descriptor columns emit their canonical JSON text as JSON values.
- Nonbinary string descriptor columns emit JSON strings.

For supported `JSON_OBJECT()` operands:

- The argument count must be even. Odd counts report MySQL-compatible native
  function parameter-count diagnostics.
- Key operands are evaluated before construction. SQL `NULL` keys report
  `3158 / 22032`.
- Non-`NULL` string keys use the decoded string bytes as the member name.
- Non-`NULL` integer and boolean keys use their visible scalar decimal text as
  the member name.
- Values use the same conversion rules as `JSON_ARRAY()`.
- Duplicate keys replace the earlier member value. The final object is emitted
  in the current verified MySQL display order for ASCII keys: shorter keys
  before longer keys, bytewise order for equal-length keys.

Constructor output uses the same canonical display style as the JSON type
slice: comma-space separators, colon-space object separators, lowercase JSON
literals, and JSON string escaping for quotes, backslashes, and control bytes.

## SQLite Handling

Generated row-scalar SQL shapes:

```sql
_mylite_json_array(kind, argument_sql[, kind, argument_sql] ...)
_mylite_json_object(kind, key_sql, kind, value_sql[, kind, key_sql, kind, value_sql] ...)
```

Literal values are bound as parameters rather than interpolated. Descriptor
column identifiers are quoted. The `kind` operands are MyLite-internal integer
constants derived from descriptors or scalar literal kinds, not user SQL. The
internal SQLite callbacks are registered as variadic deterministic scalar
functions through public SQLite APIs. They inspect SQLite value types only for
the planned subset and delegate JSON text construction to MyLite-owned JSON
helpers.

No SQLite JSON1 function, SQLite optional JSON extension, or SQLite fork patch
is introduced.

## Diagnostics

- `JSON_OBJECT()` odd argument count: MySQL-compatible `1582 / 42000` with
  native function name `JSON_OBJECT`.
- `JSON_OBJECT()` SQL `NULL` key: MySQL-compatible `3158 / 22032` with a
  member-name message.
- Unsupported scalar or row-scalar argument shape: deterministic MyLite
  unsupported diagnostic naming the function and admitted operand set.
- Unsupported use in non-admitted statement classes: existing statement
  unsupported diagnostics.
- Unknown descriptor columns: existing MySQL-compatible unknown-column
  diagnostics.
- Unsupported descriptor type: deterministic MyLite unsupported diagnostic.
- Out-of-range integer literal: deterministic MyLite unsupported diagnostic
  matching the current scalar envelope policy.
- Allocation failure: `MYLITE_NOMEM` with existing diagnostics.
- SQLite callback misuse or registration failure: existing internal/SQLite
  failure handling.

## Tests

MySQL expectation script coverage:

- empty and populated `JSON_ARRAY()`;
- empty and populated `JSON_OBJECT()`;
- string escaping under ordinary mode and `NO_BACKSLASH_ESCAPES`;
- integer, signed integer, boolean, SQL `NULL`, and JSON column value
  conversion;
- SQL string values that look like JSON remaining JSON strings;
- duplicate object keys and display ordering;
- integer and boolean object keys;
- `JSON_OBJECT(NULL, value)` and odd argument-count diagnostics;
- `SELECT`, `SELECT ... FROM DUAL`, aliases, `DO`, `ROW_COUNT()`, and
  warning-count behavior;
- table-backed row-scalar projection and reopen persistence.

MyLite C tests cover:

- parser acceptance for empty and variadic constructor calls;
- parser rejection or deterministic diagnostics for unsupported forms;
- scalar `SELECT`, `FROM DUAL`, and `DO`;
- single-table row-scalar projection over integer, string, JSON, and nullable
  descriptor columns;
- object duplicate-key and `NULL` key behavior;
- unknown column and unsupported type diagnostics;
- reopen persistence to prove constructors do not disturb storage invariants;
- zero-initialized cleanup for newly added planned expression branches.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/functions-json.md` to mark
`JSON_ARRAY()` and `JSON_OBJECT()` as partially supported with the exact scalar
and row-scalar limits above. Do not mark JSON mutation, aggregation, path,
predicate, arbitrary expression, DML assignment, or metadata support as
complete.
