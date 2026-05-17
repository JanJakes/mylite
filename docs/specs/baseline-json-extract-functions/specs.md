# Baseline JSON Extraction Functions

## Summary

This phase adds a narrow JSON extraction and unquoting slice:

```sql
JSON_EXTRACT(json_doc, path)
JSON_UNQUOTE(value)
json_column -> path
json_column ->> path
```

The goal is to make common JSON read paths usable in scalar `SELECT`,
`SELECT ... FROM DUAL`, `DO`, and single-table row-scalar `SELECT`
projections while preserving the current descriptor-owned JSON storage model.
It deliberately does not add JSON mutation, JSON construction, `JSON_VALUE()`,
wildcard/range path semantics, generated JSON-path indexes, JSON comparison
predicates, expression ordering, DML assignment expressions, or a SQLite JSON1
dependency.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing JSON and scalar expression slices:
  - `docs/specs/baseline-json-type/specs.md`
  - `docs/specs/baseline-json-valid-function/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - JSON function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html>
  - JSON search functions:
    <https://dev.mysql.com/doc/refman/8.4/en/json-search-functions.html>
  - JSON modification functions, including `JSON_UNQUOTE()`:
    <https://dev.mysql.com/doc/refman/8.4/en/json-modification-functions.html>
  - JSON data type and path syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/json.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_json_extract_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- `JSON_EXTRACT(json_doc, path)` requires at least two arguments; zero- and
  one-argument forms fail with `1582 / 42000`.
- `JSON_EXTRACT()` returns SQL `NULL` when any argument is SQL `NULL` or when
  the path matches no value.
- A matched JSON string is returned as JSON text including double quotes; a
  matched JSON `null` is returned as the text `null`, not SQL `NULL`.
- A matched object or array is returned as normalized JSON text.
- With one simple path that matches one value, the function returns that value.
  With multiple paths, wildcards, or range paths, MySQL may autowrap multiple
  matched values as a JSON array; those multi-match forms are deferred here.
- Invalid JSON document text fails with `3141 / 22032`.
- Non-JSON scalar argument types such as SQL integers and booleans fail as
  invalid JSON data type with `3146 / 22032`.
- Binary string arguments fail with `3144 / 22032`.
- Invalid path expressions fail with `3143 / 42000`.
- `JSON_UNQUOTE(value)` requires exactly one argument; wrong arity fails with
  `1582 / 42000`.
- `JSON_UNQUOTE(NULL)` returns SQL `NULL`.
- If the argument is a valid JSON string literal, `JSON_UNQUOTE()` returns the
  decoded string. If the argument is valid JSON text for a non-string value
  such as `123`, `true`, `null`, an array, or an object, it returns the visible
  JSON text unchanged.
- If the argument does not begin and end with double quotes, MySQL returns the
  argument text unchanged.
- If the argument begins and ends with double quotes but is not a valid JSON
  string literal, `JSON_UNQUOTE()` fails with `3141 / 22032`.
- SQL numeric and boolean arguments to `JSON_UNQUOTE()` fail with
  `3064 / HY000`; binary string arguments fail with `3144 / 22032`.
- `column -> path` is a shorthand for `JSON_EXTRACT(column, path)`.
- `column ->> path` is equivalent to
  `JSON_UNQUOTE(JSON_EXTRACT(column, path))`.
- MySQL accepts `->` and `->>` with unqualified and qualified column
  identifiers on the left. Literal or function-call left operands are syntax
  errors in the observed subset.
- Path expressions always start at `$`. Simple member legs use `.name`, quoted
  member legs use `."name"`, and array legs use `[nonnegative_integer]`.
  MySQL also supports wildcards, recursive descent, array ranges, and `last`,
  but those are outside this baseline.
- Successful supported invocations emit no warnings. Scalar `SELECT` follows
  existing result-set conventions; `DO` reports row count `0`.

## Supported Scope

Supported:

- no-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projections over persistent and temporary
  base tables already supported by the row-scalar expression framework;
- `JSON_EXTRACT(json_doc, path)` with exactly one path argument;
- `JSON_UNQUOTE(value)` with exactly one argument;
- `descriptor_column -> path` and `descriptor_column ->> path`;
- JSON document operands from:
  - SQL string literals;
  - SQL `NULL`;
  - JSON descriptor columns;
  - nonbinary string descriptor columns containing JSON text;
- `JSON_UNQUOTE()` operands from:
  - SQL string literals;
  - SQL `NULL`;
  - JSON descriptor columns;
  - nonbinary string descriptor columns;
  - supported `JSON_EXTRACT()` calls;
- simple JSON paths:
  - `$`;
  - `.identifier` member legs where the identifier is ASCII
    `[A-Za-z_$][A-Za-z0-9_$]*`;
  - `."member"` quoted member legs with JSON-string-compatible ASCII content;
  - `[N]` array legs where `N` is a nonnegative decimal integer with no leading
    zero unless it is exactly `0`;
- nested combinations of those simple path legs, such as `$.a[0].b`;
- result labels, aliases, result ownership, affected-row state, and warning
  count through existing public result conventions.

Deferred:

- arbitrary expression arguments outside the existing scalar and row-scalar
  envelopes;
- `JSON_EXTRACT()` with multiple path arguments;
- wildcard paths, recursive descent, array ranges, `last`, path filters, and
  any path shape that can match more than one value;
- `JSON_UNQUOTE()` over binary strings, numeric SQL values, booleans, temporal
  values, parameters, user variables, subqueries, or arbitrary function
  results outside the explicitly supported `JSON_EXTRACT()` nesting;
- `WHERE JSON_EXTRACT(...) ...`, expression `ORDER BY`, grouping, distinct,
  aggregates, `HAVING`, DML assignment expressions, defaults, generated
  columns, functional indexes, JSON path indexes, and JSON comparison
  semantics;
- JSON mutation and construction functions, `JSON_VALUE()`, `JSON_TABLE()`,
  `CAST(... AS JSON)`, and `MEMBER OF()`;
- full Unicode path member names, Unicode escape normalization beyond current
  JSON string handling, non-ASCII collation behavior, and full protocol-grade
  expression metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse behavior, result
  lifetime, and cleanup. Results are exposed as text or SQL `NULL` through the
  existing public result object.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported successful invocations report
  `warning_count == 0`; `DO` preserves the existing non-row statement
  conventions.
- Lexer/parser/AST: admits `JSON_EXTRACT()`, `JSON_UNQUOTE()`, `->`, and
  `->>` syntax, preserves source spans for labels and diagnostics, and records
  wrong-arity nodes where MySQL reports native function parameter-count errors.
  Parser code does not inspect table descriptors or parse JSON documents.
- Analyzer/planner: admits the functions only in the current scalar and
  row-scalar expression envelopes, resolves descriptor columns through MyLite
  descriptors, rejects unsupported argument and path shapes before generated
  SQLite SQL is executed, and keeps result metadata within existing expression
  metadata limits.
- JSON runtime module: owns MyLite-authored JSON document parsing, simple path
  parsing/evaluation, JSON string unquoting, and canonical JSON serialization
  of extracted values. It must not depend on SQLite JSON1 or copied
  implementation code.
- SQLite physical execution: row-scalar projection is lowered to internal
  MyLite SQLite scalar functions registered through public SQLite APIs. SQLite
  scans and projects rows; MyLite does not materialize table rows in C for this
  feature.
- Catalog/storage/VFS: unchanged. No successful JSON extraction statement
  mutates catalog rows, descriptor versions, descriptor caches, catalog
  generation, `sqlite_schema_generation`, `.mylite` preamble bytes, or shifted
  SQLite payload invariants.

## Grammar

Supported SQL expression shapes:

```sql
JSON_EXTRACT(json_doc, json_path)
JSON_UNQUOTE(json_unquote_value)
descriptor_column -> json_path
descriptor_column ->> json_path
```

MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= JSON_EXTRACT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION, B, C, R);
}

expression(A) ::= JSON_UNQUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION, B, R);
}

expression(A) ::= column_reference(B) JSON_EXTRACT_OPERATOR(O) string_literal_path(C). {
    A = mylite_sql_parser_make_json_path_operator(
        state, B, O, MYLITE_SQL_AST_JSON_EXTRACT_OPERATOR, C);
}

expression(A) ::= column_reference(B) JSON_UNQUOTE_EXTRACT_OPERATOR(O) string_literal_path(C). {
    A = mylite_sql_parser_make_json_path_operator(
        state, B, O, MYLITE_SQL_AST_JSON_UNQUOTE_EXTRACT_OPERATOR, C);
}

expression(A) ::= JSON_EXTRACT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= JSON_EXTRACT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR, B, R);
}

expression(A) ::= JSON_EXTRACT(T) LPAREN expression(B) COMMA expression(C)
                  COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR, D, R);
}

expression(A) ::= JSON_UNQUOTE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= JSON_UNQUOTE(T) LPAREN expression(B) COMMA function_argument_list(C)
                  RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR, C, R);
}

identifier(A) ::= JSON_EXTRACT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

identifier(A) ::= JSON_UNQUOTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## JSON Path Semantics

The path parser evaluates against a parsed JSON document value:

- `$` selects the whole document.
- `.name` selects the named object member only when the current value is an
  object and the member exists.
- `."name"` does the same after decoding the quoted path member name.
- `[N]` selects the zero-based array element only when the current value is an
  array and `N` is in range.
- If any leg does not match because of type mismatch, missing object member, or
  out-of-range array index, the overall result is SQL `NULL`.
- A JSON `null` value reached by a path is a successful JSON value and renders
  as text `null`.
- Duplicate object keys follow the existing JSON type policy: the last admitted
  key value is authoritative in the parsed object model.
- Path strings that are invalid for MySQL's simple syntax fail with
  `3143 / 42000`.
- Valid MySQL paths outside this phase, such as wildcards and ranges, fail with
  a deterministic MyLite unsupported diagnostic until those shapes are
  implemented and tested.

## Runtime Semantics

`JSON_EXTRACT()` evaluation:

1. Evaluate the JSON document operand in the existing scalar or row-scalar
   envelope.
2. If either operand is SQL `NULL`, return SQL `NULL`.
3. Require a nonbinary text or JSON descriptor value for the document operand.
   Unsupported SQL numeric, boolean, temporal, binary, and arbitrary expression
   operands fail before SQLite execution where possible.
4. Parse the document with MyLite's JSON parser. Malformed JSON text maps to
   the MySQL-compatible invalid-JSON diagnostic for this function.
5. Parse the simple path subset.
6. Evaluate the path. Missing matches return SQL `NULL`; matched values are
   serialized to MySQL-style JSON display text using the current MyLite JSON
   canonicalization policy.

`JSON_UNQUOTE()` evaluation:

1. Evaluate the operand in the existing scalar or row-scalar envelope.
2. SQL `NULL` returns SQL `NULL`.
3. Require nonbinary text or JSON descriptor input, or a supported nested
   `JSON_EXTRACT()` result.
4. If the value begins and ends with double quotes, parse it as a JSON string
   literal and return the decoded text. Invalid JSON string text fails with
   `3141 / 22032`.
5. Otherwise return the input text unchanged.

The `->` operator behaves like `JSON_EXTRACT(column, path)`. The `->>` operator
behaves like `JSON_UNQUOTE(JSON_EXTRACT(column, path))`.

Supported successful calls never add warnings. JSON extraction statements are
read-only and must not affect catalog or SQLite schema generations.

## SQLite Handling

Generated row-scalar SQL uses internal MyLite scalar functions:

```sql
_mylite_json_extract(argument_sql, path_parameter)
_mylite_json_unquote(argument_sql)
_mylite_json_unquote(_mylite_json_extract(column_sql, path_parameter))
```

The `argument_sql` and `column_sql` subtrees are generated from existing
descriptor/planned expression code. Literal values and path values are bound as
parameters rather than interpolated. Descriptor column identifiers and physical
table names are quoted through existing SQL builders. SQLite receives text or
NULL arguments, scans tables, and calls MyLite-owned callbacks.

The implementation uses only public SQLite function-registration APIs. No
SQLite JSON1 dependency and no SQLite fork patch are required for this phase.

## Diagnostics

- Wrong `JSON_EXTRACT()` argument count: `1582 / 42000` with native function
  name `JSON_EXTRACT`.
- Wrong `JSON_UNQUOTE()` argument count: `1582 / 42000` with native function
  name `JSON_UNQUOTE`.
- Invalid JSON document text for `JSON_EXTRACT()`: `3141 / 22032`.
- Invalid JSON string literal for `JSON_UNQUOTE()`: `3141 / 22032`.
- Non-JSON scalar document argument to `JSON_EXTRACT()`: `3146 / 22032` when
  MyLite can classify the argument as a supported SQL scalar type.
- Binary string input where MySQL rejects it: `3144 / 22032` when the current
  expression layer can classify it.
- SQL numeric or boolean argument to `JSON_UNQUOTE()`: `3064 / HY000` when the
  current expression layer can classify it.
- Invalid simple path syntax: `3143 / 42000`.
- Valid but deferred path syntax: deterministic MyLite unsupported diagnostic.
- Unknown row-scalar columns, ambiguous columns, unsupported row sources,
  unsupported nested expressions, unsupported statement contexts, allocation
  failures, SQLite callback failures, and public API misuse reuse existing
  MyLite diagnostic policy.

## Tests

MySQL expectation script coverage:

- `JSON_EXTRACT()` over literal JSON strings for object members, array
  elements, root, nested paths, missing paths, JSON `null`, SQL `NULL`, and
  warning count;
- `JSON_UNQUOTE()` over JSON string text, non-string JSON text, SQL `NULL`,
  invalid quoted JSON string text, and wrong argument counts;
- table-backed JSON and `VARCHAR` columns with `JSON_EXTRACT()`,
  `JSON_UNQUOTE(JSON_EXTRACT())`, `->`, and `->>`;
- result labels and explicit aliases;
- `DO` row-count and warning-count behavior;
- MySQL-compatible diagnostics for wrong argument counts, invalid JSON text,
  invalid simple paths, non-JSON scalar document operands, and unsupported
  scalar input types where this phase claims exact diagnostics.

Fast MyLite C tests must cover:

- parser acceptance for admitted functions/operators, qualified columns,
  aliases, and whitespace;
- parser diagnostics for wrong arity and operator left-operand restrictions;
- scalar, `DUAL`, `DO`, and row-scalar runtime values;
- JSON and nonbinary string descriptor columns;
- SQL `NULL` versus JSON `null`;
- simple path legs and nested simple paths;
- unsupported path shapes such as wildcards, ranges, `last`, negative indexes,
  leading-zero indexes, expression paths, multiple paths, literal/operator
  left operands, binary inputs, numeric `JSON_UNQUOTE()` inputs, and unsupported
  statement contexts;
- result labels, aliases, absence of warnings, absence of affected rows for
  scalar `SELECT`, `DO` row count, generation counters, reopen persistence, and
  independent file-backed handle isolation where row data is involved;
- zero-initialized cleanup for any new AST, planner, or runtime helper state.

Existing lexer, parser, runtime handle, diagnostics, statement context, result
metadata, SQLite bootstrap/registration, file-backed opening, VFS, JSON type,
JSON_VALID, row-scalar expression, and broader runtime lifecycle tests must
continue to pass.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/functions-json.md` should move only
the exact implemented subset for `JSON_EXTRACT()`, `JSON_UNQUOTE()`, `->`, and
`->>` from unsupported to partial support. `JSON path literals` should become
partial only for the simple path subset documented here. The JSON type row
should mention limited extraction while continuing to deny mutation,
construction, full path, comparison, ordering, generated-column, and indexing
semantics.

No docs should claim full JSON path grammar, full expression evaluation,
wildcards, ranges, `JSON_VALUE()`, `JSON_TABLE()`, JSON mutation functions,
JSON construction functions, generated columns, path indexes, binary JSON
storage, or SQLite JSON1 compatibility.

## Verification

Before implementation is marked done:

1. `cmake --build --preset dev`
2. Run the new JSON extraction CTest entries plus existing parser,
   `runtime_json_type`, `runtime_json_valid_function`, and row-scalar
   expression entries.
3. Run
   `packages/libmylite/tests/mysql_baseline_json_extract_functions_expectations.sh`.
4. `cmake --workflow --preset check`
5. Review the final diff for architecture boundaries, public ABI stability,
   independent spec text, MySQL 8.4.9 evidence, descriptor-driven row access,
   path parsing correctness, SQL NULL versus JSON null behavior, result labels,
   warning behavior, file-format safety, zero-init cleanup, scope control, and
   compatibility documentation accuracy.
