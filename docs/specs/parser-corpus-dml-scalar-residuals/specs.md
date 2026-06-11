# Parser Corpus DML Scalar Residuals

This slice admits a small set of valid MySQL DML value expressions that remain
in the WordPress mysql-on-sqlite MySQL server-test query corpus after the
larger parser compatibility batches.

References:

- MySQL 8.4 Reference Manual, INSERT Statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, UPDATE Statement:
  https://dev.mysql.com/doc/refman/8.4/en/update.html
- MySQL 8.4 Reference Manual, Regular Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/regexp.html
- MySQL 8.4.9 runtime probes run with:
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names`

## MySQL-observed behavior

Observed against MySQL 8.4.9:

```sql
CREATE TABLE t(id INT PRIMARY KEY, v VARCHAR(64), i INT);
INSERT INTO t VALUES (1, 'seed', 0);
INSERT INTO t(id, v, i) VALUES (2, REGEXP_SUBSTR('abc', 'b', 1), (7));
SELECT ROW_COUNT(), @@warning_count;
UPDATE t SET v = REGEXP_SUBSTR('xyz', 'y', 1), i = ('9') WHERE id = 1;
SELECT ROW_COUNT(), @@warning_count;
SELECT id, v, i FROM t ORDER BY id;
```

The first DML statement reports affected rows `1` and warning count `0`. The
second DML statement also reports affected rows `1` and warning count `0`. The
stored rows are:

```text
1    y    9
2    b    7
```

The official MySQL 8.4 `UPDATE` statement describes assignment values as
expressions or `DEFAULT`. The `INSERT` statement stores expressions after
column type conversion. `REGEXP_SUBSTR(expr, pat[, pos[, ...]])` returns the
matching substring, or `NULL` when there is no match.

## Scope

MyLite will:

- admit `REGEXP_SUBSTR(expr, pat)` and `REGEXP_SUBSTR(expr, pat, pos)` as DML
  scalar values in `INSERT ... VALUES`, `REPLACE ... VALUES`, `INSERT ... SET`,
  `REPLACE ... SET`, `UPDATE ... SET`, and `ON DUPLICATE KEY UPDATE` contexts
  that already use `insert_value` or `update_value`;
- support the optional `pos` argument only when it is the literal default
  position `1` or `NULL`; `NULL` returns `NULL`, and other positions remain a
  deterministic unsupported subset;
- admit a single parenthesized literal or supported source-free scalar DML
  value, such as `(7)`, `('text')`, or `(REGEXP_SUBSTR('abc', 'b', 1))`,
  where the existing `insert_value` or `update_value` grammar accepts the
  unparenthesized value;
- preserve the existing MyLite runtime semantics for `REGEXP_SUBSTR`, including
  the current ASCII regular-expression subset, broader optional-argument
  diagnostics, and string/binary limitations;
- preserve existing DML conversion, warning, generated-column, default, and
  duplicate-key behavior by representing parenthesized values with the existing
  parenthesized-expression AST node that the DML planner already unwraps.

This is not a general expression-in-DML-values feature. MyLite still admits only
the documented DML scalar-expression subset.

## Syntax

Intended MyLite Lemon-syntax additions:

```lemon
insert_value ::= LPAREN dml_parenthesized_scalar_value RPAREN.
update_value ::= LPAREN dml_parenthesized_scalar_value RPAREN.

dml_constant_scalar_value ::= REGEXP_SUBSTR LPAREN function_argument_list RPAREN.
```

The parenthesized productions wrap the inner value in
`MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION`. The runtime already unwraps this node
while converting DML values, so no new storage or SQLite behavior is required.

## Compatibility decisions and gaps

- `REGEXP_SUBSTR` keeps MyLite's current baseline regexp-string implementation.
  Full ICU-compatible MySQL regular expressions, positions other than `1`,
  occurrence selection, match-type flags, and full character-set/collation
  behavior remain outside this slice.
- Parenthesized values are limited to the literal, supported source-free scalar
  function, and bitwise-not values enumerated by the
  `dml_parenthesized_scalar_value` grammar. This avoids broadening DML to
  arbitrary table-backed expressions before those semantics are specified.
- Nested parentheses outside existing function/arithmetic subgrammars remain
  deferred.
- No metadata or information-schema behavior changes are involved.
- No SQLite fork or extension hook is involved.

## Tests

Tests must cover:

- parser acceptance of `INSERT ... VALUES (REGEXP_SUBSTR(...))`;
- parser acceptance of `UPDATE ... SET col = REGEXP_SUBSTR(...)`;
- parser acceptance of parenthesized string/integer DML values;
- runtime storage and affected-row/warning behavior for the MySQL-observed
  insert/update paths;
- MySQL 8.4.9 expectation probes for the same statements;
- parser corpus benchmark movement over
  `build/perf-data/mysql-server-tests-queries.csv`.
