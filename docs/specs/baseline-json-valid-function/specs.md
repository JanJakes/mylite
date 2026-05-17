# Baseline JSON_VALID Function

## Summary

This phase adds a narrow `JSON_VALID(value)` scalar function over the current
MyLite scalar and row-scalar expression envelopes. It is intended to support
common application checks over JSON text columns before the broader JSON path
and construction functions are implemented.

The function reports JSON syntax validity. It does not canonicalize, mutate, or
extract JSON values. In particular, this phase must not reuse the existing JSON
storage normalizer as the semantic authority because that normalizer rejects
some valid JSON documents that MyLite cannot yet store canonically. For
`JSON_VALID()`, valid-but-not-storable JSON text still returns `1`.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing JSON type support:
  - `docs/specs/baseline-json-type/specs.md`
  - `packages/libmylite/src/runtime/mylite_json.c`
  - `packages/libmylite/tests/runtime_json_type_test.c`
- Existing scalar and row-scalar function slices:
  - `docs/specs/baseline-field-function/specs.md`
  - `docs/specs/baseline-concat-ws-function/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - JSON function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html>
  - JSON attribute functions:
    <https://dev.mysql.com/doc/refman/8.4/en/json-attribute-functions.html>
  - JSON data type:
    <https://dev.mysql.com/doc/refman/8.4/en/json.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_json_valid_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Observations

Observed behavior shaping this slice:

- `JSON_VALID(value)` returns `1` for valid JSON string values, `0` for invalid
  JSON string values, and SQL `NULL` for SQL `NULL`.
- SQL numeric and boolean scalar values such as `JSON_VALID(1)` and
  `JSON_VALID(TRUE)` return `0`, while string values containing JSON numbers
  such as `JSON_VALID('1.2')` and `JSON_VALID('1e2')` return `1`.
- JSON column values return `1` for non-`NULL` rows because MySQL stores only
  valid JSON documents in JSON columns.
- `VARCHAR` column values are checked as JSON text per row. `WHERE
  JSON_VALID(varchar_col)` treats `1` as true and excludes `0`/`NULL`.
- Binary string inputs return `0` in the verified subset, even when the bytes
  spell JSON text.
- Successful supported invocations emit no warnings.
- `SELECT JSON_VALID()` and `SELECT JSON_VALID('{}', '{}')` fail with
  `1582 / 42000`.

The official JSON function reference lists `JSON_VALID()` as the JSON validity
check, and the JSON attribute function documentation describes the `0`, `1`,
and `NULL` result shape.

## Supported Scope

Supported:

- `JSON_VALID(value)` in no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
  `DO`, and single-table row-scalar `SELECT` projections;
- `JSON_VALID(value)` inside the existing row-scalar predicate envelope where
  row-scalar truth predicates are already admitted;
- `value` as one existing scalar or row-scalar operand:
  - SQL string literal;
  - SQL integer literal with optional unary sign;
  - SQL `TRUE` / `FALSE`;
  - SQL `NULL`;
  - limited no-source and `FROM DUAL` binary casts/conversions over those same
    scalar literal values, returning `0` for non-`NULL` binary input;
  - descriptor columns already admitted by row-scalar planning, including
    JSON, nonbinary string, binary string, integer, boolean-like integer, and
    nullable columns;
- validation of ordinary JSON documents with objects, arrays, strings, numbers,
  `true`, `false`, and `null`;
- JSON number validity independent of MyLite's current JSON storage
  canonicalization limits, including decimal fractions, exponents, and
  magnitudes outside the current JSON storage signed-64 range;
- result labels, aliases, affected-row state, warning count, and result
  ownership through existing public API conventions.

Deferred:

- `JSON_VALID()` over arbitrary expressions outside the current scalar and
  row-scalar envelopes;
- nested JSON functions such as `JSON_VALID(JSON_OBJECT(...))`;
- `CAST(... AS JSON)`, JSON path operators, `JSON_TABLE`, parameters, prepared
  statement metadata, generated columns, and indexes;
- JSON charset conversion and exact invalid-UTF-8 diagnostics;
- embedded-NUL SQL string literals beyond existing MyLite string-literal
  decoder limits;
- full protocol-grade expression metadata.

## Ownership Boundary

- Public API: unchanged. `mylite_execute()` and result lifetime conventions
  remain the public surface.
- Statement context: owns warnings, diagnostics, affected rows, and result
  completion. Supported `JSON_VALID()` invocations report warning count `0`.
- Lexer/parser/AST: adds the nonreserved `JSON_VALID` function token and an AST
  function node. `JSON_VALID` remains usable as an identifier where the
  identifier grammar permits nonreserved function names.
- Analyzer/planner: admits the function only in the current scalar and
  row-scalar function envelopes, resolves descriptor columns through MyLite
  descriptors, and rejects unsupported argument shapes before SQLite SQL is
  generated.
- JSON runtime module: owns syntax-only JSON validation. It must be separate
  from storage canonicalization so `JSON_VALID('1.2')` can return `1` while
  the current JSON column storage slice may still reject some values.
- SQLite physical execution: row-scalar SQL calls a registered MyLite scalar
  function through public SQLite function-registration APIs. SQLite scans and
  projects rows; MyLite does not materialize table rows in C for this function.
- Catalog/storage/VFS: unchanged. No catalog descriptors, schema generations,
  `.mylite` preamble bytes, or SQLite fork patches are modified by successful
  `JSON_VALID()` evaluation.

## Grammar

Supported MyLite grammar:

```sql
expression:
    JSON_VALID '(' expression ')'

predicate:
    JSON_VALID '(' expression ')'
    JSON_VALID '(' expression ')' comparison_operator integer_value
    JSON_VALID '(' expression ')' IS [NOT] NULL
```

Argument-count diagnostics are represented explicitly:

```sql
JSON_VALID '(' ')'
JSON_VALID '(' expression ',' function_argument_list ')'
```

MyLite Lemon-syntax snippet:

```lemon
expression(A) ::= json_valid_expression(B). {
    A = B;
}

json_valid_expression(A) ::= JSON_VALID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_VALID_FUNCTION, B, R);
}

predicate_atom(A) ::= json_valid_expression(C). {
    A = C;
}

predicate_atom(A) ::= json_valid_expression(C) EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}

predicate_atom(A) ::= json_valid_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}

expression(A) ::= JSON_VALID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= JSON_VALID(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR, C, R);
}

identifier(A) ::= JSON_VALID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Semantics

For one supported argument:

- SQL `NULL` returns SQL `NULL`.
- Nonbinary text values are interpreted as JSON document text. Syntactically
  valid JSON returns `1`; syntactically invalid JSON returns `0`.
- JSON descriptor columns are stored as canonical JSON text by the existing
  JSON type slice, so non-`NULL` JSON column values return `1`.
- Numeric, boolean, and binary descriptor values return `0` when non-`NULL`.

The JSON validity grammar admits:

- JSON objects and arrays with arbitrary nesting up to the current MyLite
  validation stack limit;
- JSON strings with standard JSON escapes and `\uXXXX` escapes;
- JSON numbers with optional fraction and exponent parts;
- `true`, `false`, and `null`;
- RFC-style whitespace around and between JSON tokens.

Invalid JSON text never raises the JSON column assignment diagnostic in this
function. It returns `0`.

## SQLite Handling

Generated row-scalar SQL shape:

```sql
_mylite_json_valid(argument_sql)
```

The `argument_sql` subtree is generated from existing descriptor/planned
expression code. Literal values are still bound as parameters rather than
interpolated. Descriptor column identifiers are quoted. The SQLite callback
returns:

- `NULL` for SQLite `NULL`;
- integer `1` or `0` for SQLite `TEXT`;
- integer `0` for SQLite numeric or blob values.

The callback uses only public SQLite APIs and MyLite-owned code. No SQLite JSON1
dependency or SQLite fork patch is introduced.

## Diagnostics

- Wrong argument count: MySQL-compatible `1582 / 42000` with native function
  name `JSON_VALID`.
- Unsupported scalar or row-scalar argument shape: deterministic MyLite
  unsupported diagnostic.
- Unsupported use in non-admitted statement classes: existing statement
  unsupported diagnostics.
- Allocation failure: `MYLITE_NOMEM` with existing diagnostics.
- SQLite callback misuse or registration failure: existing internal/SQLite
  failure handling.

Invalid JSON text is not a diagnostic for `JSON_VALID()`; it returns `0`.

## Tests

MySQL expectation script coverage:

- literal valid/invalid/`NULL` text;
- JSON numeric text including fraction, exponent, and large integer forms;
- SQL numeric and boolean scalar arguments returning `0`;
- JSON and `VARCHAR` table columns;
- binary string inputs returning `0`;
- `WHERE JSON_VALID(varchar_col)`;
- labels and aliases;
- `DO JSON_VALID(...)`;
- argument-count diagnostics.

Fast C test coverage:

- no-source, `FROM DUAL`, and `DO` execution;
- table-backed projection over JSON, string, integer, binary, and nullable
  columns;
- row-scalar `WHERE JSON_VALID(...)` and DML predicate reuse where currently
  admitted;
- invalid JSON returns `0` without warnings;
- unsupported nested or arbitrary expression diagnostics;
- wrong argument count;
- reopen persistence remains unaffected by JSON-valid projections;
- independent file-backed handles.

## Compatibility Docs

Update `COMPATIBILITY.md` and `docs/compatibility/functions-json.md` only for
this partial `JSON_VALID()` slice. Do not claim JSON path extraction, JSON
construction/mutation functions, `JSON_TABLE`, expression defaults, generated
columns, multi-valued indexes, or full JSON expression support.
