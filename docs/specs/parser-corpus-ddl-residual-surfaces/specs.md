# Parser Corpus DDL Residual Surfaces

This slice reduces remaining parser-corpus DDL rejects by implementing small
MySQL 8.4.9 grammar aliases that map onto existing MyLite type semantics and by
classifying selected valid-but-unsupported DDL forms as unsupported utility
placeholders after normal parsing fails.

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, numeric types:
  <https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html>
- MySQL 8.4 Reference Manual, string types:
  <https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html>
- MySQL 8.4 Reference Manual, generated columns:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table-generated-columns.html>
- Runtime evidence:
  `packages/libmylite/tests/mysql_parser_corpus_ddl_residual_surfaces_expectations.sh`
  against MySQL 8.4.9.

## Scope

Implemented as real grammar/runtime-compatible aliases:

- `DOUBLE PRECISION(M,D)` maps to MyLite's existing `DOUBLE(M,D)` descriptor.
- `REAL(M,D)` maps to MyLite's existing `REAL`/double descriptor with scale.
- explicit `SIGNED` on approximate numeric types is accepted and normalized as
  the signed default.
- `YEAR UNSIGNED` and `YEAR(4) UNSIGNED` are accepted and normalized as `YEAR`.
- `VARCHAR(n) BYTE` maps to `VARBINARY(n)`.
- `LONG BYTE` maps to MySQL's `MEDIUMBLOB` alias behavior.
- legacy index `TYPE name` clauses are accepted wherever MyLite already accepts
  index type options.
- `ALTER TABLE ... CHARACTER SET binary` and
  `ALTER TABLE ... CONVERT TO CHARACTER SET DEFAULT COLLATE name` parse through
  the existing ALTER-table charset/collation path.
- generated-column table definitions with complete
  `GENERATED ALWAYS AS (expr)` clauses followed by `VIRTUAL NOT NULL` and table
  options parse and execute through MyLite's existing generated-column support.

Implemented as unsupported utility placeholders after normal parse failure:

- `FULLTEXT ... WITH PARSER parser_name` in `CREATE TABLE`, `ALTER TABLE`, or
  `CREATE FULLTEXT INDEX` DDL.
- foreign-key actions using `ON DELETE SET DEFAULT` or
  `ON UPDATE SET DEFAULT`.

Out of scope:

- installing or resolving fulltext parser plugins;
- partition DDL execution;
- executing generated-column forms beyond MyLite's existing generated-column
  support;
- broad quoted-identifier mode changes such as `ANSI_QUOTES`;
- accepting malformed generated expressions, incomplete `WITH PARSER` clauses,
  incomplete foreign-key actions, or incomplete DDL tails.

## MySQL 8.4.9 Runtime Observations

The expectation script verifies that MySQL accepts the real alias forms and
normalizes their metadata. It also verifies that `FULLTEXT ... WITH PARSER`
reaches a plugin-resolution diagnostic rather than a syntax error when the
named parser plugin is not installed, and that generated columns with
`VIRTUAL NOT NULL` plus table options are accepted.

## MyLite Grammar Snippets

These snippets describe the MyLite-owned grammar surface for this slice.

```lemon
approximate_type ::= DOUBLE PRECISION LPAREN integer COMMA integer RPAREN signedness_opt.
approximate_type ::= REAL LPAREN integer COMMA integer RPAREN signedness_opt.
signedness_opt ::= SIGNED.

year_type ::= YEAR UNSIGNED.
year_type ::= YEAR LPAREN integer RPAREN UNSIGNED.

binary_string_type ::= VARCHAR LPAREN integer RPAREN BYTE.
binary_string_type ::= LONG BYTE.

index_type_option ::= TYPE identifier.

alter_table_default_charset_collation_option ::= CHARACTER SET BINARY.
alter_table_convert_character_set_statement ::=
    ALTER TABLE table_name CONVERT TO CHARACTER SET DEFAULT collate_opt.

generated_column_attribute ::=
    GENERATED ALWAYS AS (expression) VIRTUAL NOT NULL.
```

Unsupported DDL residual placeholders are deliberately not broad grammar
support. They are a post-failure classifier over complete token streams.

## Runtime Semantics

Real alias grammar follows existing runtime semantics for the normalized target
types, ALTER paths, and generated-column paths. Unsupported residual DDL forms
return `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT`; execution returns the
existing `1064 / 42000` unsupported utility diagnostic with no result rows and
no schema mutation.

## Tests

- `mysql_parser_corpus_ddl_residual_surfaces_expectations.sh` records
  representative MySQL 8.4.9 behavior.
- `parser_corpus_ddl_residual_surfaces_test.c` verifies parser AST
  classification and syntax-error preservation.
- `runtime_parser_corpus_ddl_residual_surfaces_test.c` verifies unsupported
  runtime diagnostics and representative executable alias metadata.

The parser corpus benchmark over
`build/perf-data/mysql-server-tests-queries.csv` was rerun after the slice:
69,432 of 69,595 statements parsed, leaving 163 residual parse failures.

## Compatibility Status

This slice improves parser and DDL alias compatibility. It does not mark
fulltext parser plugins, generated-column edge forms outside current execution
support, partition DDL, or foreign-key `SET DEFAULT` actions as supported.
