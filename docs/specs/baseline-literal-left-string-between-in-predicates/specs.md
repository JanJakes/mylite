# Baseline Literal-Left String BETWEEN And IN Predicates

This slice implements MySQL-compatible table-backed predicates where a string
literal appears on the left side of `BETWEEN` or `IN` and one or more right-side
operands are descriptor columns. It follows the adjacent literal-left string
comparison slice and removes the remaining literal-left predicate residuals
from the parser corpus.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html

Runtime probes are captured in:

- `packages/libmylite/tests/mysql_parser_corpus_query_expression_clause_surfaces_expectations.sh`

## MySQL 8.4.9 Runtime Observations

Observed against MySQL 8.4.9:

```sql
CREATE TABLE t_dates (
  f1 DATE,
  f2 DATETIME,
  f3 DATE,
  a DATETIME,
  value DECIMAL(30,0)
);
CREATE TABLE t2 (a INT);
INSERT INTO t_dates VALUES
  ('2001-01-01','2001-04-10 12:34:56','2001-05-01',
   '2010-02-01 09:31:02',100000000000000000000002),
  ('2001-01-01','2001-03-01 00:00:00','2001-03-20',
   '2010-02-02 00:00:00',5);
INSERT INTO t2 VALUES (1),(3);

SELECT f2 FROM t_dates
WHERE '2001-04-10 12:34:56' BETWEEN f2 AND '01-05-01';

SELECT f2, f3 FROM t_dates
WHERE '01-03-10' BETWEEN f2 AND f3;

SELECT COUNT(*) FROM t_dates, t2
WHERE '01-01-01' IN (f1, '01-02-03');
```

MySQL returns the first and second `f2` values for the first query, the second
row for the second query, and `4` for the `IN` count.

The string literal on the left is compared using the descriptor context supplied
by the first descriptor operand in the bounds or list. Two-digit date strings in
this documented predicate subset use MySQL's observed two-digit year mapping:
`00..69` map to `2000..2069`, and `70..99` map to `1970..1999`.

## Scope

In scope:

- `STRING [NOT] BETWEEN predicate_range_value AND predicate_range_value` in
  table-backed predicate clauses when a bound descriptor column supplies a
  `DATE`, `DATETIME`, or `TIMESTAMP` context;
- `STRING [NOT] IN (predicate_in_value_list)` when a list item descriptor column
  supplies a `DATE`, `DATETIME`, or `TIMESTAMP` context;
- descriptor and literal bound/list combinations covered by the MySQL probes;
- two-digit date-only predicate literals in the descriptor-normalized
  literal-left path;
- existing single-table and joined `SELECT` predicate envelopes where row-scalar
  predicate planning already applies.

Out of scope:

- broad expression-left `BETWEEN` / `IN` planning;
- literal-left lists without a descriptor operand to define conversion context;
- non-temporal descriptor contexts beyond behavior already supported by the
  row-scalar predicate planners;
- non-date two-digit temporal forms, fractional seconds, time-only values, or
  loaded time-zone conversion;
- `UPDATE` / `DELETE` joined semantics beyond already supported predicate
  envelopes.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape.

```lemon
literal_left_predicate_value ::= STRING.

predicate_atom ::=
    literal_left_predicate_value BETWEEN predicate_range_value AND predicate_range_value.
predicate_atom ::=
    literal_left_predicate_value NOT BETWEEN predicate_range_value AND predicate_range_value.
predicate_atom ::=
    literal_left_predicate_value IN LPAREN predicate_in_value_list RPAREN.
predicate_atom ::=
    literal_left_predicate_value NOT IN LPAREN predicate_in_value_list RPAREN.
```

The parser builds the same `BETWEEN` and `IN` AST shapes used by existing
predicate forms. The runtime planner recognizes the left operand as a
literal-left predicate value, finds a descriptor operand in the right-side
bounds/list, normalizes the string literal and literal peers with that descriptor
context, and lowers the predicate to row-scalar SQL executed by SQLite.

## Storage, SQLite, And Performance

No SQLite fork hook is needed. MyLite performs compile-time predicate
normalization and still lets SQLite evaluate the row filter. MyLite does not
load table rows into memory or post-filter rows for this slice.

## Tests

Tests cover:

- MySQL 8.4.9 expectation probes for the three parser-corpus residuals;
- parser admission for literal-left `BETWEEN`, `NOT BETWEEN`, `IN`, and
  `NOT IN`;
- runtime row counts and values for descriptor-backed temporal `BETWEEN` and
  joined `IN` cases;
- malformed literal-left tails remaining syntax errors.

## Compatibility Status

This slice moves the targeted literal-left string `BETWEEN` and `IN` predicates
from placeholder diagnostics to executable row-scalar predicates. General
expression-left predicate execution remains tracked separately.
