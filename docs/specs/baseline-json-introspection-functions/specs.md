# Baseline JSON Introspection Functions

## Summary

This phase adds a narrow JSON introspection slice:

```sql
JSON_TYPE(json_doc)
JSON_LENGTH(json_doc[, path])
```

The goal is to finish the current JSON read/construction cluster with common
attribute functions while preserving MyLite-owned JSON parsing and descriptor
semantics. This phase does not add JSON mutation, JSON aggregation,
`JSON_DEPTH()`, `JSON_KEYS()`, wildcard path behavior, arbitrary expression
evaluation, JSON predicates, expression ordering, generated columns, functional
indexes, or a SQLite JSON1 dependency.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing JSON and scalar expression slices:
  - `docs/specs/baseline-json-type/specs.md`
  - `docs/specs/baseline-json-valid-function/specs.md`
  - `docs/specs/baseline-json-extract-functions/specs.md`
  - `docs/specs/baseline-json-construction-functions/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - JSON function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html>
  - JSON attribute functions:
    <https://dev.mysql.com/doc/refman/8.4/en/json-attribute-functions.html>
  - JSON data type and path syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/json.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_json_introspection_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- `JSON_TYPE(json_doc)` requires exactly one argument. Wrong arity fails with
  `1582 / 42000`.
- `JSON_TYPE(NULL)` returns SQL `NULL`.
- `JSON_TYPE()` returns uppercase type labels for valid JSON values. The
  verified values are `OBJECT`, `ARRAY`, `BOOLEAN`, `NULL`, `STRING`,
  `INTEGER`, and `DOUBLE`.
- `JSON_TYPE(1)` fails with `3146 / 22032`; SQL numeric values are not accepted
  as JSON document text.
- Invalid JSON document text fails with `3141 / 22032`.
- Binary string arguments fail with `3144 / 22032`.
- `JSON_LENGTH(json_doc)` accepts one or two arguments. Wrong arity fails with
  `1582 / 42000`.
- `JSON_LENGTH(NULL, path)` returns SQL `NULL` without validating `path`.
  `JSON_LENGTH(json_doc, NULL)` returns SQL `NULL` only after a non-`NULL`
  document argument passes document type and JSON text validation.
- Scalars have length `1`; arrays report their element count; objects report
  their member count; nested children are not recursively counted.
- `JSON_LENGTH(json_doc, path)` reports the length of the matched path value.
  If the path matches no value, it returns SQL `NULL`.
- Invalid document text and non-JSON SQL scalar document arguments use the same
  diagnostics as `JSON_TYPE()`. Invalid path syntax fails with
  `3143 / 42000`.
- MySQL accepts wildcard paths for `JSON_LENGTH()`, but this baseline keeps the
  same simple-path envelope as `JSON_EXTRACT()` and defers wildcard/multi-match
  behavior.
- Successful supported invocations emit no warnings. Scalar `SELECT` follows
  existing result-set conventions; `DO` reports row count `0`.

The official JSON attribute function documentation defines `JSON_LENGTH()` as
the shallow length of the document or path-selected value, returning `NULL` for
`NULL` arguments or unmatched paths. It defines `JSON_TYPE()` as returning a
UTF-8 string label for the JSON value type and errors for invalid JSON input.

## Supported Scope

Supported:

- no-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projections over persistent and temporary
  base tables already supported by the row-scalar expression framework;
- `JSON_TYPE(json_doc)` with one document argument;
- `JSON_LENGTH(json_doc)` and `JSON_LENGTH(json_doc, path)`;
- JSON document operands from:
  - SQL string literals;
  - SQL `NULL`;
  - JSON descriptor columns;
  - nonbinary string descriptor columns containing JSON text;
  - supported `JSON_EXTRACT()` results in scalar expression paths;
- `JSON_LENGTH()` path operands from:
  - SQL string literals in the current simple JSON path subset;
  - SQL `NULL`;
- simple JSON paths already specified for `JSON_EXTRACT()`:
  - `$`;
  - `.identifier` member legs where the identifier is ASCII
    `[A-Za-z_$][A-Za-z0-9_$]*`;
  - `."member"` quoted member legs with JSON-string-compatible ASCII content;
  - `[N]` array legs where `N` is a nonnegative decimal integer with no leading
    zero unless it is exactly `0`;
- type labels for the current MyLite JSON storage/read subset:
  - `OBJECT`;
  - `ARRAY`;
  - `BOOLEAN`;
  - `NULL` for JSON null;
  - `STRING`;
  - `INTEGER` for signed integer JSON numbers;
- result labels, aliases, result ownership, affected-row state, and warning
  count through existing public result conventions.

Deferred:

- JSON decimal and exponent-number parsing for `JSON_TYPE()` / `JSON_LENGTH()`;
  MySQL returns `DOUBLE` or `DECIMAL` labels for those values, but MyLite's
  current canonical JSON storage parser intentionally supports only signed
  integer JSON numbers;
- `JSON_LENGTH()` wildcard paths, recursive descent, array ranges, `last`, path
  filters, or any path shape that can match more than one value;
- arbitrary expression arguments outside the existing scalar and row-scalar
  envelopes;
- `JSON_TYPE()` / `JSON_LENGTH()` over binary strings, numeric SQL values,
  booleans, temporal values, parameters, user variables, subqueries, aggregate
  results, or arbitrary function results outside the explicitly supported
  `JSON_EXTRACT()` scalar nesting;
- use in `WHERE`, expression `ORDER BY`, grouping, distinct, aggregate
  arguments, `HAVING`, DML assignment expressions, defaults, generated columns,
  functional indexes, JSON path indexes, and JSON comparison semantics;
- JSON mutation and utility functions, `JSON_TABLE()`, `JSON_VALUE()`,
  `CAST(... AS JSON)`, and `MEMBER OF()`;
- full Unicode path member names, Unicode escape normalization beyond current
  JSON string handling, non-ASCII collation behavior, and full protocol-grade
  expression metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse behavior, result
  lifetime, and cleanup. Results are exposed as text/integer strings or SQL
  `NULL` through the existing public result object.
- Statement context: owns diagnostics, warning count, affected rows, row-count
  state, and result finalization. Supported successful invocations report
  `warning_count == 0`.
- Lexer/parser/AST: admits `JSON_TYPE()` and `JSON_LENGTH()` syntax, preserves
  source spans for labels and diagnostics, and records wrong-arity nodes where
  MySQL reports native function parameter-count errors. Parser code does not
  inspect table descriptors or parse JSON documents.
- Analyzer/planner: admits the functions only in current scalar and row-scalar
  expression envelopes, resolves descriptor columns through MyLite descriptors,
  rejects unsupported argument and path shapes before generated SQLite SQL is
  executed, and keeps result metadata within existing expression metadata
  limits.
- JSON runtime module: owns MyLite-authored JSON parsing, simple path
  evaluation, value-type mapping, and shallow-length calculation. It must not
  depend on SQLite JSON1 or copied implementation code.
- SQLite physical execution: row-scalar projection is lowered to internal
  MyLite SQLite scalar functions registered through public SQLite APIs. SQLite
  scans and projects rows; MyLite does not materialize table rows in C for this
  feature.
- Catalog/storage/VFS: unchanged. No successful JSON introspection statement
  mutates catalog rows, descriptor versions, descriptor caches, catalog
  generation, `sqlite_schema_generation`, `.mylite` preamble bytes, or shifted
  SQLite payload invariants.

## Grammar

Supported SQL expression shapes:

```sql
JSON_TYPE(json_doc)
JSON_LENGTH(json_doc)
JSON_LENGTH(json_doc, path)
```

MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= JSON_TYPE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_TYPE_FUNCTION, B, R);
}

expression(A) ::= JSON_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_FUNCTION, B, R);
}

expression(A) ::= JSON_LENGTH(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_FUNCTION, B, C, R);
}

expression(A) ::= JSON_TYPE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= JSON_TYPE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR, C, R);
}

expression(A) ::= JSON_LENGTH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= JSON_LENGTH(T) LPAREN expression(B) COMMA expression(C)
                  COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR, D, R);
}

identifier(A) ::= JSON_TYPE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

identifier(A) ::= JSON_LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Semantics

For supported `JSON_TYPE(json_doc)`:

- SQL `NULL` returns SQL `NULL`.
- JSON objects return `OBJECT`.
- JSON arrays return `ARRAY`.
- JSON `true` and `false` return `BOOLEAN`.
- JSON `null` returns the SQL string `NULL`.
- JSON strings return `STRING`.
- Signed integer JSON numbers return `INTEGER`.
- Unsupported valid JSON shapes, including decimal/exponent numbers or
  non-ASCII string content currently unsupported by MyLite JSON storage
  normalization, return a deterministic MyLite unsupported diagnostic.
- Invalid JSON document text reports MySQL-compatible invalid JSON text
  diagnostics.

For supported `JSON_LENGTH(json_doc[, path])`:

- SQL `NULL` document arguments return SQL `NULL` without validating the path.
  SQL `NULL` path arguments return SQL `NULL` after a non-`NULL` document is
  validated as an acceptable JSON document argument.
- Without a path, the document itself is measured.
- With a path, the path-selected value is measured. If no value is matched,
  SQL `NULL` is returned.
- Scalars, including JSON strings, signed integer numbers, booleans, and JSON
  null, have length `1`.
- Arrays return their direct element count.
- Objects return their direct member count.
- Nested containers count as one direct value at their parent level.

## SQLite Handling

Generated row-scalar SQL shapes:

```sql
_mylite_json_type(document_sql)
_mylite_json_length(document_sql)
_mylite_json_length_path(document_sql, path_sql)
```

Literal values are bound as parameters rather than interpolated. Descriptor
column identifiers are quoted. Internal SQLite callbacks are deterministic
scalar functions registered through public SQLite APIs. They inspect SQLite
value types only for the planned subset and delegate JSON parsing/type/length
work to MyLite-owned JSON helpers.

No SQLite JSON1 function, optional SQLite JSON extension, or SQLite fork patch
is introduced.

## Diagnostics

- `JSON_TYPE()` wrong argument count: MySQL-compatible `1582 / 42000` with
  native function name `JSON_TYPE`.
- `JSON_LENGTH()` wrong argument count: MySQL-compatible `1582 / 42000` with
  native function name `JSON_LENGTH`.
- Non-JSON SQL scalar document argument: MySQL-compatible `3146 / 22032` for
  the supported subset.
- Binary string document argument: MySQL-compatible `3144 / 22032`.
- Invalid JSON document text: MySQL-compatible `3141 / 22032`.
- Invalid JSON path syntax: MySQL-compatible `3143 / 42000`.
- Unsupported document or path shape, including wildcard paths and
  decimal/exponent JSON numbers: deterministic MyLite unsupported diagnostic.
- Unknown row-scalar descriptor column: existing unknown-column diagnostics.
- Unsupported scalar or row-scalar argument shape: deterministic MyLite
  unsupported diagnostic.
- Allocation failure: `MYLITE_NOMEM` with existing diagnostics.
- SQLite callback misuse or registration failure: existing internal/SQLite
  failure handling.

## Tests

MySQL expectation script coverage:

- literal `JSON_TYPE()` over `NULL`, object, array, boolean, JSON null, string,
  signed integer number, and current MySQL decimal/exponent observations;
- literal `JSON_LENGTH()` over `NULL`, scalar, array, object, nested object,
  simple matched path, missing path, `NULL` path, and `NULL` document with
  invalid path text;
- table-backed JSON and nonbinary string descriptor documents;
- `JSON_EXTRACT()` nesting where admitted;
- wrong argument counts;
- non-JSON SQL scalar document arguments;
- invalid JSON document text, including invalid document text with `NULL` path;
- binary document arguments;
- invalid and unsupported path forms.

MyLite C coverage:

- scalar no-source, `DUAL`, and `DO` behavior;
- row-scalar projections over JSON and nonbinary string descriptor columns;
- simple `JSON_LENGTH()` path selection and unmatched path `NULL`;
- reopen persistence for JSON rows used by row-scalar introspection;
- row count, warning count, and absence/presence of result rows through the
  existing public result conventions;
- deterministic diagnostics for wrong arity, unsupported argument shapes,
  unknown columns, invalid JSON, invalid paths, binary inputs, and deferred
  decimal/exponent/wildcard shapes.

## Verification

Before committing this phase:

1. Run the new MySQL 8.4.9 expectation script.
2. Build the new runtime test and parser test.
3. Run focused JSON parser/runtime CTests.
4. Run `cmake --workflow --preset check`.
5. Review the diff for architecture boundaries, MySQL evidence, descriptor
   authority, JSON parser ownership, generated SQLite SQL shape, binding,
   memory cleanup, compatibility wording, and test relevance.
