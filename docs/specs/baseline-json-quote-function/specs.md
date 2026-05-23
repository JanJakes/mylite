# Baseline JSON_QUOTE Function

## Summary

This phase adds a narrow MySQL-compatible JSON string quoting slice:

```sql
JSON_QUOTE(string)
```

The goal is to cover a common JSON creation helper without widening MyLite's
general expression engine. `JSON_QUOTE()` converts a supported nonbinary SQL
string value into the JSON string literal text that would represent that string.
It does not parse the argument as JSON, does not accept general SQL scalars, and
does not add JSON mutation, aggregation, expression predicates, expression
ordering, generated columns, functional indexes, or a SQLite JSON1 dependency.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing JSON and scalar expression slices:
  - `docs/specs/baseline-json-extract-functions/specs.md`
  - `docs/specs/baseline-json-construction-functions/specs.md`
  - `docs/specs/baseline-json-introspection-functions/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - JSON creation functions:
    <https://dev.mysql.com/doc/refman/8.4/en/json-creation-functions.html>
  - JSON function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/json-functions.html>
  - JSON data type:
    <https://dev.mysql.com/doc/refman/8.4/en/json.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_json_quote_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- `JSON_QUOTE(string)` requires exactly one argument. Wrong arity fails with
  `1582 / 42000`.
- SQL `NULL` returns SQL `NULL`.
- SQL string input is quoted as a JSON string literal. JSON-looking text such as
  `null`, `"null"`, and `[1, 2, 3]` is not parsed as JSON; it is returned as a
  quoted JSON string.
- SQL literal escape decoding happens before JSON quoting. With default SQL
  mode, `'a\nb'` becomes a newline and `JSON_QUOTE()` returns bytes for
  `"a\nb"`. With `NO_BACKSLASH_ESCAPES`, the same SQL literal remains
  backslash-plus-`n`, and the backslash is JSON-escaped.
- Interior double quotes and backslashes are JSON-escaped. Control characters
  are represented by JSON escapes.
- Numeric and boolean SQL scalar arguments fail with `3064 / HY000`.
- JSON column arguments fail with `3064 / HY000`; this function expects a SQL
  string, not an already typed JSON value.
- Binary string arguments, binary casts, and binary string descriptor columns
  fail with `3144 / 22032`.
- Nonbinary string descriptor columns are supported. `VARCHAR` results report a
  `VAR_STRING` protocol type in MySQL; wider text columns can report a blob/text
  family protocol type. This baseline keeps MyLite's existing limited scalar
  metadata model and reports a nullable binary-flagged `VAR_STRING` for
  supported `JSON_QUOTE()` projections.
- Successful supported invocations emit no warnings. Scalar `SELECT` follows
  existing result-set conventions; `DO` reports row count `0`.

The official JSON creation-function documentation describes `JSON_QUOTE()` as
returning a `utf8mb4` string containing the argument wrapped as a JSON string
literal and returning `NULL` for SQL `NULL`.

## Supported Scope

Supported:

- no-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projections over persistent and temporary
  base tables already supported by the row-scalar expression framework;
- `JSON_QUOTE(string)` with exactly one argument;
- SQL string literal arguments decoded by MyLite's current SQL string literal
  rules, including the active `NO_BACKSLASH_ESCAPES` setting;
- SQL `NULL` arguments;
- nonbinary string descriptor columns in row-scalar projections;
- result labels, aliases, result ownership, affected-row state, and warning
  count through existing public result conventions.

Deferred:

- binary string literals, binary casts, binary descriptor columns, and binary
  character set metadata beyond the MySQL-compatible rejection path;
- JSON descriptor column inputs, numeric SQL values, booleans, temporal values,
  enum/set values, parameters, user variables, subqueries, aggregate/window
  results, and arbitrary function results;
- nested `JSON_QUOTE()` or `JSON_QUOTE(JSON_UNQUOTE(...))` expression trees
  outside a later general expression slice;
- use in predicates, expression `ORDER BY`, grouping, distinct, aggregate
  arguments, `HAVING`, DML assignment expressions, defaults, generated columns,
  functional indexes, and JSON comparison semantics;
- exact MySQL protocol metadata width/type changes for every input column
  family; the current MyLite metadata remains a limited scalar expression
  approximation;
- invalid UTF-8 diagnostics and full Unicode validation beyond the current
  string storage and JSON writer behavior.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse behavior, result
  lifetime, and cleanup. Results are exposed as text or SQL `NULL` through the
  existing result object.
- Statement context: owns diagnostics, warning count, affected rows, row-count
  state, and result finalization. Supported successful invocations report
  `warning_count == 0`.
- Lexer/parser/AST: admits `JSON_QUOTE` as a nonreserved function token,
  preserves source spans for labels and diagnostics, and records wrong-arity
  nodes where MySQL reports native function parameter-count errors. Parser code
  does not inspect table descriptors or perform JSON escaping.
- Analyzer/planner: admits the function only in current scalar and row-scalar
  expression envelopes, resolves descriptor columns through MyLite descriptors,
  rejects unsupported argument shapes before generated SQLite SQL is executed,
  and keeps result metadata within existing expression metadata limits.
- JSON runtime module: owns MyLite-authored JSON string literal escaping. It
  may reuse the existing JSON writer used by constructor functions, but it must
  not depend on SQLite JSON1.
- SQLite physical execution: row-scalar projection is lowered to an internal
  MyLite SQLite scalar function registered through public SQLite APIs. SQLite
  scans and projects rows; MyLite does not materialize full tables in C for
  this feature.
- Catalog/storage/VFS: unchanged. No successful `JSON_QUOTE()` statement
  mutates catalog rows, descriptor versions, descriptor caches, catalog
  generation, `sqlite_schema_generation`, `.mylite` preamble bytes, or shifted
  SQLite payload invariants.

## Grammar

Supported SQL expression shape:

```sql
JSON_QUOTE(string)
```

MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= JSON_QUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_QUOTE_FUNCTION, B, R);
}

expression(A) ::= JSON_QUOTE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= JSON_QUOTE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR, C, R);
}

identifier(A) ::= JSON_QUOTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Semantics

For supported `JSON_QUOTE(string)`:

- SQL `NULL` returns SQL `NULL`.
- A non-`NULL` string argument returns a SQL string whose bytes are a JSON string
  literal:
  - output starts and ends with double quotes;
  - interior double quotes become `\"`;
  - interior backslashes become `\\`;
  - backspace, form feed, newline, carriage return, and tab become the matching
    short JSON escapes;
  - other control bytes below `0x20` become lowercase `\u00xx` escapes;
  - other bytes are copied unchanged by the baseline writer.
- The input is never parsed as JSON. For example, the SQL string `null` returns
  the JSON string literal `"null"`, not the JSON null value.
- SQL mode affects only SQL literal decoding before the value reaches
  `JSON_QUOTE()`.
- Successful scalar `SELECT` returns one row with the existing scalar result
  conventions. Successful `DO` returns no result columns or rows and affected
  rows `0`.

## SQLite Handling

Generated row-scalar SQL shape:

```sql
_mylite_json_quote(argument_sql)
```

Literal values are bound as parameters rather than interpolated. Descriptor
column identifiers are quoted. The internal SQLite callback is registered as a
deterministic scalar function through public SQLite APIs. It accepts `NULL` and
SQLite text values from the already-planned subset and delegates JSON string
escaping to MyLite-owned JSON helpers.

No SQLite JSON1 function, optional SQLite JSON extension, SQLite
`UPDATE`/`SELECT` syntax extension, or SQLite fork patch is introduced.

## Diagnostics

- Wrong argument count: MySQL-compatible `1582 / 42000` with native function
  name `JSON_QUOTE`.
- Numeric or boolean scalar literals: MySQL-compatible `3064 / HY000`.
- JSON descriptor columns: MySQL-compatible `3064 / HY000`.
- Binary string literals, binary casts, and binary descriptor columns:
  MySQL-compatible `3144 / 22032`.
- Unknown descriptor columns: existing MySQL-compatible unknown-column
  diagnostics for the active clause.
- Unsupported expression shapes: deterministic MyLite unsupported diagnostics
  that name the limited `JSON_QUOTE()` argument surface.
- Physical SQLite allocation or execution failures: existing statement context
  allocation and physical SQLite failure diagnostics.
- Public API misuse: unchanged.

## Performance

No table rows are materialized in C. For table-backed projections, SQLite scans
and filters rows using generated SQL over physical descriptor-backed tables and
invokes `_mylite_json_quote()` for each projected value. The per-row MyLite work
is linear in the input string length and allocates only the output string.

## Tests

Runtime tests must cover:

- literal quoting for ordinary text, JSON-looking text, quotes, backslashes, and
  newline escape behavior;
- `NO_BACKSLASH_ESCAPES` interaction;
- SQL `NULL`;
- `SELECT ... FROM DUAL`, `DO`, row count, warning count, labels, and aliases;
- row-scalar projection over `VARCHAR` and `TEXT` descriptor columns, including
  close/reopen persistence;
- unknown column diagnostics;
- wrong arity diagnostics;
- numeric, boolean, JSON descriptor, binary cast, and binary descriptor
  rejection;
- zero-initialized cleanup through ordinary result and error paths;
- result-column metadata in the existing scalar and row-scalar metadata tests.

MySQL expectation coverage lives in
`packages/libmylite/tests/mysql_baseline_json_quote_function_expectations.sh`.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/functions-json.md` to mark
`JSON_QUOTE()` as a limited supported function. Do not claim JSON mutation,
general expression support, binary-string support, JSON column support,
predicates, ordering expressions, DML assignment expressions, or exact
protocol metadata for every input family.
