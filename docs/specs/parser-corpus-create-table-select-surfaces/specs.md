# Parser Corpus CREATE TABLE SELECT Surfaces

This slice reduces high-volume parser-corpus failures around MySQL
`CREATE TABLE ... SELECT` forms that are syntactically valid in MySQL 8.4.9
but outside MyLite's current descriptor-inference envelope.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-table-select.html
- https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- https://dev.mysql.com/doc/refman/8.4/en/union.html
- https://dev.mysql.com/doc/refman/8.4/en/with.html
- https://dev.mysql.com/doc/refman/8.4/en/parenthesized-query-expressions.html

Runtime probes are verified against the local MySQL 8.4.9 container
`mylite-mysql-849` with `mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw`.

## Scope

MyLite already supports a limited descriptor-backed
`CREATE TABLE [IF NOT EXISTS] target [AS] SELECT ... FROM source` form for
single-table source selects. This slice admits additional MySQL-shaped CTAS
syntax as explicit unsupported placeholders:

- explicit destination column, key, and constraint definitions before `SELECT`;
- table options between the target definition and `SELECT`;
- `CREATE TABLE ... SELECT ... UNION ...` and related set-operation sources;
- parenthesized query-expression sources after `AS` or in MySQL's
  parenthesized CTAS shorthand;
- `WITH` / recursive-CTE sources;
- `TABLE` and `VALUES` query-expression sources;
- partitioned `CREATE TABLE ... PARTITION BY ... AS SELECT ...` forms.

These forms return deterministic unsupported diagnostics at runtime. They must
not create, alter, or populate the target table until MyLite implements the
corresponding descriptor inference and row-copy semantics.

## Non-Goals

This slice does not implement:

- descriptor inference from explicit destination definitions plus selected
  source expressions;
- expression, literal, aggregate, joined, CTE, or compound-source row copying;
- CTAS key/index/constraint creation;
- CTAS partition descriptors or partitioned storage;
- `TABLE` / `VALUES` CTAS source execution;
- temporary-table CTAS behavior beyond parser-level unsupported diagnostics for
  newly admitted shapes.

Existing supported CTAS syntax and runtime behavior must continue to execute
unchanged.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned syntax surface and do not
copy MySQL grammar. The implementation uses a post-parse fallback rather than
main Lemon productions so the supported CTAS executor is not exposed to shapes
it cannot execute correctly.

```lemon
create_table_select_placeholder_statement ::=
    CREATE TEMPORARY? TABLE create_if_not_exists_opt table_name
    create_table_definition_or_options_opt create_table_query_source.

create_table_query_source ::= SELECT select_tail.
create_table_query_source ::= TABLE table_name table_tail_opt.
create_table_query_source ::= VALUES row_value_list values_tail_opt.
create_table_query_source ::= WITH common_table_expression_list SELECT select_tail.
create_table_query_source ::= WITH common_table_expression_list TABLE table_name table_tail_opt.
create_table_query_source ::= WITH common_table_expression_list VALUES row_value_list values_tail_opt.
create_table_query_source ::= AS SELECT select_tail.
create_table_query_source ::= AS TABLE table_name table_tail_opt.
create_table_query_source ::= AS VALUES row_value_list values_tail_opt.
create_table_query_source ::= AS parenthesized_query_expression set_tail_opt.
create_table_query_source ::= parenthesized_query_expression set_tail_opt.

set_tail_opt ::= .
set_tail_opt ::= UNION query_expression_body set_tail_opt.
set_tail_opt ::= INTERSECT query_expression_body set_tail_opt.
set_tail_opt ::= EXCEPT query_expression_body set_tail_opt.
```

## Runtime Behavior

No SQLite fork hook is needed. This is MyLite parser and diagnostic routing:

- the normal parser/executor remains authoritative for already-supported CTAS;
- unsupported CTAS variants parse to
  `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT`;
- runtime reports `1064 / 42000` with a deterministic "not supported"
  diagnostic;
- no catalog descriptor, physical table, row copy, warning, or transaction
  boundary is produced by the unsupported placeholder.

## Tests

MySQL 8.4.9 expectation tests cover accepted syntax and representative
side effects for explicit-definition CTAS, compound CTAS, parenthesized CTAS,
CTE CTAS, `TABLE`/`VALUES` CTAS, and partitioned CTAS.

MyLite parser tests cover placeholder classification for the same syntax
families and preservation of the existing supported CTAS AST. Runtime tests
verify unsupported diagnostics and confirm the target table is not created.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

Observed after implementation:

```text
parse.csv.mysql_server_tests,parse,1,69595,69595,65736,3859,0,5544381,2365.405,33.988,29422.023
```

## Compatibility Status

This slice moves selected CTAS syntax from syntax errors to parser-admitted
unsupported diagnostics. It does not mark broad CTAS descriptor inference or
execution as supported.
