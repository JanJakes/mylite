# Baseline SHOW COLUMNS WHERE

## Summary

This phase extends descriptor-driven `SHOW COLUMNS`, `SHOW FIELDS`,
`SHOW FULL COLUMNS`, and `SHOW FULL FIELDS` with a limited `WHERE` filter over
visible output columns:

```sql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} table_name
    [ {FROM | IN} schema_name ]
    WHERE predicate
```

The statement remains MyLite-owned metadata. It reads durable schema, table,
column, and index descriptors, builds the same result rows as the existing
`SHOW COLUMNS` and `SHOW FULL COLUMNS` surfaces, and evaluates the admitted
predicate subset in MyLite runtime code before appending rows. It does not query
SQLite schema text, SQLite PRAGMAs, or `INFORMATION_SCHEMA`, and it does not
change descriptors, physical rows, catalog generations, or file format state.

## Compatibility Authority

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `COMPATIBILITY.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW VARIABLES WHERE`,
  `SHOW TABLE STATUS WHERE`, and `SHOW INDEX WHERE` designs:
  - `docs/specs/baseline-show-columns-introspection/specs.md`
  - `docs/specs/baseline-show-full-columns/specs.md`
  - `docs/specs/baseline-show-variables-where/specs.md`
  - `docs/specs/baseline-show-table-status-where/specs.md`
  - `docs/specs/baseline-show-index-where/specs.md`
- Official MySQL 8.4 documentation:
  - `SHOW COLUMNS`: <https://dev.mysql.com/doc/refman/8.4/en/show-columns.html>
  - `SHOW` statement filters:
    <https://dev.mysql.com/doc/refman/8.4/en/show.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_show_columns_where_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against a local MySQL 8.4.9 runtime establish these expectations
for this slice:

- `SHOW COLUMNS`, `SHOW FIELDS`, `SHOW FULL COLUMNS`, and `SHOW FULL FIELDS`
  accept a trailing `WHERE` clause after the table and optional schema clauses.
- `LIKE` and `WHERE` are mutually exclusive; `SHOW COLUMNS FROM t LIKE 'i%'
  WHERE Field = 'id'` is a syntax error.
- The `WHERE` predicate is evaluated against displayed output column names.
  Non-`FULL` output columns are `Field`, `Type`, `Null`, `Key`, `Default`, and
  `Extra`. `FULL` output additionally admits `Collation`, `Privileges`, and
  `Comment` in the displayed full-column order.
- Referencing a `FULL`-only column such as `Collation` from non-`FULL`
  `SHOW COLUMNS` reports `1054 / 42S22` unknown-column diagnostics.
- Backtick-quoted output column names such as `` `Default` `` and `` `Null` ``
  are accepted. Output column name resolution is ASCII case-insensitive for the
  observed subset.
- MySQL accepts the special qualifier `COLUMNS.Field`; other qualifiers such as
  `t.Field` are rejected as unknown columns. MyLite deliberately admits only
  unqualified output-column references in this slice to keep the grammar surface
  aligned with the existing MyLite `SHOW ... WHERE` filters.
- Text comparisons over current output cells are ASCII case-insensitive in the
  observed default metadata collation. `LIKE` is also case-insensitive for the
  observed ASCII values.
- SQL `NULL` output cells such as `Default` for no-default or explicit-null
  defaults and `Collation` for non-string columns follow normal three-valued
  logic. `Default <=> NULL` and `Default IS NULL` match those columns.
- MySQL accepts numeric-literal comparisons against output strings with warning
  `1292` per compared row in observed cases. MyLite deliberately defers that
  warning-producing coercion surface and rejects non-string filter literals
  deterministically in this slice.
- MySQL accepts broader predicates such as `REGEXP`; MyLite defers them with a
  deterministic unsupported diagnostic for this slice.
- Successful supported filters leave `@@warning_count == 0`,
  `@@error_count == 0`, and make `ROW_COUNT()` return `-1`.

## Ownership Boundaries

- Public API: no ABI or public-header change. `mylite_execute()` returns the
  existing row-result handle for successful metadata statements.
- Statement context: successful `SHOW COLUMNS WHERE` is a result-producing
  statement. It reports affected rows `0`, warning count `0`, and updates the
  previous row count to `-1`.
- Lexer/parser/AST: grammar admission belongs to MyLite's parser. The existing
  predicate AST is reused, but runtime admits only the subset specified here.
- Runtime/analyzer: runtime resolves the target schema/table, rejects reserved
  names and unsupported object kinds through the existing `SHOW COLUMNS` path,
  loads descriptor-owned columns and indexes, builds candidate output cells,
  evaluates the admitted predicate subset, and appends matching rows.
- Catalog: descriptors remain authoritative for schema, table, column, primary
  key, and secondary-index metadata. This feature reads descriptors but does not
  mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or SQLite schema generation.
- SQLite physical storage: no generated SQLite SQL is added for this feature.
  `SHOW COLUMNS WHERE` does not inspect SQLite metadata.
- Storage/VFS/file format: no `.mylite` preamble or shifted SQLite payload
  behavior changes.

## Syntax

The independent MyLite subset is:

```ebnf
show_columns_statement:
    SHOW show_columns_full_opt show_columns_keyword show_columns_table_keyword
        table_name show_columns_schema_opt show_columns_filter_opt

show_columns_full_opt:
    empty
  | FULL

show_columns_keyword:
    COLUMNS
  | FIELDS

show_columns_table_keyword:
    FROM
  | IN

show_columns_schema_opt:
    empty
  | FROM identifier
  | IN identifier

show_columns_filter_opt:
    empty
  | LIKE string_literal
  | WHERE show_columns_predicate
```

`LIKE` and `WHERE` are mutually exclusive. `EXTENDED`, `ORDER BY`, `LIMIT`,
multiple filters, filters before the optional schema clause, non-string `LIKE`
operands, and schema qualifiers after `DESCRIBE` / `EXPLAIN` remain outside
this slice.

### MyLite Lemon-Syntax Snippet

```lemon
show_columns_statement(A) ::=
    SHOW(S) show_columns_full_opt(F) show_columns_keyword show_columns_table_keyword
    table_name(T) show_columns_schema_opt(D) show_columns_filter_opt(W). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, F, T, D, W);
}

show_columns_full_opt(A) ::= . { A = false; }
show_columns_full_opt(A) ::= FULL. { A = true; }
show_columns_keyword ::= COLUMNS.
show_columns_keyword ::= FIELDS.
show_columns_table_keyword ::= FROM.
show_columns_table_keyword ::= IN.
show_columns_schema_opt(A) ::= . { A = NULL; }
show_columns_schema_opt(A) ::= FROM identifier(D). { A = D; }
show_columns_schema_opt(A) ::= IN identifier(D). { A = D; }
show_columns_filter_opt(A) ::= . { A = NULL; }
show_columns_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_columns_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}
```

The implementation may keep separate existing constructor functions for normal
and `FULL` statements; the snippet describes the intended admitted grammar, not
MySQL grammar text.

## WHERE Predicate Subset

The filter is evaluated against the current displayed `SHOW COLUMNS` output
row. For non-`FULL` statements, the admitted column names are exactly:

```text
Field
Type
Null
Key
Default
Extra
```

For `FULL` statements, the admitted column names are exactly:

```text
Field
Type
Collation
Null
Key
Default
Extra
Privileges
Comment
```

Column names are resolved ASCII case-insensitively. Backtick quoting does not
change the name. Qualified column references are reported as unknown columns in
the `WHERE` clause for this slice.

The admitted predicate subset is:

- `column = string_literal`
- `column <=> string_literal`
- `column <> string_literal`
- `column != string_literal`
- `column < string_literal`
- `column <= string_literal`
- `column > string_literal`
- `column >= string_literal`
- `column <=> NULL`
- `column LIKE string_literal`
- `column NOT LIKE string_literal`
- `column IN (string_literal_or_NULL [, ...])`
- `column NOT IN (string_literal_or_NULL [, ...])`
- `column IS NULL`
- `column IS NOT NULL`
- parenthesized predicates
- `NOT predicate`
- `predicate AND predicate`
- `predicate OR predicate`

Comparisons and `LIKE` matching use ASCII case-insensitive comparison for
current descriptor-generated metadata cells. `LIKE` uses `%`, `_`, and
backslash escaping through the existing SHOW pattern matcher.

SQL `NULL` output cells follow normal three-valued logic. `Default <=> NULL`
and `Default IS NULL` match columns whose displayed default is SQL `NULL`.
`Collation IS NULL` in `SHOW FULL COLUMNS` matches non-character columns.

The following are intentionally outside this slice and must fail
deterministically instead of being approximated:

- numeric, decimal, float, hex, bit, boolean, national-string, introducer, or
  parameter literals in `WHERE`;
- column-to-column comparisons;
- functions such as `LOWER(Field)`;
- `BETWEEN`, `REGEXP`, `RLIKE`, `XOR`, `IS TRUE`, `IS FALSE`, and arbitrary
  expression predicates;
- subqueries and CTEs;
- `ORDER BY`, `LIMIT`, and `LIKE ... WHERE` combinations.

## Result Semantics

Successful non-`FULL` statements return a row result with columns:

```text
Field
Type
Null
Key
Default
Extra
```

Successful `FULL` statements return a row result with columns:

```text
Field
Type
Collation
Null
Key
Default
Extra
Privileges
Comment
```

Rows are generated by the existing descriptor-driven `SHOW COLUMNS` logic. The
`WHERE` predicate only controls whether a fully built candidate row is appended.
It does not affect descriptor ordering, metadata text generation, warning
counts, or statement context beyond normal result-statement state.

Successful supported filters report:

- affected rows `0`;
- warning count `0`;
- no stored warnings for the admitted subset;
- `ROW_COUNT()` as `-1` for the following statement.

## Diagnostics

- Syntax errors, `LIKE ... WHERE`, `ORDER BY`, `LIMIT`, `EXTENDED`, non-string
  `LIKE` operands, and unsupported grammar use the existing parser syntax-error
  diagnostics.
- Missing default schema, unknown schema, unknown table, reserved MyLite names,
  unsupported object kinds, and catalog/storage failures use the existing
  descriptor-driven `SHOW COLUMNS` diagnostics.
- Unknown output columns, `FULL`-only output columns referenced from non-`FULL`
  statements, and qualified output-column references report deterministic
  `1054 / 42S22` unknown-column diagnostics.
- Unsupported predicate shapes such as functions, column-to-column comparison,
  `BETWEEN`, `REGEXP`, `RLIKE`, `XOR`, `IS TRUE`, and `IS FALSE` report
  deterministic MyLite unsupported diagnostics.
- Unsupported literals report deterministic MyLite diagnostics that the
  admitted `SHOW COLUMNS WHERE` subset supports only string and `NULL`
  predicates.
- String literals containing decoded NUL bytes are rejected because the current
  MyLite result cells are C strings and NUL-bearing SHOW predicate semantics are
  outside this slice.
- Allocation failures report the existing out-of-memory diagnostics.

## Physical SQLite Handling

No new SQLite SQL is generated. The feature evaluates descriptor-built metadata
rows in MyLite runtime code and appends matching rows to a MyLite result object.
No SQLite extension API, SQLite wrapper, or SQLite fork hook is needed.

## Tests

Add MySQL-runtime-verified expectation coverage for:

- `SHOW COLUMNS FROM t WHERE Field = 'id'`;
- `SHOW FIELDS FROM t WHERE Type LIKE 'varchar%'`;
- `SHOW FULL COLUMNS FROM t WHERE Collation IS NOT NULL`;
- `SHOW FULL FIELDS FROM t WHERE Privileges LIKE '%update%' AND Comment = ''`;
- `Default <=> NULL`, `Default IS NULL`, `Default IS NOT NULL`, and `IN` /
  `NOT IN` behavior;
- `LIKE ... WHERE`, `ORDER BY`, and `LIMIT` syntax errors;
- unknown columns, `FULL`-only columns in non-`FULL` mode, qualified columns,
  non-string literals, `REGEXP`, `BETWEEN`, functions, and `XOR` diagnostics;
- result metadata, warning count, error count, and `ROW_COUNT()` behavior.

Extend plain C parser/runtime tests under `packages/libmylite/tests/` using the
existing `runtime_show_columns_introspection` and
`runtime_show_full_columns_introspection` binaries unless a separate binary
becomes clearer. Cover persistence, rename/drop behavior already present in the
base tests by verifying filtered metadata before and after the same descriptor
operations.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` to
state that `SHOW COLUMNS` / `SHOW FIELDS` / `SHOW FULL COLUMNS` /
`SHOW FULL FIELDS` support a limited trailing `WHERE` filter over displayed
output columns. Do not claim `EXTENDED`, views, privileges, general expressions,
warning-producing numeric coercions, `REGEXP`, `ORDER BY`, `LIMIT`, or full
`INFORMATION_SCHEMA.COLUMNS` parity.
