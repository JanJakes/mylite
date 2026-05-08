# Aggregate functions and grouping

## Scope

Task 25 adds the first MySQL-compatible aggregate query surface. It builds on
the existing table-backed `SELECT`, expression, `WHERE`, `ORDER BY`, `LIMIT`,
result metadata, scalar-function, and `CASE` expression foundations.

In scope for the first implementable slice:

- one user base table in the `FROM` clause, including the existing selected
  schema, schema-qualified table, and table-alias resolution rules
- no-table aggregate `SELECT` where every expression can be evaluated without a
  row source, for example `SELECT COUNT(*)`
- `COUNT(*)`, `COUNT(expr)`, `SUM(expr)`, `AVG(expr)`, `MIN(expr)`, and
  `MAX(expr)`
- follow-on aggregate slices for `COUNT(DISTINCT expr [, expr ...])` and
  `GROUP_CONCAT(...)` in the same aggregate execution surfaces
- aggregate functions in the select list, `HAVING`, and `ORDER BY`
- aggregate queries without `GROUP BY`, treating the filtered input as one
  implicit group
- `GROUP BY` over the currently supported scalar expression subset
- `GROUP BY` references by expression, unqualified column, selected alias, and
  one-based select-list ordinal
- `HAVING` over the currently supported scalar expression subset plus aggregate
  expressions and visible select-list aliases
- alias ambiguity warnings for `GROUP BY` and `HAVING` when MySQL emits them
- `WHERE` before grouping and `HAVING` after grouping
- `ORDER BY` / `LIMIT` after grouping and `HAVING`, using the existing Task 18
  ordering and limiting semantics where the order expression is supported
- result metadata for grouped columns and aggregate expressions
- MySQL-compatible diagnostics for aggregate misuse, unresolved grouping
  references, unsupported aggregate arity, and `ONLY_FULL_GROUP_BY` violations
- warning-list and warning-count lifecycle for successful and failed aggregate
  statements

Out of scope for the first implementable slice:

- derived tables, CTEs, views, subqueries as row sources, set operations,
  lateral references, and table functions
- `DISTINCT`, `DISTINCTROW`, and aggregate-local `DISTINCT` forms other than
  `COUNT(DISTINCT ...)` and `GROUP_CONCAT(DISTINCT ...)`
- `GROUP BY ... WITH ROLLUP` and `GROUPING()`
- window-function execution and aggregate `OVER (...)` clauses
- aggregate functions beyond `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, and
  `GROUP_CONCAT`, including bit, JSON, remaining string, statistical, spatial,
  and variance aggregates
- `ANY_VALUE()` and broader functional-dependence detection beyond
  primary-key/non-null-unique-key proof for non-grouped base-table columns
- `HAVING` references to outer query columns
- `SELECT` modifiers that tune MySQL execution strategy, such as
  `SQL_BIG_RESULT` and `SQL_SMALL_RESULT`
- `max_sort_length` truncation for grouping/sorting long string values
- full enum/set, temporal, JSON, bit, binary-string, and collation-specific
  aggregate edge cases
- optimizer pushdown, index-assisted grouping, streaming aggregation, temporary
  table selection, and SQLite aggregate delegation unless each behavior is
  proven to preserve MySQL rows, metadata, warnings, and errors
- protocol packet metadata beyond the existing public result descriptor surface

This task should not mark grouping or aggregates as supported until runtime
tests compare MyLite against MySQL 8.4.9 for result rows, warnings, errors,
metadata, and statement side effects.

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Aggregate Functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions-and-modifiers.html
- MySQL 8.4 Reference Manual, Aggregate Function Descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, MySQL Handling of GROUP BY:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4 Reference Manual, Detection of Functional Dependence:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-functional-dependence.html
- MySQL 8.4 Reference Manual, Problems with Column Aliases:
  https://dev.mysql.com/doc/refman/8.4/en/problems-with-alias.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS` statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/case-expression/specs.md`
  - `docs/specs/count-distinct-aggregate/specs.md`
  - `docs/specs/group-concat-function/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/create-table-indexes/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

### Test fixture

Runtime probes used this fixture:

```sql
DROP DATABASE IF EXISTS mylite_aggregate_grouping;
CREATE DATABASE mylite_aggregate_grouping;
USE mylite_aggregate_grouping;

CREATE TABLE t (
  id INT PRIMARY KEY,
  grp VARCHAR(10),
  n INT,
  decv DECIMAL(10,2),
  txt VARCHAR(20),
  nullable INT NULL,
  u INT UNIQUE
);

INSERT INTO t VALUES
  (1, 'a', 10, 1.50, 'alpha', NULL, 101),
  (2, 'a', 20, 2.25, 'beta', 5, 102),
  (3, 'b', NULL, NULL, 'gamma', NULL, 103),
  (4, 'b', 0, -3.75, 'delta', 0, 104),
  (5, NULL, 7, 4.00, 'epsilon', 7, 105);

CREATE TABLE empty_t (
  id INT,
  n INT,
  decv DECIMAL(10,2),
  txt VARCHAR(20)
);

CREATE TABLE fd_t (
  id INT PRIMARY KEY,
  name VARCHAR(20),
  age INT
);

INSERT INTO fd_t VALUES (1, 'ann', 10), (2, 'bob', 20);

CREATE TABLE alias_t (
  col2 INT,
  col1 INT
);

INSERT INTO alias_t VALUES (2, 10), (3, 20), (2, 30);
```

### Aggregate results

Aggregate functions operate over each group. Without `GROUP BY`, an aggregate
query has one implicit group over the rows remaining after `WHERE`.

`COUNT(*)` counts rows. `COUNT(expr)` counts non-`NULL` expression results.
`SUM`, `AVG`, `MIN`, and `MAX` ignore `NULL` input values. For an empty input or
an all-`NULL` argument, `COUNT` returns `0`; the other scoped aggregates return
`NULL`.

Representative runtime results:

| SQL | Result |
| --- | --- |
| `SELECT COUNT(*), COUNT(n), COUNT(nullable), SUM(n), AVG(n), MIN(n), MAX(n) FROM t` | `5`, `4`, `3`, `37`, `9.2500`, `0`, `20` |
| `SELECT COUNT(*), COUNT(n), SUM(n), AVG(n), MIN(n), MAX(n) FROM empty_t` | `0`, `0`, `NULL`, `NULL`, `NULL`, `NULL` |
| `SELECT COUNT(*) AS c FROM t HAVING c > 0` | one row: `5` |
| `SELECT COUNT(*) AS c FROM t HAVING c > 10` | empty result |
| `SELECT COUNT(*) AS c FROM t WHERE id > 10 HAVING c = 0` | one row: `0` |
| `SELECT COUNT(*) AS c FROM t WHERE id > 10 HAVING c > 0` | empty result |
| `SELECT COUNT(*), COUNT(NULL), COUNT(1), SUM(1), AVG(1), MIN(1), MAX(1)` | `1`, `0`, `1`, `1`, `1.0000`, `1`, `1` |

Grouped aggregate results verified against MySQL:

| SQL | Ordered result |
| --- | --- |
| `SELECT grp, COUNT(*), COUNT(n), SUM(n), AVG(n), MIN(txt), MAX(txt) FROM t GROUP BY grp ORDER BY grp IS NULL, grp` | `('a',2,2,30,15.0000,'alpha','beta')`, `('b',2,1,0,0.0000,'delta','gamma')`, `(NULL,1,1,7,7.0000,'epsilon','epsilon')` |
| `SELECT grp AS g, SUM(n) AS total FROM t GROUP BY g HAVING total > 10 ORDER BY g` | `('a',30)` |
| `SELECT grp, SUM(n) AS total FROM t GROUP BY 1 HAVING SUM(n) >= 10 ORDER BY 2 DESC` | `('a',30)` |
| `SELECT grp, SUM(n) AS total FROM t WHERE n IS NOT NULL GROUP BY grp HAVING total > 7 ORDER BY grp IS NULL, grp` | `('a',30)` |

The order of groups is not guaranteed without `ORDER BY`. Tests that depend on
group order must include an explicit ordering clause or compare unordered row
sets.

### Numeric conversion and warnings

`SUM` and `AVG` evaluate their arguments as numeric values. String conversion
can produce truncation warnings, and each aggregate expression may produce its
own warning for the same input value. `MIN` and `MAX` preserve string comparison
for string arguments.

Verified fixture:

```sql
CREATE TABLE conv_t (id INT PRIMARY KEY, s VARCHAR(20));
INSERT INTO conv_t VALUES (1, '10'), (2, 'bad'), (3, NULL), (4, '2.5x');
```

Runtime result:

| SQL | Result | Warnings |
| --- | --- | --- |
| `SELECT SUM(s), AVG(s), MIN(s), MAX(s) FROM conv_t` | `12.5`, `4.166666666666667`, `10`, `bad` | four 1292 warnings: one for `'bad'` in `SUM`, one for `'bad'` in `AVG`, one for `'2.5x'` in `SUM`, and one for `'2.5x'` in `AVG` |

This first slice should reuse the Task 16 and Task 24 conversion/warning
machinery where possible, but aggregate evaluation needs statement-visible
warnings to remain ordered and counted like MySQL.

### Name resolution, aliases, and ordinals

`GROUP BY` and `HAVING` can refer to select-list aliases. `GROUP BY` also
accepts one-based select-list ordinals. Ordinals are deprecated upstream but
remain part of MySQL 8.4 behavior and must be accepted for compatibility.

For unqualified names, MySQL searches table columns before select-list aliases
in `GROUP BY` and `HAVING`. This differs from `ORDER BY`, which prefers
select-list aliases. If a name could mean both a table column and a selected
alias, MySQL chooses the table column and records ambiguity warnings in the
affected clauses.

Representative runtime observations:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT grp AS g, SUM(n) AS total FROM t GROUP BY g HAVING total > 10` | groups by alias `g`; `HAVING` reads alias `total`; returns `('a',30)` |
| `SELECT grp, SUM(n) AS total FROM t GROUP BY 1 HAVING SUM(n) >= 10 ORDER BY 2 DESC` | `GROUP BY 1` uses first select expression; `ORDER BY 2` uses second select expression; returns `('a',30)` |
| `SELECT COUNT(*) AS c FROM conv_t GROUP BY 2` | error 1054 / `42S22`, unknown column `2` in `group statement` |
| `SELECT grp, SUM(n) FROM t GROUP BY 3` | error 1054 / `42S22`, unknown column `3` in `group statement` |
| `SELECT grp, SUM(n) FROM t GROUP BY 0` | error 1054 / `42S22`, unknown column `0` in `group statement` |
| `SELECT COUNT(*) FROM t GROUP BY missing_col` | error 1054 / `42S22`, unknown column `missing_col` in `group statement` |
| `SELECT COUNT(col1) AS col2 FROM alias_t GROUP BY col2 HAVING col2 = 2` | returns `2`; records warning 1052 for `col2` in `group statement` and warning 1052 for `col2` in `having clause` |
| `SELECT n AS x, id AS x, COUNT(*) AS c FROM t GROUP BY x` | error 1052 / `23000`, `Column 'x' in group statement is ambiguous` |
| `SELECT n AS x, id AS x FROM t HAVING x = 10` | error 1052 / `23000`, `Column 'x' in having clause is ambiguous` |

An implementation should preserve the resolved expression kind, not just a
string name. This matters for aliases that resolve to scalar expressions,
duplicate output labels, and the distinction between table-column and alias
resolution.

### HAVING evaluation

`WHERE` filters input rows before grouping. `HAVING` filters groups after
aggregate state is finalized and can reference aggregate expressions. `HAVING`
without an explicit `GROUP BY` is valid for aggregate queries because the query
still has one implicit group.

For a nonaggregate query, `HAVING` is not an implicit aggregation trigger.
MySQL evaluates it as a row filter over projected output labels after `WHERE`
and before `ORDER BY`/`LIMIT`. Non-projected table columns remain unknown in
that context, and duplicate projected labels are ambiguous in `having clause`.

Representative runtime observations:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT id, COUNT(*) FROM conv_t GROUP BY id HAVING id > 2 ORDER BY id` | returns `(3,1)`, `(4,1)` |
| `SELECT id AS x, COUNT(*) AS c FROM conv_t GROUP BY id HAVING x > 2 ORDER BY x` | returns `(3,1)`, `(4,1)` |
| `SELECT id AS x, COUNT(*) AS c FROM conv_t GROUP BY x HAVING c = 1 ORDER BY x` | returns `(1,1)`, `(2,1)`, `(3,1)`, `(4,1)` |
| `SELECT id, COUNT(*) AS c FROM conv_t GROUP BY id HAVING COUNT(*) > 0 ORDER BY id` | returns all four ids |
| `SELECT id, COUNT(*) AS c FROM conv_t GROUP BY id HAVING SUM(id) > 2 ORDER BY id` | returns `(3,1)`, `(4,1)` |
| `SELECT id, n FROM t HAVING n > 0 ORDER BY id` | returns rows where projected `n` is positive |
| `SELECT n AS id FROM t HAVING id >= 10 ORDER BY id` | resolves `id` to the projected alias, not the base table `id` |
| `SELECT id FROM t HAVING n > 0` | error 1054 / `42S22`, hidden `n` is not visible in `having clause` |

`HAVING` predicates use MySQL truthiness conversion after group-level
expression evaluation. Conversion warnings from `HAVING` expressions are
statement-visible even when the group is filtered out.

### ONLY_FULL_GROUP_BY

With the default SQL mode, aggregate queries reject nonaggregate references in
the select list, `HAVING`, or `ORDER BY` unless the reference is grouped or
functionally dependent on grouped columns.

Verified diagnostics:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT grp, n, SUM(n) FROM t GROUP BY grp` | error 1055 / `42000`, nonaggregated select expression is not in `GROUP BY` and is not functionally dependent |
| `SELECT name, MAX(age) FROM fd_t` | error 1140 / `42000`, aggregate query without `GROUP BY` contains a nonaggregated select expression |
| `SELECT grp FROM t HAVING SUM(n) > 0` | error 1140 / `42000`, aggregate query without `GROUP BY` contains a nonaggregated select expression |
| `SELECT id, name, MAX(age) FROM fd_t GROUP BY id` | accepted because `id` is a primary key and `name` is functionally dependent on it; returns `(1,'ann',10)`, `(2,'bob',20)` |

The first implementation deliberately uses a narrower proof than MySQL:

- A selected expression is group-invariant when it is an aggregate call, a
  grouping expression, a selected expression reached through a valid grouping
  ordinal, a resolved grouping alias, a constant, or an expression derived only
  from group-invariant children.
- Primary-key and `UNIQUE NOT NULL` functional-dependence proof is deferred even
  when MySQL can prove it.
- Functional dependence through joins, generated columns, expressions,
  equality predicates, outer joins, derived tables, views, and broader schema
  metadata remains deferred.

When MyLite cannot prove functional dependence, it rejects the query
rather than returning nondeterministic nonaggregate values. This is stricter
than MySQL only for cases MySQL can prove and MyLite cannot yet prove, and those
cases must be listed as deferred until metadata support catches up.

### Invalid aggregate placement and arity

Aggregate functions are not legal in `WHERE`, because `WHERE` operates before
grouping.

Verified diagnostics:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT COUNT(*) FROM t WHERE COUNT(*) > 0` | error 1111 / `HY000`, invalid use of group function |
| `SELECT COUNT() FROM t` | syntax error 1064 / `42000` |
| `SELECT SUM() FROM t` | syntax error 1064 / `42000` |
| `SELECT COUNT(*, n) FROM t` | syntax error 1064 / `42000` |

`COUNT(DISTINCT grp)` returned `2` in the runtime probe because `NULL` did not
contribute to the distinct count. This first slice intentionally defers
`DISTINCT` aggregate support; parser support may accept the syntax only if the
analyzer returns an explicit unsupported-feature diagnostic until execution is
implemented.

### Result metadata

Aggregate result descriptors have no origin schema, table, original table, or
original column. Grouping columns preserve base-column origin metadata.

Observed `mysql --column-type-info -vvv` metadata:

| Expression | Type | Length | Decimals | Flags | Origin |
| --- | --- | --- | --- | --- | --- |
| `COUNT(*) AS c_all` | `LONGLONG` | `21` | `0` | `NOT_NULL BINARY NUM` | none |
| `COUNT(n) AS c_n` | `LONGLONG` | `21` | `0` | `NOT_NULL BINARY NUM` | none |
| `SUM(n) AS s_int` | `NEWDECIMAL` | `33` | `0` | `BINARY NUM` | none |
| `AVG(n) AS a_int` | `NEWDECIMAL` | `16` | `4` | `BINARY NUM` | none |
| `SUM(decv) AS s_dec` | `NEWDECIMAL` | `34` | `2` | `BINARY NUM` | none |
| `AVG(decv) AS a_dec` | `NEWDECIMAL` | `16` | `6` | `BINARY NUM` | none |
| `SUM(r)` where `r DOUBLE` | `DOUBLE` | `23` | `31` | `BINARY NUM` | none |
| `AVG(r)` where `r DOUBLE` | `DOUBLE` | `23` | `31` | `BINARY NUM` | none |
| `SUM(s)` where `s VARCHAR(10)` | `DOUBLE` | `23` | `31` | `BINARY NUM` | none |
| `AVG(s)` where `s VARCHAR(10)` | `DOUBLE` | `23` | `31` | `BINARY NUM` | none |
| `MIN(n) AS min_n` | `LONG` | `11` | `0` | `BINARY NUM` | none |
| `MAX(txt) AS max_txt` | `VAR_STRING` | `20` | `31` | none | none |
| grouped `grp` | `VAR_STRING` | `10` | `0` | observed `NUM` flag | `mylite_aggregate_grouping.t.grp` |

Implementation should preserve the existing Task 23 descriptor API behavior:
labels are statement-owned output labels, aggregate output has empty origin
fields, aggregate expressions are nullable except `COUNT`, and base grouped
columns retain their origin metadata. MySQL's exact `max_length` is
materialized from produced rows and remains a known Task 23 limitation.

## Parser and AST design

Aggregate support needs explicit AST nodes rather than treating these functions
as ordinary scalar built-ins. The analyzer must know where aggregate functions
appear, which query block they belong to, and which expressions are evaluated
per row versus per group.

Recommended AST additions:

- expression node kind for aggregate calls:
  - function identifier preserving original spelling for diagnostics
  - canonical aggregate kind: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`,
    `GROUP_CONCAT`
  - argument mode: star, single expression, or distinct expression list
  - source span covering the function call
- `SELECT` query block fields:
  - optional `GROUP BY` list with expression, ordinal, and direction metadata
    if existing grammar keeps sort directions
  - optional `HAVING` expression
  - aggregate-presence flag computed by analysis, not parsing
- resolved grouping key representation:
  - original expression
  - resolved select-list ordinal or alias, if applicable
  - descriptor for result and grouping comparisons
- aggregate binding representation:
  - per-aggregate argument expression
  - result descriptor
  - per-group state slot

Aggregate expressions should be rejected during analysis in row-before-group
contexts such as `WHERE`, insert values, update assignments, delete predicates,
and grouping key expressions unless MySQL permits the exact use.

## Lemon-style grammar snippets

These snippets describe MyLite's intended grammar shape. They are not copied
from MySQL grammar.

```lemon
select_core(A) ::= SELECT select_options(O) select_list(L) from_clause(F)
                   where_opt(W) group_by_opt(G) having_opt(H)
                   order_by_opt(R) limit_opt(M). {
    A = mylite_ast_select_new(O, L, F, W, G, H, R, M);
}

group_by_opt(A) ::= . {
    A = NULL;
}

group_by_opt(A) ::= GROUP BY group_expr_list(L). {
    A = L;
}

group_expr_list(A) ::= group_expr(E). {
    A = mylite_ast_group_list_new(E);
}

group_expr_list(A) ::= group_expr_list(L) COMMA group_expr(E). {
    A = mylite_ast_group_list_append(L, E);
}

group_expr(A) ::= expr(E) ordering_opt(O). {
    A = mylite_ast_group_expr_new(E, O);
}

having_opt(A) ::= . {
    A = NULL;
}

having_opt(A) ::= HAVING expr(E). {
    A = E;
}

expr(A) ::= aggregate_call(F). {
    A = F;
}

aggregate_call(A) ::= aggregate_name(N) LP STAR RP. {
    A = mylite_ast_aggregate_call_new(N, MYLITE_AGG_ARG_STAR, NULL);
}

aggregate_call(A) ::= aggregate_name(N) LP expr(E) RP. {
    A = mylite_ast_aggregate_call_new(N, MYLITE_AGG_ARG_EXPR, E);
}

aggregate_call(A) ::= COUNT LP DISTINCT distinct_expr_list(L) RP. {
    A = mylite_ast_aggregate_call_new_distinct(MYLITE_AGG_COUNT, L);
}

aggregate_name(A) ::= COUNT. { A = MYLITE_AGG_COUNT; }
aggregate_name(A) ::= SUM.   { A = MYLITE_AGG_SUM; }
aggregate_name(A) ::= AVG.   { A = MYLITE_AGG_AVG; }
aggregate_name(A) ::= MIN.   { A = MYLITE_AGG_MIN; }
aggregate_name(A) ::= MAX.   { A = MYLITE_AGG_MAX; }
```

`COUNT(DISTINCT ...)` is specified and implemented separately in
`docs/specs/count-distinct-aggregate/specs.md`. `GROUP_CONCAT(...)` and its
aggregate-local options are specified in
`docs/specs/group-concat-function/specs.md`. `SUM(DISTINCT ...)`,
`AVG(DISTINCT ...)`, `MIN(DISTINCT ...)`, and `MAX(DISTINCT ...)` should remain
unsupported until their deduplication behavior and metadata are specified.

## Analyzer semantics

Analysis should process one query block at a time:

1. Resolve `FROM` and `WHERE` as existing table-backed `SELECT` does.
2. Bind select-list expressions and record aliases and duplicate labels.
3. Bind `GROUP BY` items. Integer literals in grouping position can be
   one-based select-list ordinals; valid ordinals bind to the selected
   expression. Out-of-range or zero ordinals should produce MySQL-compatible
   1054 diagnostics in the scoped runtime cases.
4. Discover aggregate calls in select, `HAVING`, and `ORDER BY` expressions.
   Reject aggregate calls in `WHERE` and grouping key expressions.
5. Determine whether the query is aggregated. A query is aggregated when it has
   at least one aggregate call or a `GROUP BY` clause. `HAVING` alone does not
   make a query aggregated.
6. Validate `ONLY_FULL_GROUP_BY` for selected expressions, `HAVING`, and
   `ORDER BY`.
7. Create result descriptors after aggregate and grouping validation, preserving
   output labels even for hidden `ORDER BY` expressions.

Expression classification should distinguish:

- group-invariant expressions: aggregate calls, grouping expressions,
  constants, and expressions derived only from group-invariant expressions
- row-dependent nonaggregates: base columns or expressions over base columns
  that are not group-invariant in the first implementation
- illegal aggregate nesting: aggregate calls inside aggregate arguments for the
  same query block

The first implementation should reject aggregate nesting deterministically.

## Runtime design

The runtime should use MyLite-owned aggregate execution rather than relying
directly on SQLite aggregate semantics. SQLite can assist with row storage or
sorting, but MyLite needs control over MySQL type conversion, warning counts,
metadata, alias resolution, `ONLY_FULL_GROUP_BY`, and error behavior.

Suggested execution phases:

1. Open the scoped single-table row source or create a synthetic one-row source
   for no-table aggregate queries, matching `SELECT COUNT(*)` returning `1`
   without a `FROM` clause.
2. Evaluate `WHERE` per input row using existing predicate semantics. Rejected
   rows do not reach grouping.
3. Compute each grouping key for accepted rows. `NULL` keys compare equal for
   grouping.
4. Find or create the group state. For implicit aggregation, use exactly one
   group even when the input is empty.
5. For each accepted row, evaluate aggregate arguments and update aggregate
   state:
   - `COUNT(*)`: increment for every accepted row.
   - `COUNT(expr)`: increment when the argument result is not `NULL`.
   - `SUM(expr)`: convert non-`NULL` arguments to the aggregate numeric domain
     and add them.
   - `AVG(expr)`: maintain sum and non-`NULL` count, then divide during
     finalization.
   - `MIN(expr)` / `MAX(expr)`: compare non-`NULL` arguments using the
     expression's MySQL comparison domain.
6. Finalize aggregate values. Empty or all-`NULL` groups produce `NULL` for
   `SUM`, `AVG`, `MIN`, and `MAX`; `COUNT` produces `0`.
7. Evaluate select-list and `HAVING` group expressions against finalized group
   state. `HAVING` false or `NULL` removes the group from the result.
8. Apply `ORDER BY` and `LIMIT` after `HAVING`, preserving visible result
   metadata.

Nonaggregate table-backed SELECT evaluates `HAVING` as a row predicate after
`WHERE` and before `DISTINCT`, hidden `ORDER BY` evaluation, sorting, and
limiting. The joined-row path applies the same ordering after join `ON` and
`WHERE` predicates have accepted a row.

For the first slice, a hash or sorted in-memory group table is acceptable. The
implementation should keep the data structure narrow: grouping keys, aggregate
state slots, warning state, and enough representative row/group data to
evaluate permitted group-invariant expressions. Large-result spill behavior can
be deferred, but the design should avoid making that hard to add.

## Aggregate result rules

### `COUNT`

- `COUNT(*)` has no argument expression and counts accepted rows.
- `COUNT(expr)` evaluates `expr` per accepted row and counts non-`NULL` results.
- Result type is MySQL `LONGLONG`, length `21`, decimals `0`, not nullable.
- `COUNT()` and `COUNT(*, expr)` are syntax errors in MySQL.
- `COUNT(DISTINCT expr [, expr ...])` counts distinct non-`NULL` argument
  tuples in the currently supported aggregate execution surfaces. See
  `docs/specs/count-distinct-aggregate/specs.md`.

### `SUM`

- Ignores `NULL` argument results.
- Returns `NULL` when no non-`NULL` argument exists.
- For integer and decimal input, MySQL reports a `NEWDECIMAL` result in the
  verified cases.
- String input is converted numerically and may emit truncation warnings.
- Exact overflow, unsigned, approximate, temporal, and bit-string behavior is
  deferred unless covered by the existing expression conversion machinery.

### `AVG`

- Ignores `NULL` argument results.
- Returns `NULL` when no non-`NULL` argument exists.
- Maintains both numeric sum and count; do not compute by averaging row by row.
- For integer and decimal input, MySQL reports `NEWDECIMAL` with scale widened
  from `SUM`; verified examples were `AVG(INT)` decimals `4` and
  `AVG(DECIMAL(10,2))` decimals `6`.
- String input follows numeric conversion and warning rules.

### `MIN` and `MAX`

- Ignore `NULL` argument results.
- Return `NULL` when no non-`NULL` argument exists.
- Preserve the argument expression's result type and descriptor where practical.
- String arguments use string comparison under the expression collation.
- Numeric arguments use numeric comparison.
- Enum/set comparison, full collation coercion, JSON, temporal, and binary
  edge cases are deferred.

## Diagnostics and warnings

Implementation tests should assert numeric error codes and SQLSTATE where the
current diagnostic layer can expose them. At minimum, the analyzer should
classify these cases:

| Case | MySQL 8.4.9 behavior |
| --- | --- |
| aggregate in `WHERE` | error 1111 / `HY000`, invalid use of group function |
| missing grouping column | error 1054 / `42S22`, unknown column in `group statement` |
| invalid grouping ordinal | error 1054 / `42S22`, unknown column in `group statement` |
| unsafe nonaggregate with `GROUP BY` | error 1055 / `42000` |
| unsafe nonaggregate in implicit aggregate query | error 1140 / `42000` |
| ambiguous `GROUP BY` reference | warning 1052 in `group statement` when MySQL emits it |
| ambiguous `HAVING` reference | warning 1052 in `having clause` when MySQL emits it |
| duplicate `GROUP BY` output label | error 1052 in `group statement` |
| duplicate `HAVING` output label | error 1052 in `having clause` |
| numeric conversion in `SUM` / `AVG` | warning 1292 for truncated numeric conversion |
| unsupported aggregate syntax accepted by parser | deterministic unsupported-feature diagnostic until implemented |

Warnings should survive successful statements and failed statements in the same
way as existing expression and `WHERE` warnings. The statement warning list
should include aggregate argument warnings in MySQL evaluation order where
covered by tests.

## MySQL-verified implementation tests

The first implementation should add MySQL comparison tests for at least these
expectations:

| Category | SQL | Expected behavior |
| --- | --- | --- |
| implicit aggregate | `SELECT COUNT(*), COUNT(n), SUM(n), AVG(n), MIN(n), MAX(n) FROM t` | one row: `5`, `4`, `37`, `9.2500`, `0`, `20` |
| empty input | `SELECT COUNT(*), COUNT(n), SUM(n), AVG(n), MIN(n), MAX(n) FROM empty_t` | one row: `0`, `0`, `NULL`, `NULL`, `NULL`, `NULL` |
| grouped rows | `SELECT grp, COUNT(*), COUNT(n), SUM(n), AVG(n), MIN(txt), MAX(txt) FROM t GROUP BY grp ORDER BY grp IS NULL, grp` | three rows matching the verified grouped result table above |
| grouped string aggregation | `SELECT grp, GROUP_CONCAT(txt ORDER BY n SEPARATOR '|') FROM t GROUP BY grp ORDER BY grp IS NULL, grp` | grouped concatenated strings matching `docs/specs/group-concat-function/specs.md` |
| alias grouping | `SELECT grp AS g, SUM(n) AS total FROM t GROUP BY g HAVING total > 10 ORDER BY g` | one row: `('a',30)` |
| ordinal grouping | `SELECT grp, SUM(n) AS total FROM t GROUP BY 1 HAVING SUM(n) >= 10 ORDER BY 2 DESC` | one row: `('a',30)` |
| `WHERE` before grouping | `SELECT grp, SUM(n) FROM t WHERE n IS NOT NULL GROUP BY grp HAVING SUM(n) > 7 ORDER BY grp IS NULL, grp` | one row: `('a',30)` |
| `HAVING` without rows | `SELECT COUNT(*) AS c FROM t WHERE id > 10 HAVING c = 0` | one row: `0` |
| `HAVING` removes implicit group | `SELECT COUNT(*) AS c FROM t WHERE id > 10 HAVING c > 0` | empty result |
| nonaggregate `HAVING` over label | `SELECT id, n FROM t HAVING n > 0 ORDER BY id` | rows where projected `n` is positive |
| nonaggregate `HAVING` hidden column | `SELECT id FROM t HAVING n > 0` | error 1054 / `42S22`, hidden `n` is unknown |
| duplicate `HAVING` label | `SELECT n AS x, id AS x FROM t HAVING x = 10` | error 1052 / `23000`, ambiguous `x` in `having clause` |
| duplicate `GROUP BY` label | `SELECT n AS x, id AS x, COUNT(*) AS c FROM t GROUP BY x` | error 1052 / `23000`, ambiguous `x` in `group statement` |
| no-table aggregate | `SELECT COUNT(*), COUNT(NULL), COUNT(1), SUM(1), AVG(1), MIN(1), MAX(1)` | one row: `1`, `0`, `1`, `1`, `1.0000`, `1`, `1` |
| conversion warnings | `SELECT SUM(s), AVG(s), MIN(s), MAX(s) FROM conv_t` | `12.5`, `4.166666666666667`, `10`, `bad`; four 1292 warnings |
| aggregate in `WHERE` | `SELECT COUNT(*) FROM t WHERE COUNT(*) > 0` | error 1111 / `HY000` |
| unsafe grouped select | `SELECT grp, n, SUM(n) FROM t GROUP BY grp` | error 1055 / `42000` |
| unsafe implicit aggregate | `SELECT name, MAX(age) FROM fd_t` | error 1140 / `42000` |
| deferred primary-key dependence | `SELECT id, name, MAX(age) FROM fd_t GROUP BY id` | MySQL returns `(1,'ann',10)`, `(2,'bob',20)`; MyLite first slice rejects with 1055 until functional-dependence proof is implemented |
| unresolved group key | `SELECT COUNT(*) FROM t GROUP BY missing_col` | error 1054 / `42S22` |
| invalid aggregate arity | `SELECT COUNT() FROM t` | syntax error 1064 / `42000` |
| count distinct aggregate | `SELECT COUNT(DISTINCT grp) FROM t` | one row: `2` |
| ambiguous alias | `SELECT COUNT(col1) AS col2 FROM alias_t GROUP BY col2 HAVING col2 = 2` | result `2`; warnings 1052 for group and having ambiguity |
| metadata | `SELECT COUNT(*), SUM(n), AVG(n), SUM(decv), AVG(decv), MIN(n), MAX(txt) FROM t` | descriptors match the metadata table above where MyLite exposes fields |

Tests should also assert that result descriptors stay available for empty
aggregate results and `HAVING`-filtered empty result sets.

## Compatibility decisions

- Keep `ONLY_FULL_GROUP_BY` enabled by default, matching the current MySQL 8.4.9
  probe mode.
- Prefer rejection over nondeterministic row choice when functional dependence
  cannot yet be proven; primary-key and unique-key proofs are deferred from
  this first implementation slice.
- Implement aggregate execution in MyLite so diagnostics, warnings, metadata,
  and type conversion remain MySQL-facing rather than SQLite-facing.
- Implement `COUNT(DISTINCT)` through MyLite-owned aggregate state, using the
  current duplicate comparison semantics already shared with top-level
  `SELECT DISTINCT`.
- Defer `WITH ROLLUP` and `GROUPING()` as a separate grouping modifier feature.
- Do not expose SQLite aggregate names or SQLite permissive grouping behavior
  through the MyLite SQL surface.

## Implementation risks

- `ONLY_FULL_GROUP_BY` is easy to under-enforce. The analyzer needs a clear
  expression classifier and a conservative functional-dependence proof.
- Alias resolution differs between `ORDER BY` and `GROUP BY` / `HAVING`; sharing
  the current ordering resolver without a clause-specific policy would produce
  wrong results and miss MySQL ambiguity warnings.
- SQLite aggregate delegation would leak SQLite type and grouping behavior.
  MyLite should own aggregate state even if SQLite assists with scanning.
- Aggregate metadata differs from scalar expression metadata: aggregate
  expressions have no origin fields, `COUNT` is non-nullable, and `SUM` / `AVG`
  widen numeric descriptors.
- Conversion warnings can occur per aggregate expression, not merely per input
  row. A shared converted argument cache could undercount warnings unless it is
  designed around MySQL behavior.
- Functional dependence requires reliable index metadata, including uniqueness
  and nullability. Partial metadata support should be reflected in analyzer
  limitations and tests.
- Grouping key equality must use MySQL comparison and collation rules, not raw
  SQLite value equality, especially for strings and numeric/string mixtures.
