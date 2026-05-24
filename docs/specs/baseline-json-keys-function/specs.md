# Baseline JSON_KEYS Function

## Summary

This phase adds a narrow JSON key-listing slice:

```sql
JSON_KEYS(json_doc)
JSON_KEYS(json_doc, path)
```

The goal is to extend the existing MyLite-owned JSON read/introspection layer
with the common object-key helper while preserving the current JSON parser,
simple-path envelope, row-scalar planning model, and SQLite scalar-function
integration. This phase does not add JSON mutation, wildcard paths,
`JSON_VALUE()`, `JSON_TABLE()`, JSON predicates, expression ordering, generated
columns, functional indexes, or a SQLite JSON1 dependency.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing JSON slices:
  - `docs/specs/baseline-json-type/specs.md`
  - `docs/specs/baseline-json-extract-functions/specs.md`
  - `docs/specs/baseline-json-introspection-functions/specs.md`
  - `docs/specs/baseline-json-contains-functions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - JSON function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html>
  - JSON search functions, including `JSON_KEYS()`:
    <https://dev.mysql.com/doc/refman/8.4/en/json-search-functions.html>
  - JSON data type and path syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/json.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_json_keys_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- `JSON_KEYS(json_doc)` and `JSON_KEYS(json_doc, path)` are valid forms.
- Wrong arity fails with `1582 / 42000`.
- `JSON_KEYS(NULL)` and `JSON_KEYS(json_doc, NULL)` return SQL `NULL`.
- `JSON_KEYS(NULL, invalid_path_text)` returns SQL `NULL` without validating
  the path.
- A top-level object returns a JSON array of top-level object member names.
  Nested object keys are not included unless selected by the `path` argument.
- Key display order follows MySQL's normalized JSON object display order, not
  source text order. For example, `{"z":1,"a":2,"m":3}` returns
  `["a", "m", "z"]`.
- Duplicate object names collapse to one key after MySQL JSON normalization.
- An empty selected object returns `[]`.
- A non-object document, an unmatched path, or a path selecting a non-object
  returns SQL `NULL`.
- Invalid document text fails with `3141 / 22032`.
- Binary string document arguments fail with `3144 / 22032`.
- Non-JSON SQL scalar document arguments fail with `3146 / 22032`.
- Invalid path syntax fails with `3143 / 42000`.
- MySQL rejects wildcard/range paths for `JSON_KEYS()` with `3149 / 42000`.
  MyLite keeps wildcard/range/recursive paths outside this baseline and reports
  a deterministic unsupported-path diagnostic before generated SQLite SQL is
  executed.
- Successful supported invocations emit no warnings. Scalar `SELECT` follows
  existing result-set conventions; `DO` reports row count `0`.

The official JSON search-function documentation defines `JSON_KEYS()` as
returning the keys from the top-level value of a JSON object, or from the value
selected by an optional path, and returning `NULL` for `NULL` arguments,
non-object inputs, or missing/non-object path matches.

## Supported Scope

Supported:

- no-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projections over persistent and temporary
  base tables already supported by the row-scalar expression framework;
- `JSON_KEYS(json_doc)` with one document argument;
- `JSON_KEYS(json_doc, path)` with one document argument and one path argument;
- JSON document operands from:
  - SQL string literals;
  - SQL `NULL`;
  - JSON descriptor columns;
  - nonbinary string descriptor columns containing JSON text;
  - supported `JSON_EXTRACT()` results in scalar expression paths;
- path operands from:
  - SQL string literals in the current simple JSON path subset;
  - SQL `NULL`;
- simple JSON paths already specified for `JSON_EXTRACT()`:
  - `$`;
  - `.identifier` member legs where the identifier is ASCII
    `[A-Za-z_$][A-Za-z0-9_$]*`;
  - `."member"` quoted member legs with JSON-string-compatible ASCII content;
  - `[N]` array legs where `N` is a nonnegative decimal integer with no leading
    zero unless it is exactly `0`;
- result labels, aliases, result ownership, affected-row state, and warning
  count through existing public result conventions.

Deferred:

- wildcard paths, recursive descent, array ranges, `last`, path filters, or any
  path shape that can match more than one value;
- arbitrary expression arguments outside the existing scalar and row-scalar
  envelopes;
- `JSON_KEYS()` over binary strings, numeric SQL values, booleans, temporal
  values, parameters, user variables, subqueries, aggregate results, or
  arbitrary function results outside the explicitly supported `JSON_EXTRACT()`
  scalar nesting;
- use in `WHERE`, expression `ORDER BY`, grouping, distinct, aggregate
  arguments, `HAVING`, DML assignment expressions, defaults, generated columns,
  functional indexes, JSON path indexes, and JSON comparison semantics;
- JSON mutation functions, `JSON_TABLE()`, `JSON_VALUE()`, `CAST(... AS JSON)`,
  and `MEMBER OF()`;
- full Unicode path member names, Unicode escape normalization beyond current
  JSON string handling, non-ASCII collation behavior, and full protocol-grade
  expression metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse behavior, result
  lifetime, and cleanup. Results are exposed as text or SQL `NULL` through the
  existing public result object.
- Statement context: owns diagnostics, warning count, affected-row state,
  row-count state, and result finalization. Supported successful invocations
  report `warning_count == 0`.
- Lexer/parser/AST: admits `JSON_KEYS()` syntax, preserves source spans for
  labels and diagnostics, and records wrong-arity nodes where MySQL reports
  native function parameter-count errors. Parser code does not inspect table
  descriptors or parse JSON documents.
- Analyzer/planner: admits the function only in the current scalar and
  row-scalar expression envelopes, resolves descriptor columns through MyLite
  descriptors, rejects unsupported argument and path shapes before generated
  SQLite SQL is executed, and keeps result metadata within existing expression
  metadata limits.
- JSON runtime module: owns MyLite-authored JSON parsing, simple path
  evaluation, normalized object-key ordering, and JSON array serialization. It
  must not depend on SQLite JSON1 or copied implementation code.
- SQLite physical execution: row-scalar projection is lowered to an internal
  MyLite SQLite scalar function registered through public SQLite APIs. SQLite
  scans and projects rows; MyLite does not materialize table rows in C for this
  feature.
- Catalog/storage/VFS: unchanged. No successful `JSON_KEYS()` statement mutates
  catalog rows, descriptor versions, descriptor caches, catalog generation,
  `sqlite_schema_generation`, `.mylite` preamble bytes, or shifted SQLite
  payload invariants.

## Grammar

Supported SQL expression shapes:

```sql
JSON_KEYS(json_doc)
JSON_KEYS(json_doc, path)
```

MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= JSON_KEYS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_KEYS_FUNCTION, B, R);
}

expression(A) ::= JSON_KEYS(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_KEYS_FUNCTION, B, C, R);
}

expression(A) ::= JSON_KEYS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= JSON_KEYS(T) LPAREN expression(B) COMMA expression(C)
                  COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR, D, R);
}

identifier(A) ::= JSON_KEYS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Semantics

For supported `JSON_KEYS(json_doc[, path])`:

- SQL `NULL` document returns SQL `NULL`.
- SQL `NULL` path returns SQL `NULL` after a non-`NULL` document argument
  passes document type and JSON text validation.
- If the document is an object and no path is supplied, return a JSON array of
  the object's normalized top-level keys.
- If a path is supplied and matches an object, return a JSON array of the
  matched object's normalized top-level keys.
- If the selected object has no members, return `[]`.
- If the document is valid JSON but the selected value is not an object, return
  SQL `NULL`.
- If the path matches no value, return SQL `NULL`.
- Duplicate object names are already collapsed by MyLite's JSON normalization
  parser in the same way as existing JSON storage and `JSON_OBJECT()` output.
- The result array uses existing JSON string emission, including JSON string
  escaping for object names.

## Diagnostics

Supported diagnostics:

- wrong arity: `1582 / 42000`, native function parameter-count diagnostic for
  `JSON_KEYS`;
- invalid JSON document text: `3141 / 22032`;
- binary document argument: `3144 / 22032`;
- non-JSON SQL scalar document argument: `3146 / 22032`;
- invalid JSON path syntax: `3143 / 42000`;
- unsupported path shape, including wildcards/ranges/recursive descent:
  deterministic MyLite unsupported-path diagnostic;
- unsupported argument expression or descriptor type: deterministic MyLite
  unsupported-expression diagnostic;
- missing row-scalar column: existing descriptor-driven unknown-column
  diagnostic;
- allocation failure: `MYLITE_NOMEM` through the current public API behavior;
- public API misuse: unchanged.

Supported successful invocations produce `warning_count == 0`.

## Physical SQLite Handling

MyLite lowers row-scalar `JSON_KEYS()` projections to:

```sql
_mylite_json_keys(document_sql)
_mylite_json_keys(document_sql, path_sql)
```

The generated SQL is built from planned descriptor expressions. Document column
references use quoted physical identifiers and stable physical table aliases.
Literal JSON documents and paths are bound as parameters. The internal SQLite
function parses only its arguments and returns either SQL `NULL` or JSON text.
No SQLite JSON1 dependency or SQLite fork hook is required.

## Tests

Add fast plain C coverage in the existing JSON introspection runtime test,
`packages/libmylite/tests/runtime_json_introspection_functions_test.c`, and
MySQL expectation coverage in
`packages/libmylite/tests/mysql_baseline_json_keys_function_expectations.sh`.

Required coverage:

- literal object, empty object, nested object, duplicate key, non-object,
  unmatched path, selected non-object, SQL `NULL` document, SQL `NULL` path,
  and `NULL` document with invalid path text;
- normalized key order and nested object path selection;
- row-scalar projection from `JSON` and nonbinary string descriptor columns,
  including reopen persistence;
- supported `JSON_EXTRACT()` nesting;
- `SELECT ... FROM DUAL`, aliases/labels, and `DO` status;
- wrong arity, invalid JSON, invalid path, binary document, non-JSON scalar
  document, unsupported wildcard path, unsupported argument expression, and
  unknown row-scalar column diagnostics;
- no result rows for `DO`, `ROW_COUNT() == 0`, and `warning_count == 0` for
  successful supported statements;
- existing JSON valid/extract/introspection/contains tests continue to pass.

## Compatibility Updates

- `COMPATIBILITY.md`: move `JSON_KEYS()` from not implemented to limited
  supported.
- `docs/compatibility/functions-json.md`: document the exact
  no-source/`DUAL`/`DO` and row-scalar subset.
- `docs/compatibility/sql-query-expressions.md`: update only if row-scalar
  JSON expression wording needs the new function named explicitly.
