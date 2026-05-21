# Baseline SHOW TABLES WHERE

## Status

This feature specifies the next metadata filtering slice for MyLite's
descriptor-owned `SHOW TABLES` and `SHOW FULL TABLES` statements. It adds a
trailing `WHERE` predicate over the statement's displayed output columns while
preserving the existing `LIKE` behavior and schema-resolution policy.

This is not a general SHOW expression engine. It admits only the same
output-column predicate family already used by nearby SHOW metadata statements:
comparisons, `LIKE`, `REGEXP`/`RLIKE`, `IN`, `IS NULL`, `IS NOT NULL`, `NOT`,
`AND`, and `OR` over supported string literals and `NULL` values.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Existing SHOW LIKE filter coverage:
  `docs/specs/baseline-show-like-filters/specs.md`
- Existing SHOW metadata predicates in runtime tests and execution code
- MySQL 8.4 Reference Manual, `SHOW TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-tables.html
- MySQL 8.4 Reference Manual, extended `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes were run against MySQL 8.4.9 with:

```sh
MYLITE_MYSQL_BIN=/opt/homebrew/opt/mysql@8.4/bin/mysql
MYLITE_MYSQL_SOCKET=/tmp/mylite-mysql-849.jsgoZE/mysql.sock
```

Observed behavior that shapes this slice:

- `SHOW TABLES WHERE Tables_in_db LIKE 'a%'` is accepted and returns a
  one-column result with the same `Tables_in_db` column label as unfiltered
  `SHOW TABLES`.
- `SHOW FULL TABLES WHERE Table_type = 'BASE TABLE'` is accepted and returns
  `Tables_in_db` plus `Table_type`.
- The displayed table-name column is resolved case-insensitively in predicates:
  `tables_in_db = 'alpha'` is accepted.
- Table-name comparisons use the current MySQL table-name comparison behavior
  for the observed runtime. For the baseline descriptor slice, MyLite keeps its
  existing ASCII case-insensitive SHOW predicate comparisons.
- `SHOW TABLES WHERE Table_type = 'BASE TABLE'` without `FULL` fails with
  `1054 / 42S22` unknown column because `Table_type` is not an output column.
- `SHOW TABLES LIKE 'a%' WHERE ...` is a syntax error. This slice keeps `LIKE`
  and `WHERE` mutually exclusive in the grammar.
- `SHOW TABLES WHERE ... ORDER BY 1` and `SHOW TABLES WHERE ... LIMIT 1` are
  syntax errors.
- Missing default schema still fails with `1046 / 3D000`; unknown explicit
  schema still fails with `1049 / 42000`; reserved `_mylite_*` schema names
  keep MyLite's existing reserved-name diagnostic.
- The statement does not report warnings for supported predicates, and
  `ROW_COUNT()` remains `-1` after successful SHOW statements.

The implementation must update
`packages/libmylite/tests/mysql_baseline_show_like_filters_expectations.sh` so
these expectations are recorded against MySQL 8.4.9.

## Scope

The implementation must add:

- parser support for `SHOW [FULL] TABLES [FROM|IN schema_name] WHERE predicate`;
- output-column filtering for the existing persistent base-table list;
- column resolution for `Tables_in_<schema_name>` in both ordinary and `FULL`
  modes;
- column resolution for `Table_type` only in `SHOW FULL TABLES`;
- supported predicate evaluation for `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`,
  `>=`, `LIKE`, `REGEXP`, `RLIKE`, `IN`, `IS NULL`, `IS NOT NULL`, `NOT`,
  `AND`, and `OR`;
- deterministic diagnostics for unknown predicate columns, unsupported
  predicate shapes, unsupported right-hand values, unsupported pattern strings,
  missing default schema, unknown schema, and reserved schema names;
- tests for ordinary and `FULL` output, schema-qualified filtering, unknown
  columns, missing/unknown/reserved schemas, persistence after rename/drop, and
  no row-result side effects beyond existing SHOW result conventions.

The implementation must not add:

- `LIKE` and `WHERE` on the same `SHOW TABLES` statement;
- `ORDER BY`, `LIMIT`, `GROUP BY`, `HAVING`, `PROCEDURE`, `INTO`, subqueries,
  functions, arithmetic, scalar expression projection, aliases, table-qualified
  predicate columns, or arbitrary expression evaluation in `SHOW TABLES WHERE`;
- view rows, temporary table rows, system schemas, privilege filtering, or a
  complete MySQL data dictionary;
- SQLite fork patches or new storage descriptors.

## Syntax

The independently authored MyLite Lemon grammar shape is:

```lemon
show_tables_filter_opt(A) ::= . {
    A = NULL;
}
show_tables_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_tables_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

show_tables_statement(A) ::= SHOW(S) show_full_opt(F) TABLES(T)
    show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, NULL, L);
}
show_tables_statement(A) ::= SHOW(S) show_full_opt(F) TABLES(T)
    FROM identifier(D) show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, D, L);
}
show_tables_statement(A) ::= SHOW(S) show_full_opt(F) TABLES(T)
    IN identifier(D) show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, D, L);
}
```

The AST continues to store the optional schema node as the first child and the
optional filter node as the second child. The filter child is either a string
literal for `LIKE`, a `WHERE` clause node for this feature, or absent.

## Semantics

Schema resolution is unchanged:

- without `FROM` or `IN`, the selected/default schema is required;
- with `FROM` or `IN`, the explicit schema is resolved before any predicate
  evaluation;
- reserved `_mylite_*` schema names are rejected before catalog iteration;
- unknown explicit schemas return the existing unknown-database diagnostic.

The output row set is the same descriptor-owned persistent base-table list used
by current `SHOW TABLES`. Session temporary tables, views, system views, and
unsupported object kinds remain outside this slice.

Predicate columns are the displayed output columns:

- `Tables_in_<schema_name>` is available for both ordinary and `FULL` forms;
- `Table_type` is available only when `FULL` is present and has the value
  `BASE TABLE` for all currently emitted rows;
- predicate column names are resolved ASCII case-insensitively;
- qualified predicate column references are rejected as unsupported or unknown
  deterministic errors.

Predicate values are string or `NULL` literals. `LIKE` patterns use the existing
SHOW LIKE pattern matcher and reject decoded NUL bytes. `REGEXP` and `RLIKE`
patterns use MyLite's baseline ASCII regular-expression engine. `IN` lists are
limited to string and `NULL` literals. `IS NULL` and `IS NOT NULL` follow the
current three-valued metadata predicate model.

MyLite intentionally keeps table-name predicate comparison ASCII
case-insensitive for this slice, matching the existing SHOW metadata predicate
surface and the observed MySQL 8.4.9 runtime on the local development platform.
This does not claim complete cross-platform MySQL lower-case-table-name
semantics.

## Ownership Boundaries

- Public API: no ABI or API changes. Successful statements return a row result
  through existing SHOW result conventions.
- Statement context: successful SHOW statements keep `warning_count == 0` and
  `ROW_COUNT()` semantics unchanged.
- Parser/AST: admits the `WHERE` syntax and stores the predicate tree; it does
  not evaluate compatibility.
- Runtime/analyzer: resolves schemas, validates output-column references,
  evaluates the supported predicate subset, and appends matching result rows.
- Catalog: remains the authority for table descriptors and object kinds.
- Result builder: keeps existing `SHOW TABLES` / `SHOW FULL TABLES` column
  labels and row shape.
- Storage/VFS and SQLite physical storage: unchanged. This is MyLite
  descriptor/result filtering over catalog rows and uses no SQLite fork hook.

## Diagnostics

Supported filters produce no warnings. Unsupported or invalid behavior returns
existing deterministic diagnostics:

- missing default schema: existing no-database-selected diagnostic;
- unknown explicit schema: existing unknown-database diagnostic;
- reserved `_mylite_*` schema: existing reserved-name diagnostic;
- unknown predicate output column: `1054 / 42S22` unknown-column diagnostic;
- `Table_type` in non-`FULL` `SHOW TABLES`: unknown-column diagnostic;
- non-output-column predicates, functions, arithmetic, scalar literals as
  standalone predicates, subqueries, parameters, and qualified columns:
  unsupported output-column-predicate diagnostic;
- non-string and non-`NULL` predicate literal values: deterministic syntax or
  unsupported string/`NULL` diagnostic, depending on whether the shared parser
  admits the literal shape before runtime evaluation;
- decoded NUL bytes in predicate strings or patterns: existing SHOW metadata
  string diagnostic;
- unsupported regular-expression pattern features: existing MyLite REGEXP
  diagnostic.

## Performance And SQLite Fit

The implementation filters catalog descriptors while iterating the schema's
table descriptors. It does not materialize user table rows, does not inspect
SQLite schema text, and does not generate SQLite SQL. The cost is linear in the
number of descriptors already visited by `SHOW TABLES`; predicate evaluation
uses small stack vectors like existing SHOW metadata filters.

## Tests

Fast C runtime/parser tests must cover:

- parsing `SHOW TABLES WHERE ...`, `SHOW FULL TABLES WHERE ...`, and explicit
  `FROM` / `IN` schema forms;
- successful filters by `LIKE`, equality, `IN`, `REGEXP`/`RLIKE`, `IS NULL`,
  `IS NOT NULL`, `NOT`, `AND`, and `OR`;
- `SHOW FULL TABLES WHERE Table_type = 'BASE TABLE'`;
- `Table_type` rejected for non-`FULL` `SHOW TABLES`;
- unknown predicate column diagnostics;
- missing default schema, unknown schema, and reserved schema names;
- no rows after filters that do not match;
- rename/drop/reopen behavior continuing to reflect descriptor state;
- rejection of `LIKE ... WHERE`, `ORDER BY`, `LIMIT`, functions, arithmetic,
  table-qualified predicate columns, and non-string predicate values.

MySQL expectation coverage must be added to the existing SHOW LIKE expectation
script or a new feature script. The expectation artifact must record accepted
ordinary/FULL WHERE filters, output headers, diagnostics for unknown columns,
missing/unknown schemas, `LIKE ... WHERE`, `ORDER BY`, and `LIMIT`, plus
warning count and row-count state for successful statements.
