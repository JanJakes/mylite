# EXPLAIN Statement Parser Placeholders

## Scope

This slice keeps the existing table-description implementation for:

- `DESCRIBE tbl_name [col_name | wild]`
- `DESC tbl_name [col_name | wild]`
- `EXPLAIN tbl_name [col_name | wild]`

It adds parser-placeholder support for MySQL 8.4 `EXPLAIN`, `DESCRIBE`, and
`DESC` execution-plan forms. These forms prepare successfully and execute as
no-op custom statements with warning `1235` (`ER_NOT_SUPPORTED_YET`). They do
not produce query-plan rows, run `EXPLAIN ANALYZE`, inspect another connection,
or mutate session state.

## Sources

- MySQL 8.4 Reference Manual, `EXPLAIN` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/explain.html
- Runtime probes against local `mysql:8.4.9` container
  `mylite-mysql-849`.

## MySQL Runtime Observations

Representative MySQL 8.4.9 probes accepted the following syntax:

- `EXPLAIN SELECT * FROM explain_t WHERE id = 1`
- `EXPLAIN FORMAT=TREE SELECT * FROM explain_t WHERE id = 1`
- `EXPLAIN ANALYZE FORMAT=TREE SELECT * FROM explain_t WHERE id = 1`
- `DESCRIBE SELECT 1`
- `DESC SELECT 1`
- `EXPLAIN FOR DATABASE mylite_placeholder_probe SELECT * FROM explain_t`

`EXPLAIN FOR CONNECTION CONNECTION_ID()` is a syntax error. MySQL accepts a
numeric connection id, then reports `1094 Unknown thread id` when the id is not
known.

## Syntax

The placeholder grammar recognizes these statement forms:

```lemon
explain_statement ::= explain_keyword explain_tail.

explain_keyword ::= EXPLAIN.
explain_keyword ::= DESCRIBE.
explain_keyword ::= DESC.

explain_tail ::= opt_explain_type opt_explain_into opt_explain_schema
        explainable_statement.
explain_tail ::= opt_explain_type opt_explain_into FOR CONNECTION INTEGER.
explain_tail ::= ANALYZE opt_explain_analyze_format opt_explain_schema
        select_statement.

opt_explain_type ::= .
opt_explain_type ::= FORMAT opt_equal identifier.

opt_explain_into ::= .
opt_explain_into ::= INTO USER_VARIABLE.

opt_explain_schema ::= .
opt_explain_schema ::= FOR SCHEMA identifier.
opt_explain_schema ::= FOR DATABASE identifier.

explainable_statement ::= select_statement.
explainable_statement ::= union_query_expression.
explainable_statement ::= table_query_statement.
explainable_statement ::= insert_values_statement.
explainable_statement ::= insert_set_statement.
explainable_statement ::= replace_values_statement.
explainable_statement ::= replace_set_statement.
explainable_statement ::= update_statement.
explainable_statement ::= delete_statement.

table_query_statement ::= TABLE table_name opt_order_by_clause opt_limit_clause.
```

The table-description grammar remains separate so `EXPLAIN t` and
`DESCRIBE t` still execute the implemented metadata path instead of becoming
placeholders.

## Runtime Semantics

The AST node is `MYLITE_SQL_AST_PLACEHOLDER_STATEMENT` with placeholder kind
`MYLITE_SQL_AST_PLACEHOLDER_EXPLAIN`. Preparing maps it to a custom statement.
Executing appends one warning with code `1235` and message text containing
`EXPLAIN statement`, returns `MYLITE_DONE`, reports zero affected rows, and
returns no result columns.

## Tests

Parser coverage must include:

- statement-form `EXPLAIN SELECT`, `DESCRIBE SELECT`, and `DESC SELECT`
- `EXPLAIN FORMAT=TREE SELECT`
- `EXPLAIN FORMAT=JSON INTO @plan SELECT`
- `EXPLAIN ANALYZE FORMAT=TREE SELECT`
- `EXPLAIN FOR DATABASE app SELECT`
- `EXPLAIN FOR CONNECTION 123`
- table-form `EXPLAIN t` remains `MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT`

Runtime coverage must assert placeholder execution warning `1235` and no result
columns for representative statement forms.
