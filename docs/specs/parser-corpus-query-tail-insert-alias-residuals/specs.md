# Parser Corpus Query Tail And Insert Alias Residuals

This slice reduces MySQL server-test parser-corpus residuals around two small
surfaces that are valid in MySQL 8.4.9 but not executable in MyLite's current
query and DML envelopes:

- final query-expression tails on compound `SELECT` forms, including
  compound-level `ORDER BY`, `LIMIT`, `INTO`, and locking-clause permutations;
- `INSERT ... VALUES ... AS alias` row aliases and duplicate-update references
  to row/source aliases.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/union.html
- https://dev.mysql.com/doc/refman/8.4/en/insert.html

Runtime probes are captured in:

- `packages/libmylite/tests/mysql_parser_corpus_query_expression_surfaces_expectations.sh`
- `packages/libmylite/tests/mysql_parser_corpus_dml_variant_surfaces_expectations.sh`

## MySQL 8.4.9 Observations

Observed accepted query forms include:

```sql
SELECT 1 UNION SELECT 1 LIMIT 0;
SELECT id FROM t1 UNION ALL SELECT 99 ORDER BY 1;
SELECT 1 FROM DUAL LIMIT 1 INTO @var FOR UPDATE;
SELECT 1 FROM DUAL LIMIT 1 FOR UPDATE INTO @var;
SELECT 1 UNION SELECT 1 FOR UPDATE INTO @var;
SELECT 1 UNION SELECT 1 FROM DUAL INTO @var FOR UPDATE;
```

Observed accepted INSERT forms include:

```sql
INSERT INTO empty_ok VALUES() AS row_alias
  ON DUPLICATE KEY UPDATE f1 = 1;
INSERT INTO t VALUES (4, 40, 400, '2000-01-04', '2000-01-04') AS n;
INSERT INTO t VALUES (1, 77, 700, '2000-01-01', '2000-01-01') AS n
  ON DUPLICATE KEY UPDATE v = n.v;
INSERT INTO t SELECT * FROM t AS source
  ON DUPLICATE KEY UPDATE t.v = source.v;
```

## Scope

MyLite admits these surfaces through the existing post-failure parser
placeholder classifier. The normal Lemon grammar remains unchanged in this
slice because the general query-expression tail grammar interacts with the
existing `SELECT` order/limit/locking grammar and should be handled in a
dedicated grammar refactor.

In scope:

- parser acceptance for compound query final `ORDER BY`, `LIMIT`, `INTO`, and
  locking tails when the normal parser rejects the statement;
- parser acceptance for `SELECT ... LIMIT ... INTO ... FOR UPDATE` and
  `SELECT ... LIMIT ... FOR UPDATE INTO ...` permutations when the normal
  parser rejects the statement;
- parser acceptance for `INSERT ... VALUES|VALUE ... [AS] alias` with optional
  alias column lists;
- parser acceptance for duplicate-update assignments that reference qualified
  row/source aliases;
- runtime rejection through the existing unsupported-utility diagnostic.

Out of scope:

- executable compound-level ordering, limiting, locking, or `SELECT ... INTO`
  assignment for compound queries;
- executing row aliases or row-alias column lists in `INSERT ... VALUES`;
- executing `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE` source-alias
  references outside the existing supported duplicate-key tail;
- broader query-expression grammar restructuring, type aggregation, optimizer
  behavior, triggers, privileges, or SQLite fork changes.

## MyLite Grammar Direction

The intended future grammar direction is:

```lemon
query_expression ::=
    query_expression_body set_operation_list query_expression_tail_opt.

query_expression_tail_opt ::= .
query_expression_tail_opt ::= order_clause limit_clause_opt into_clause_opt locking_clause_opt.
query_expression_tail_opt ::= limit_clause into_clause_opt locking_clause_opt.
query_expression_tail_opt ::= limit_clause locking_clause into_clause_opt.

insert_values_source ::=
    VALUES insert_row_constructor_list insert_row_alias_opt.

insert_row_alias_opt ::= .
insert_row_alias_opt ::= AS identifier.
insert_row_alias_opt ::= AS identifier LPAREN identifier_list RPAREN.
insert_row_alias_opt ::= identifier.
insert_row_alias_opt ::= identifier LPAREN identifier_list RPAREN.

duplicate_update_value ::= qualified_identifier.
```

This slice implements the same compatibility decision as a post-failure
placeholder, not as executable grammar.

## Runtime Behavior

Accepted residuals become `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT` and
execute with MyLite's deterministic unsupported-utility diagnostic. Malformed
tails, such as a missing `LIMIT` value or a missing row alias after `AS`, remain
syntax errors.

## Tests

Tests cover:

- MySQL 8.4.9 expectation probes for representative accepted query-tail and
  insert-alias statements;
- MyLite parser acceptance as unsupported placeholders;
- malformed-tail regression tests;
- runtime unsupported diagnostics for accepted placeholders;
- parser-corpus benchmark movement over
  `build/perf-data/mysql-server-tests-queries.csv`.

## Compatibility Status

This slice moves the targeted residuals from syntax errors to explicit
placeholder diagnostics. It does not mark compound query final-tail execution
or INSERT row-alias execution as supported.
