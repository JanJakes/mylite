# `COUNT(DISTINCT ...)` aggregate

## Scope

This feature adds MySQL-compatible `COUNT(DISTINCT expr [, expr ...])` for the
aggregate execution paths already owned by MyLite:

- no-table aggregate `SELECT` where each argument can be evaluated without a row
  source
- single-table aggregate `SELECT`
- grouped aggregate queries
- `HAVING` and `ORDER BY` expressions over aggregate results
- select-list aliases where current aggregate support already resolves them

Out of scope:

- `SUM(DISTINCT)`, `AVG(DISTINCT)`, `MIN(DISTINCT)`, and `MAX(DISTINCT)`
- windowed aggregate `OVER (...)` forms
- `GROUP_CONCAT(DISTINCT ...)` and other aggregate families
- rollup, grouping sets, and `GROUPING()`
- duplicate-elimination behavior outside the existing `SELECT DISTINCT`,
  `UNION DISTINCT`, and aggregate grouping surfaces
- full collation fidelity beyond MyLite's currently implemented comparison
  domains

## Sources

- MySQL 8.4 Reference Manual, Aggregate Function Descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, Aggregate Functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions-and-modifiers.html
- Existing MyLite specs:
  - `docs/specs/aggregate-grouping/specs.md`
  - `docs/specs/select-distinct/specs.md`
  - `docs/specs/union-query-expressions/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

`COUNT(DISTINCT expr [, expr ...])` evaluates its arguments for each row in the
current aggregate group, ignores rows where any argument is `NULL`, and returns
the number of distinct remaining argument tuples. A single argument is a
one-column tuple. Multiple arguments are compared as ordered tuples, not by
concatenating values. If no rows match, or every candidate row has a `NULL`
argument, the result is `0`.

The return descriptor matches existing `COUNT` metadata: `LONGLONG`, length
`21`, decimals `0`, binary collation, `NOT_NULL`, `BINARY`, and `NUM`.

### Fixture

Runtime probes used this fixture:

```sql
DROP DATABASE IF EXISTS mylite_count_distinct_probe;
CREATE DATABASE mylite_count_distinct_probe DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_0900_ai_ci;
USE mylite_count_distinct_probe;

CREATE TABLE t (
  id INT PRIMARY KEY,
  grp VARCHAR(10),
  a INT NULL,
  b INT NULL,
  s VARCHAR(20) NULL
) CHARACTER SET utf8mb4;

INSERT INTO t VALUES
  (1,'x',1,1,'1'),
  (2,'x',1,1,'1'),
  (3,'x',1,2,'01'),
  (4,'x',2,NULL,'bad'),
  (5,'y',NULL,2,'2'),
  (6,'y',NULL,2,'2'),
  (7,'y',3,3,NULL),
  (8,NULL,3,3,'3');

CREATE TABLE empty_t (a INT NULL, b INT NULL, s VARCHAR(20) NULL);
```

### Verified results

| SQL | Result |
| --- | --- |
| `SELECT COUNT(DISTINCT a), COUNT(DISTINCT b), COUNT(DISTINCT s), COUNT(DISTINCT a,b), COUNT(DISTINCT a,s) FROM t` | `3`, `3`, `5`, `3`, `4` |
| `SELECT grp, COUNT(DISTINCT a), COUNT(DISTINCT a,b), COUNT(DISTINCT s) FROM t GROUP BY grp ORDER BY grp IS NULL, grp` | `('x',2,2,3)`, `('y',1,1,1)`, `(NULL,1,1,1)` |
| `SELECT COUNT(DISTINCT a) FROM empty_t` | `0` |
| `SELECT COUNT(DISTINCT a) FROM t WHERE id > 100` | `0` |
| `SELECT COUNT(DISTINCT NULL), COUNT(DISTINCT 1), COUNT(DISTINCT 1,NULL)` | `0`, `1`, `0` |
| `SELECT COUNT(DISTINCT CAST(s AS SIGNED)) FROM t` | `4`, with one 1292 truncation warning for `'bad'` |
| two identical `COUNT(DISTINCT CAST(s AS SIGNED))` expressions in the same select list | same count twice and the truncation warning twice, once per aggregate expression |
| `SELECT COUNT(DISTINCT s+0), COUNT(DISTINCT s,a) FROM t` | `4`, `4`; `s+0` emits one 1292 double-conversion warning for `'bad'` |
| `SELECT COUNT(DISTINCT a,b,s) FROM t` | `3` |
| `SELECT COUNT(DISTINCT a,a), COUNT(DISTINCT s,s) FROM t` | `3`, `5` |
| `SELECT COUNT(DISTINCT a) AS c FROM t HAVING c = 3` | one row: `3` |
| `SELECT COUNT(DISTINCT a) AS c FROM t HAVING c = 2` | empty result |
| `SELECT grp, COUNT(DISTINCT a) AS c FROM t GROUP BY grp HAVING c > 1 ORDER BY grp` | one row: `('x',2)` |

`COUNT(DISTINCT)` and `COUNT(DISTINCT *)` are syntax errors 1064 / `42000`.
Aggregate nesting remains invalid: `COUNT(DISTINCT COUNT(*))` produces 1111 /
`HY000`, matching the existing invalid group-function policy.

MySQL supports `SUM(DISTINCT a)` and related aggregate-local `DISTINCT` forms,
but MyLite keeps them out of this feature. They should continue to fail with the
current deterministic parser/runtime policy until separately specified.

### Collation and tuple comparison

For nonbinary text, distinct comparison follows the active collation in MySQL.
The verified `utf8mb4_0900_ai_ci` values `a`, `A`, `a `, `á`, `Á`, and `NULL`
produce `COUNT(DISTINCT s)=3`. The analogous binary-string fixture produces
`5`. MyLite should use its current field-descriptor-aware duplicate comparison,
which already distinguishes binary text from collation-insensitive text for the
implemented `SELECT DISTINCT` surface. Full collation coverage remains deferred
to the broader collation roadmap.

## Parser and AST

The parser should accept only the COUNT-local distinct aggregate in this
feature:

```lemon
primary_expression(A) ::= aggregate_distinct_call(B). {
    A = B;
}

aggregate_distinct_call(A) ::= function_name(N) LPAREN DISTINCT expression_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_count_distinct_call(state, N, L, R);
}
```

The parser helper validates that the function name is `COUNT`; other
aggregate-local `DISTINCT` names remain outside this feature. The AST should
keep this distinct from both `COUNT(expr)` and top-level `SELECT DISTINCT`. A
dedicated aggregate argument kind is preferred:

- `STAR` for `COUNT(*)`
- `EXPRESSION` for one ordinary aggregate argument
- `DISTINCT_EXPRESSION_LIST` for `COUNT(DISTINCT expr [, expr ...])`

`COUNT(DISTINCT *)` should not be accepted by this grammar. `COUNT(DISTINCT)`
should not be accepted because `expression_list` is nonempty.

## Runtime design

Each aggregate binding for `COUNT(DISTINCT ...)` needs per-group distinct tuple
state:

1. Evaluate every argument expression for the input row using existing aggregate
   argument expression machinery.
2. Preserve expression warnings exactly as normal expression evaluation emits
   them. Do not cache converted argument results across aggregate expressions,
   because MySQL records warnings per aggregate expression.
3. If any evaluated argument is `NULL`, discard the tuple.
4. Compare the remaining tuple with tuples already accepted for that aggregate
   state. Use the current MyLite duplicate-comparison helper with inferred
   argument descriptors so text and binary values follow the same implemented
   comparison domains as `SELECT DISTINCT`.
5. If the tuple is new, append a deep copy and increment the distinct count.
6. Finalization returns the distinct count as an integer expression value.

For no-table aggregate `SELECT`, MyLite should evaluate against the single
synthetic row already used for `COUNT(*)`. `COUNT(DISTINCT 1)` returns `1`;
`COUNT(DISTINCT NULL)` and `COUNT(DISTINCT 1,NULL)` return `0`.

The implementation should not delegate the distinct aggregate to SQLite.
MyLite needs direct control over MySQL warning emission, NULL-tuple skipping,
collation-sensitive comparison, metadata, and unsupported-surface diagnostics.

## Test expectations

Runtime tests should cover:

- one-expression and multi-expression counts
- duplicate tuples
- skipped tuples with any `NULL` argument
- empty tables and no matching rows
- no-table aggregate queries
- grouped counts, including a `NULL` grouping key
- aliases in `HAVING` and `ORDER BY`
- warning behavior from `CAST(s AS SIGNED)` and `s+0`
- duplicate warning emission from two identical distinct aggregate expressions
- repeated argument expressions such as `COUNT(DISTINCT a,a)`
- metadata for visible `COUNT(DISTINCT ...)` outputs
- syntax errors for `COUNT(DISTINCT)` and `COUNT(DISTINCT *)`
- invalid aggregate placement and nesting errors

Parser tests should assert that `COUNT(DISTINCT a)` and
`COUNT(DISTINCT a, b + 1)` produce an aggregate call with the distinct argument
kind and an expression-list child.

## Compatibility decisions

- Mark `COUNT(DISTINCT)` implemented with documented gaps for the aggregate
  execution surfaces listed in this spec until full charset and collation
  fidelity lands.
- Keep other aggregate-local `DISTINCT` families deferred.
- Reuse the existing MyLite duplicate comparison behavior. This intentionally
  inherits current text/collation limitations instead of expanding collation
  semantics inside this feature.
- Keep large distinct sets in memory for this slice. Spill-to-storage and
  optimizer pushdown are deferred.

## Risks and deferred work

- Full MySQL collation and character-set coercion can change distinct grouping
  for text values outside MyLite's current comparison domain. Runtime tests
  include the MySQL-verified text and numeric tuple cases from this spec.
- Large groups can retain many distinct tuples in memory. A future execution
  engine can add spilling without changing the AST or public behavior.
- Other aggregate-local `DISTINCT` forms need separate result-type,
  warning-order, and metadata specifications before support.
