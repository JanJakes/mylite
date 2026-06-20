# Parser Corpus SELECT Clause Residuals

This slice reduces repeated MySQL server-test parser failures around SELECT
clause tails that MySQL accepts but MyLite either did not parse yet or should
not execute until broader expression planning exists.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/innodb-locking-reads.html

## MySQL 8.4.9 Runtime Observations

The expectation script for this slice verifies representative behavior against
MySQL 8.4.9.

Observed probe:

```sql
CREATE TABLE t1(id INT PRIMARY KEY, a INT, b VARCHAR(20)) ENGINE=InnoDB;
CREATE TABLE t2(id INT PRIMARY KEY, a INT, b VARCHAR(20)) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1,10,'hello'),(2,20,'world');
INSERT INTO t2 VALUES (1,10,'x'),(2,30,'y');

SELECT 0 LIMIT 0;
SELECT 1 AS a LIMIT 1,10;
SELECT 1 AS a LIMIT 1;
SELECT id FROM t1 ORDER BY NULL;
SELECT id FROM t1 ORDER BY 'a' DESC;
SELECT a,b FROM t1 GROUP BY a,b HAVING b='hello';
SELECT t1.a AS t1c1, t2.a AS t2c1
  FROM t1 JOIN t2 ON t1.id=t2.id
  HAVING t1c1 != t2c1;
SELECT a FROM t1 GROUP BY a HAVING a IN (10,20) ORDER BY a;
SELECT t1.id,t2.id
  FROM t1 JOIN t2 ON t1.id=t2.id
  FOR SHARE OF t1 FOR UPDATE OF t2;
SELECT t1.id,t2.id
  FROM t1 JOIN t2 ON t1.id=t2.id
  FOR SHARE OF t1 NOWAIT FOR UPDATE OF t2 SKIP LOCKED;
```

MySQL returns the result rows recorded in
`mysql_parser_corpus_select_clause_residuals_expectations.sh`. In particular,
tableless `LIMIT 0` and `LIMIT offset,row_count` can return an empty rowset,
literal `ORDER BY` keys are accepted, `HAVING` can refer to grouped columns and
selected aliases using richer predicates than MyLite historically executed, and
a query block can carry multiple locking read clauses with scoped `OF` lists and
wait modifiers. The grouped string-column comparison probe is now executable in
the `baseline-grouped-string-comparison-having` slice; the remaining richer
`HAVING` probes in this corpus slice stay explicit placeholders.

## Scope

In scope:

- executable no-source row-scalar `SELECT ... LIMIT row_count`,
  `SELECT ... LIMIT offset, row_count`, and
  `SELECT ... LIMIT row_count OFFSET offset`;
- the same tableless limit planning when a supported scalar filter has already
  been admitted through existing no-source or `FROM DUAL` paths;
- parser admission for repeated trailing locking read clauses after supported
  SELECT query blocks;
- preservation of existing MyLite locking-read behavior as a no-op
  compatibility marker;
- parser fallback classification for recognized constant `ORDER BY` keys and
  richer `HAVING` residuals outside the supported grouped string comparison
  subset that normal parsing cannot yet execute correctly;
- deterministic unsupported diagnostics for those recognized residual
  expression clauses instead of generic syntax errors;
- parser corpus movement measurement over
  `build/perf-data/mysql-server-tests-queries.csv`.

Out of scope:

- broad expression execution in `ORDER BY` or `HAVING`;
- executable string-literal, `NULL`, or user-variable order-key semantics;
- full `HAVING` expression planning, selected-alias ambiguity warnings, or
  MySQL's complete group-resolution rules;
- validation of locking `OF` target names, duplicate target diagnostics, scoped
  lock behavior, privileges, transaction locking, or subquery lock propagation;
- `SELECT ... INTO` no-row warning/error parity for tableless limit-offset
  forms;
- branch-local locking behavior in compound query expressions.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape and do not copy
MySQL grammar. This slice realizes the narrow tableless-limit and repeated-lock
surfaces with targeted parser repairs instead of widening the core Lemon SELECT
productions, because the direct optional-tail expansion materially increased
parser generation cost.

```lemon
select_statement ::=
    SELECT select_modifiers select_item_list
    window_clause_opt select_order_clause_opt limit_clause_opt
    select_locking_clause_opt.

select_locking_clause_opt ::= .
select_locking_clause_opt ::= select_locking_clause_list.

select_locking_clause_list ::= select_locking_clause.
select_locking_clause_list ::= select_locking_clause_list select_locking_clause.

select_locking_clause ::= FOR UPDATE select_lock_wait_opt.
select_locking_clause ::= FOR SHARE select_lock_wait_opt.
select_locking_clause ::= LOCK IN SHARE MODE.

select_lock_wait_opt ::= .
select_lock_wait_opt ::= NOWAIT.
select_lock_wait_opt ::= SKIP LOCKED.
```

The parser driver keeps the existing MyLite-owned handling for `OF table[, ...]`
target lists by skipping those target-name tokens before Lemon sees the
locking-clause wait option or the next repeated locking clause.

## Runtime Behavior

No SQLite fork hook is needed. This slice stays in MyLite's parser and runtime
planning layer.

For `SELECT ... LIMIT` forms that the normal parser does not accept because no
table source is present or the source is `DUAL`, MyLite parses the prefix,
validates that the tail is one of the existing literal `LIMIT` shapes, appends a
normal `LIMIT_CLAUSE` node, and then uses the existing row-scalar limit planner
and SQL builder. `ORDER BY` remains rejected for tableless row-scalar planning
until constant order keys are explicitly modeled. Repeated locking clauses are
handled by parsing through the first locking clause, validating that the
remaining tail is a sequence of supported locking clauses, and executing the
existing no-op lock marker. Later repeated clauses are accepted for syntax
compatibility but do not add transaction lock behavior.

Recognized constant `ORDER BY` and residual `HAVING` forms outside the
supported grouped string comparison subset that fail the normal parser become
unsupported-utility placeholders. They return the existing deterministic
unsupported diagnostic and do not route through SQLite, avoiding silent
differences in MySQL type conversion, collation, aggregate, and alias
resolution semantics.

## Tests

Tests cover:

- MySQL 8.4.9 expectations for tableless limits, constant order keys, the
  grouped string comparison `HAVING` case, residual `HAVING` predicates, and
  repeated locking clauses;
- parser AST admission for executable tableless limits and repeated locking
  clauses;
- parser fallback admission for constant order and residual `HAVING` surfaces;
- runtime row counts for tableless `LIMIT` forms;
- runtime execution of repeated locking clauses over a supported joined source;
- runtime unsupported diagnostics for residual placeholder forms.

## Compatibility

This is a partial SELECT compatibility improvement. It expands supported
tableless `LIMIT`, parser-admits repeated locking clauses, and makes selected
corpus residuals fail predictably as unsupported placeholders. It does not
claim full MySQL SELECT expression-clause execution.
