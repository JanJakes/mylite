# Functional index key-part parser acceptance

## Scope

This slice accepts MySQL functional index key-part syntax in MyLite's parser and
AST, while keeping functional-index storage and enforcement explicitly
unsupported.

In scope:

- standalone `CREATE INDEX idx ON t ((expr))`
- table-level `CREATE TABLE t (..., INDEX idx ((expr)))`
- shared `ALTER TABLE t ADD INDEX idx ((expr))` key-part grammar
- optional `ASC` and `DESC` ordering on functional key parts
- mixed functional and identifier key-part lists
- deterministic unsupported execution with no catalog or table mutation

Out of scope:

- hidden generated columns for functional key parts
- optimizer, storage, duplicate-key, or expression-matching behavior
- full generated-column validation for functional expressions
- multi-valued `CAST(... AS ... ARRAY)` indexes
- `SHOW CREATE TABLE`, `SHOW INDEX`, and information-schema expression metadata

## Sources

- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html

This specification is independently authored from official documentation and
does not copy MySQL grammar, documentation prose, or implementation sources.

## MySQL 8.4 behavior summary

MySQL key parts can be column names with optional prefix lengths or expressions
enclosed in an extra parenthesized key-part wrapper. Functional key parts can
appear in `CREATE TABLE`, `ALTER TABLE ADD INDEX`, and standalone
`CREATE INDEX`, can be mixed with ordinary key parts, and can carry `ASC` or
`DESC` order. MySQL implements them through hidden virtual generated columns.

Functional key parts have important restrictions: a primary key cannot include
them, `FULLTEXT` and `SPATIAL` indexes cannot include them, expression-only
column names should be written as ordinary key parts, and generated-column
expression restrictions apply.

## MyLite behavior

MyLite accepts functional key parts in the parser and records them as
`MYLITE_SQL_AST_KEY_PART` nodes with `key_part_kind =
MYLITE_SQL_AST_KEY_PART_FUNCTIONAL`. The key-part child is the expression AST,
not an identifier node. Identifier key parts keep
`MYLITE_SQL_AST_KEY_PART_IDENTIFIER`.

Execution remains unsupported until generated-column and functional-index
storage exists. DDL validation returns `MYLITE_UNSUPPORTED` with the diagnostic
`Unsupported functional index key parts` before metadata/catalog writes or
physical table creation:

- `CREATE TABLE t (c VARCHAR(20), INDEX idx ((LOWER(c))))` creates no table.
- `CREATE INDEX idx ON t ((LOWER(c)))` creates no index metadata.
- `ALTER TABLE t ADD INDEX idx ((LOWER(c)))` leaves the table unchanged.

For standalone index DDL, the unsupported diagnostic is produced after the same
implicit-commit boundary and target-table resolution path used by other
standalone index validation failures.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this slice:

```lemon
key_part ::= identifier opt_key_part_prefix opt_key_part_order.
key_part ::= LPAREN expression RPAREN opt_key_part_order.

opt_key_part_prefix ::= .
opt_key_part_prefix ::= LPAREN INTEGER RPAREN.

opt_key_part_order ::= .
opt_key_part_order ::= ASC.
opt_key_part_order ::= DESC.
```

## Test expectations

Parser tests cover standalone, table-level secondary, table-level unique, and
ALTER TABLE functional key parts, including mixed identifier/functional lists
and `DESC` ordering.

Runtime tests cover:

- `CREATE TABLE` functional index rejection with no table or statistics rows
- standalone `CREATE INDEX` functional key-part rejection with no index row
- `ALTER TABLE ADD INDEX` functional key-part rejection with no index row
