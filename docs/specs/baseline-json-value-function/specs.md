# Baseline JSON_VALUE Function

## Summary

This phase adds a narrow `JSON_VALUE()` read slice:

```sql
JSON_VALUE(json_doc, path)
```

The goal is to expose the common scalar JSON path helper for no-source
`SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table row-scalar `SELECT`
projections. The implementation builds on MyLite's existing JSON document
parser, simple path evaluator, scalar expression planner, and SQLite scalar
function registration. It does not add `RETURNING`, `ON EMPTY`, `ON ERROR`,
wildcard/range path semantics, JSON predicates, expression ordering, DML
assignment expressions, generated columns, functional indexes, or a dependency
on SQLite JSON1.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing JSON slices:
  - `docs/specs/baseline-json-type/specs.md`
  - `docs/specs/baseline-json-extract-functions/specs.md`
  - `docs/specs/baseline-json-introspection-functions/specs.md`
  - `docs/specs/baseline-json-keys-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - JSON function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html>
  - JSON search functions, including `JSON_VALUE()`:
    <https://dev.mysql.com/doc/refman/8.4/en/json-search-functions.html>
  - JSON data type and path syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/json.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_json_value_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- `JSON_VALUE(json_doc, path)` is the admitted baseline form.
- Zero-, one-, and three-argument forms are syntax errors in MySQL 8.4.9.
- The `path` argument is syntax-level restricted to a JSON path string literal
  in the baseline forms observed here; numeric, `NULL`, and variable path
  operands fail with a syntax error.
- SQL `NULL` document input returns SQL `NULL`.
- Missing path matches return SQL `NULL` without warnings.
- A matched JSON string returns the unquoted string value.
- A matched JSON number or boolean returns its visible text representation.
- A matched JSON `null` returns SQL `NULL`, unlike
  `JSON_UNQUOTE(JSON_EXTRACT(...))`, which returns the text `null`.
- A matched JSON object or array returns normalized JSON text.
- Invalid JSON document text returns SQL `NULL` and emits warning
  `3141 / 22032`; row-scalar projections append one warning for each invalid
  row document observed by the internal SQLite function.
- Non-JSON SQL scalar document arguments such as SQL integers fail with
  `3146 / 22032`.
- Invalid path syntax fails with `3143 / 42000`.
- `RETURNING`, `DEFAULT ... ON EMPTY`, and `DEFAULT ... ON ERROR` are valid
  MySQL features but are deferred from this baseline.
- Successful supported invocations emit no warnings. Scalar `SELECT` follows
  existing result-set conventions; `DO` reports row count `0`.

## Supported Scope

Supported:

- no-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projections over persistent and temporary
  base tables already supported by the row-scalar expression framework;
- `JSON_VALUE(json_doc, path)` with exactly one path argument;
- JSON document operands from:
  - SQL string literals;
  - SQL `NULL`;
  - JSON descriptor columns;
  - nonbinary string descriptor columns containing JSON text;
- simple JSON path string literals already specified for `JSON_EXTRACT()`:
  - `$`;
  - `.identifier` member legs where the identifier is ASCII
    `[A-Za-z_$][A-Za-z0-9_$]*`;
  - `."member"` quoted member legs with JSON-string-compatible ASCII content;
  - `[N]` array legs where `N` is a nonnegative decimal integer with no leading
    zero unless it is exactly `0`;
- result labels, aliases, result ownership, affected-row state, and warning
  count through existing public result conventions.

Deferred:

- `RETURNING` result type clauses;
- `ON EMPTY` and `ON ERROR` behavior controls;
- path operands that are not string literals, including `NULL`, parameters,
  variables, columns, function calls, or arbitrary expressions;
- wildcard paths, recursive descent, array ranges, `last`, path filters, or any
  path shape that can match more than one value;
- arbitrary expression arguments outside the existing scalar and row-scalar
  envelopes;
- binary strings, temporal values, parameters, user variables, subqueries,
  aggregate results, or arbitrary function results as document operands;
- use in `WHERE`, expression `ORDER BY`, grouping, distinct, aggregate
  arguments, `HAVING`, DML assignment expressions, defaults, generated columns,
  functional indexes, JSON path indexes, and JSON comparison semantics;
- `JSON_TABLE()`, `CAST(... AS JSON)`, and `MEMBER OF()`;
- full Unicode path member names, Unicode escape normalization beyond current
  JSON string handling, non-ASCII collation behavior, and full protocol-grade
  expression metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse behavior, result
  lifetime, and cleanup. Results are exposed as text or SQL `NULL` through the
  existing public result object.
- Statement context: owns diagnostics, warning count, affected-row state,
  row-count state, and result finalization. Supported successful invocations
  report `warning_count == 0`; invalid JSON document text in scalar and
  row-scalar execution appends MySQL-compatible warnings and returns SQL
  `NULL`.
- Lexer/parser/AST: admits `JSON_VALUE()` syntax for the supported two-argument
  form, preserves source spans for labels and diagnostics, and records the
  function as an identifier where MySQL permits function names as identifiers.
  Parser code does not inspect table descriptors or parse JSON documents.
- Analyzer/planner: admits the function only in the current scalar and
  row-scalar expression envelopes, resolves descriptor columns through MyLite
  descriptors, rejects unsupported argument and path shapes before generated
  SQLite SQL is executed, and keeps result metadata within existing expression
  metadata limits.
- JSON runtime module: owns MyLite-authored JSON document parsing, simple path
  evaluation, JSON value-to-SQL-text conversion, JSON `null` to SQL `NULL`
  conversion, and canonical serialization of object/array results. It must not
  depend on SQLite JSON1 or copied implementation code.
- SQLite physical execution: row-scalar projection is lowered to an internal
  MyLite SQLite scalar function registered through public SQLite APIs. SQLite
  scans and projects rows; MyLite does not materialize table rows in C for this
  feature.
- Catalog/storage/VFS: unchanged. No successful `JSON_VALUE()` statement
  mutates catalog rows, descriptor versions, descriptor caches, catalog
  generation, `sqlite_schema_generation`, `.mylite` preamble bytes, or shifted
  SQLite payload invariants.

## Grammar

Supported SQL expression shape:

```sql
JSON_VALUE(json_doc, json_path_string_literal)
```

MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= JSON_VALUE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_VALUE_FUNCTION, B, C, R);
}

identifier(A) ::= JSON_VALUE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

The parser accepts the second argument as an expression node so it can preserve
the existing scalar-expression error path, but the analyzer admits only string
literal path nodes for this phase. `RETURNING`, `ON EMPTY`, and `ON ERROR`
tokens after the second argument are outside this grammar and remain syntax
errors in MyLite until specified separately.

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Semantics

For supported `JSON_VALUE(json_doc, path)`:

- SQL `NULL` document returns SQL `NULL`.
- The path is decoded from a SQL string literal and validated with the current
  simple path parser before SQLite row-scalar SQL is generated.
- If the path matches no value, return SQL `NULL`.
- If the path matches JSON `null`, return SQL `NULL`.
- If the path matches a JSON string, return the decoded string value.
- If the path matches a JSON integer number or boolean, return its visible text
  representation.
- If the path matches an object or array, return normalized JSON text using the
  existing MyLite JSON emitter.
- Successful supported invocations leave `warning_count == 0`.
- Result metadata is limited to MySQL-shaped nullable `VAR_STRING` metadata for
  the session character set, with a 512-character display length scaled to the
  connection character width and the `BINARY` flag observed for MySQL 8.4.9.

## Diagnostics

Supported diagnostics:

- wrong arity and unsupported clauses: syntax error through existing parser
  behavior for the observed MySQL baseline;
- invalid JSON document text in scalar or row-scalar execution: return SQL
  `NULL` and append warning `3141 / 22032`;
- non-JSON SQL scalar document argument: `3146 / 22032`;
- binary document argument: `3144 / 22032`;
- invalid JSON path syntax: `3143 / 42000`;
- unsupported path shape, including wildcards/ranges/recursive descent:
  deterministic MyLite unsupported-path diagnostic;
- unsupported argument expression or descriptor type: deterministic MyLite
  unsupported-expression diagnostic;
- missing row-scalar column: existing descriptor-driven unknown-column
  diagnostic;
- allocation failure: `MYLITE_NOMEM` through the current public API behavior;
- public API misuse: unchanged.

## Physical SQLite Handling

MyLite lowers row-scalar `JSON_VALUE()` projections to:

```sql
_mylite_json_value(document_sql, path_sql)
```

The generated SQL is built from planned descriptor expressions. Document column
references use quoted physical identifiers and stable physical table aliases.
Literal JSON documents and paths are bound as parameters. The internal SQLite
function parses only its arguments, appends warning diagnostics through the
owning MyLite connection for invalid document text, and returns either SQL
`NULL` or text. No SQLite JSON1 dependency or SQLite fork hook is required.

## Tests

Add fast plain C coverage in
`packages/libmylite/tests/runtime_json_value_function_test.c` and MySQL
expectation coverage in
`packages/libmylite/tests/mysql_baseline_json_value_function_expectations.sh`.

Cover:

- no-source, `DUAL`, and `DO` scalar results;
- string, integer, boolean, JSON `null`, object, array, missing path, and SQL
  `NULL` document behavior;
- table-backed JSON and nonbinary string descriptor columns;
- labels, aliases, warning count, and row-count conventions;
- invalid JSON document warning in no-source scalar execution and table-backed
  row-scalar execution;
- MySQL-shaped scalar and row-scalar result-column metadata;
- invalid path, SQL scalar document argument, unknown columns, unsupported path
  operand, and deferred `RETURNING` / `ON EMPTY` / `ON ERROR` syntax.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-json.md`, and the JSON
path literal row in `docs/compatibility/type-system-literals-conversion.md`
only for this exact limited `JSON_VALUE()` path surface. Do not claim full
`JSON_VALUE()`, return-type conversion, empty/error clauses, full JSON paths,
predicate/order/grouping support, DML expression support, generated columns, or
functional indexes.
