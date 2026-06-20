# Baseline Literal-Left String Comparison Predicates

This slice implements MySQL-compatible `WHERE` comparison predicates where a
string literal appears on the left side and a descriptor column appears on the
right side. It covers parser-corpus residuals that MySQL 8.4.9 accepts and that
can reuse MyLite's existing descriptor predicate planner by flipping the
comparison operator.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html

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
CREATE TABLE v1 (f1 DATE);
INSERT INTO t_dates VALUES
  ('2001-01-01','2001-04-10 12:34:56','2001-05-01',
   '2010-02-01 09:31:02',100000000000000000000002),
  ('2001-01-01','2001-03-01 00:00:00','2001-03-20',
   '2010-02-02 00:00:00',5);
INSERT INTO v1 VALUES ('2005-02-02');

SELECT COUNT(*) FROM t_dates WHERE '100000000000000000000002' = value;
SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' <= a;
SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' < a;
SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' >= a;
SELECT COUNT(*) FROM t_dates WHERE '2010-02-02 00:00:00.0' > a;
SELECT COUNT(*) FROM t_dates WHERE '5' <> value;
SELECT COUNT(*) FROM v1 WHERE '2005.02.02' = f1;
SELECT COUNT(*) FROM v1 WHERE '2005.02.02' <=> f1;
```

MySQL returns `1`, `2`, `1`, `1`, `1`, `1`, `1`, and `1`.

The comparison is still a normal comparison predicate. If the right operand is
a `DATETIME` column and the left operand is a constant string, MySQL compares
as temporal values. If the right operand is `DECIMAL`, the string constant is
converted through numeric comparison rules. Dotted date strings are accepted
for date comparison and normalize to the corresponding date.

## Scope

In scope:

- `STRING comparison_operator qualified_identifier` in table-backed predicate
  clauses;
- equality, NULL-safe equality, inequality, and ordered comparison operators
  already supported by MyLite's descriptor predicate planner;
- right-side `DECIMAL`, `DATE`, and `DATETIME` descriptor columns covered by
  the MySQL probes;
- zero fractional seconds in `DATETIME` comparison literals, such as
  `'2010-02-01 09:31:02.0'`, normalized to the existing `DATETIME(0)`
  predicate value;
- existing single-table `SELECT`, `UPDATE`, and `DELETE` predicate envelopes
  where descriptor comparison planning already applies;
- date literal normalization for dotted `YYYY.MM.DD` text when comparing
  against `DATE` columns.

Out of scope:

- literal-left `BETWEEN` and `IN` predicates;
- broad expression-left predicates or arbitrary expression planning;
- non-string literal-left expansion beyond the already supported integer,
  boolean, `NULL`, and hex literal subset;
- non-zero fractional seconds in literal-left `DATETIME` predicate strings;
- MySQL collation subtleties beyond MyLite's existing string predicate
  collation support;
- loaded time-zone, locale, or SQL-mode-specific temporal conversion behavior.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape.

```lemon
literal_left_comparison_value ::= STRING.

predicate_atom ::=
    literal_left_comparison_value predicate_comparison_operator qualified_identifier.
```

The parser builds the same comparison AST shape as existing literal-left
integer predicates. The runtime planner recognizes the left operand as a
literal-left descriptor comparison value, resolves the right descriptor column,
flips the operator, converts the left literal using the existing descriptor
conversion path, and emits a normal column comparison for SQLite.

## Storage, SQLite, And Performance

No SQLite fork hook is needed. This is a MyLite parser/planner translation that
keeps row filtering inside SQLite. MyLite does not load the table into memory or
post-filter rows for this slice.

## Tests

Tests cover:

- MySQL 8.4.9 expectation probes for decimal, datetime, and dotted-date
  literal-left equality, NULL-safe equality, inequality, and ordered
  comparisons;
- parser admission for the formerly placeholder corpus statements;
- runtime execution with MySQL-observed row counts;
- malformed literal-left `BETWEEN` and `IN` tails remaining syntax errors or
  unsupported placeholders as before.

## Compatibility Status

This slice moves the targeted literal-left string comparison predicates from
placeholder diagnostics to executable descriptor predicates. Literal-left
`BETWEEN` and `IN` forms remain tracked as parser-corpus residuals.
