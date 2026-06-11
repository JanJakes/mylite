# Parser Corpus Expression And DDL Tail Residuals

This slice admits a small set of valid MySQL 8.4.9 syntax still present in the
mysql-server-tests parser corpus without expanding unsupported execution
semantics.

## Sources

- MySQL 8.4 Reference Manual, window function descriptions:
  <https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html>
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `LOCK TABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/lock-tables.html>
- Runtime evidence:
  `packages/libmylite/tests/mysql_parser_corpus_expression_ddl_tail_residuals_expectations.sh`
  against MySQL 8.4.9.

## Scope

Implemented with executable MyLite behavior:

- `NTH_VALUE(expr, n) FROM FIRST OVER (...)` is accepted as the explicit
  spelling of MySQL's default `NTH_VALUE` frame direction.
- Repeated column nullability attributes such as `NOT NULL NULL` are accepted,
  with the last nullability attribute determining descriptor metadata. This
  mirrors MySQL's behavior for the covered temporal DDL shape.

Implemented as unsupported utility placeholders:

- Standalone temporal interval arithmetic expressions whose interval value is a
  simple binary expression, such as `INTERVAL 1+1 SECOND`, continue to parse as
  unsupported utility statements. This matches the existing MyLite policy for
  general date arithmetic outside the currently executable `DATE_ADD()` /
  `DATE_SUB()` envelope.
- `CREATE TABLE ... START TRANSACTION` is accepted as a deterministic
  unsupported utility placeholder. MySQL documents this table option as
  internal binary-log machinery that restricts subsequent statements to
  `BINLOG`, `COMMIT`, and `ROLLBACK`. MyLite does not enter that internal
  replication state and must not create the table while silently ignoring the
  option.

Intentionally still rejected:

- Removed legacy `SHOW MASTER STATUS`, `SHOW SLAVE STATUS`, and
  `SHOW SLAVE HOSTS` forms remain syntax errors because MySQL 8.4.9 rejects
  them.
- `LOCK TABLES table LOW_PRIORITY WRITE` remains a syntax error because MySQL
  8.4.9 rejects that modifier in `LOCK TABLES`.
- `NTH_VALUE(... ) FROM LAST OVER (...)` remains outside this slice. MySQL
  parses `FROM LAST` but returns an execution error; MyLite should not accept it
  until the AST records the direction and runtime can return the matching
  diagnostic.

## MySQL 8.4.9 Runtime Observations

The expectation script verifies:

- `NTH_VALUE(id, 1) FROM FIRST OVER ()` succeeds and returns the same values as
  the default `NTH_VALUE(id, 1) OVER ()`;
- `NTH_VALUE(id, 1) FROM LAST OVER ()` parses but fails with MySQL's unsupported
  `FROM LAST` diagnostic;
- `NOT NULL NULL` makes a column nullable, and the parser-corpus temporal DDL
  succeeds;
- `CREATE TABLE ... START TRANSACTION` succeeds in MySQL and then enters the
  documented internal binlog state;
- the interval-bitwise corpus expressions are valid MySQL syntax;
- removed `SHOW MASTER` / `SHOW SLAVE` aliases and `LOCK TABLES ...
  LOW_PRIORITY WRITE` remain syntax errors.

## MyLite Grammar Snippets

```lemon
window_function_expression ::=
    NTH_VALUE LPAREN expression COMMA expression RPAREN window_from_first_opt
    window_null_treatment_opt over_clause.

window_from_first_opt ::= .
window_from_first_opt ::= FROM FIRST.
```

The repeated-nullability behavior uses the existing column-attribute grammar and
parser normalization rather than a separate special-case production:

```lemon
column_attribute_list ::= column_attribute_list column_attribute.
column_attribute ::= nullability.
nullability ::= NULL.
nullability ::= NOT NULL.
```

`CREATE TABLE ... START TRANSACTION` and general date arithmetic stay in the
post-parse placeholder classifier rather than executable Lemon grammar.

## Runtime Semantics

`FROM FIRST` is the MySQL default direction for `NTH_VALUE`, so it is represented
as the existing `NTH_VALUE` AST and runtime behavior. `FROM LAST` is not folded
to the default because that would make MyLite return successful results where
MySQL returns an error.

For repeated nullability, planning chooses the final nullability attribute in
the column definition. Other duplicated column attributes continue to use the
existing duplicate-attribute diagnostics.

Unsupported interval arithmetic and `CREATE TABLE ... START TRANSACTION`
placeholders execute through the existing unsupported utility path. They do not
mutate catalogs or rows.

## Tests

- Parser tests cover `NTH_VALUE ... FROM FIRST`, repeated nullability, interval
  binary-expression placeholders, `CREATE TABLE ... START TRANSACTION`
  placeholders, and the intentionally rejected removed/unsupported forms.
- Runtime tests cover `NTH_VALUE ... FROM FIRST`, repeated-nullability metadata,
  and no-mutation unsupported diagnostics for `CREATE TABLE ... START
  TRANSACTION`.
- The MySQL expectation script records the target-runtime behavior used by the
  parser and runtime tests.

## Compatibility Status

This improves valid MySQL 8.4-compatible parser and descriptor behavior for a
small set of residual corpus rows. It does not add general date arithmetic,
internal binary-log create-table state, removed legacy replication aliases, or
`NTH_VALUE FROM LAST` execution diagnostics.
