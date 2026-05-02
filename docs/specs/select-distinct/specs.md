# SELECT DISTINCT and DISTINCTROW

## Scope

Task 28 specifies MySQL-compatible duplicate elimination for the `SELECT`
surfaces already made executable by Tasks 15 through 27. It adds the duplicate
mode that appears immediately after `SELECT`; it does not broaden expression,
row-source, aggregate, or set-operation support.

In scope:

- `SELECT DISTINCT select_list ...`
- `SELECT DISTINCTROW select_list ...`, as a synonym for `DISTINCT`
- `SELECT ALL select_list ...`, as the explicit form of the default duplicate
  mode
- repeated same-kind duplicate modifiers that MySQL accepts, such as
  `SELECT DISTINCT DISTINCT ...`, `SELECT DISTINCT DISTINCTROW ...`, and
  `SELECT ALL ALL ...`
- MySQL-compatible diagnostics for mixed `ALL` and `DISTINCT`/`DISTINCTROW`
  modifiers
- no-table scalar `SELECT` over the existing supported expression subset
- table-backed `SELECT` over the existing supported base-table, `WHERE`,
  projection, alias, wildcard, join, aggregate, `ORDER BY`, `LIMIT`, and
  metadata surfaces
- duplicate comparison across the complete visible projection tuple after
  wildcard expansion and expression evaluation
- `NULL` duplicate behavior, multi-column duplicate keys, expression output,
  aliases, and result metadata preservation
- string duplicate comparison through the existing character-set and collation
  foundation, including case-insensitive and binary collations already in
  scope
- interaction with `ORDER BY` hidden expressions, ambiguity diagnostics, and
  deterministic `LIMIT` tests

Out of scope:

- `COUNT(DISTINCT ...)`, `SUM(DISTINCT ...)`, `AVG(DISTINCT ...)`, and
  aggregate-function-local `DISTINCT`
- `GROUP_CONCAT(DISTINCT ...)`
- window functions and aggregate `OVER (...)` clauses
- `UNION`, `INTERSECT`, `EXCEPT`, `TABLE`, `VALUES`, and their `ALL` or
  `DISTINCT` modes
- derived tables, CTEs, subqueries, views, table functions, lateral references,
  and parenthesized query expressions
- `GROUP BY ... WITH ROLLUP` and `DISTINCT` with rollup, because rollup is not
  implemented yet
- optimizer modifiers such as `SQL_SMALL_RESULT`, `SQL_BIG_RESULT`,
  `SQL_BUFFER_RESULT`, and `SQL_CALC_FOUND_ROWS`
- optimizer-specific choices, temporary-table implementation details, stable
  tie ordering when the query lacks a complete `ORDER BY`, and early-stop
  execution behavior beyond externally visible rows, errors, warnings, and
  metadata
- protocol packet metadata beyond the current result descriptor surface

This task must not mark `COUNT(DISTINCT ...)`, set-operation `DISTINCT`, or any
deferred row-source surface as supported.

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, `DISTINCT` optimization:
  https://dev.mysql.com/doc/refman/8.4/en/distinct-optimization.html
- MySQL 8.4 Reference Manual, MySQL Handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4 Reference Manual, Collation Coercibility in Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/charset-collation-coercibility.html
- MySQL 8.4 Reference Manual, Aggregate Function Descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/case-expression/specs.md`
  - `docs/specs/aggregate-grouping/specs.md`
  - `docs/specs/inner-joins/specs.md`
  - `docs/specs/outer-joins/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --password= --protocol=TCP --table --show-warnings`
- `docker exec -i mylite-mysql-849 mysql -uroot --password= --protocol=TCP --force --table --show-warnings`
- `docker exec -i mylite-mysql-849 mysql -uroot --password= --protocol=TCP --column-type-info -vvv`

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
DROP DATABASE IF EXISTS mylite_task28;
CREATE DATABASE mylite_task28 CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE mylite_task28;

CREATE TABLE d (
  id INT PRIMARY KEY AUTO_INCREMENT,
  a INT NULL,
  b VARCHAR(10) COLLATE utf8mb4_0900_ai_ci NULL,
  c VARCHAR(10) COLLATE utf8mb4_bin NULL,
  sort_key INT NOT NULL
);

INSERT INTO d (a,b,c,sort_key) VALUES
  (1,'x','x',30),
  (1,'x','x',10),
  (2,'y','y',20),
  (NULL,'n','n',40),
  (NULL,'n','n',50),
  (3,'A','A',60),
  (3,'a','a',70),
  (4,NULL,NULL,80),
  (4,NULL,NULL,90);

CREATE TABLE e (
  id INT PRIMARY KEY AUTO_INCREMENT,
  d_a INT,
  tag VARCHAR(10)
);

INSERT INTO e (d_a, tag) VALUES
  (1,'one'),
  (1,'one'),
  (1,'two'),
  (2,'two'),
  (NULL,'nil');
```

### Duplicate mode modifiers

`ALL` is the explicit form of the default behavior: duplicates are returned.
`DISTINCT` removes duplicate rows from the visible result set. `DISTINCTROW`
has the same result as `DISTINCT`.

Representative runtime results:

| SQL | Ordered result |
| --- | --- |
| `SELECT DISTINCT a FROM d ORDER BY a IS NULL, a` | `1`, `2`, `3`, `4`, `NULL` |
| `SELECT DISTINCTROW a FROM d ORDER BY a IS NULL, a` | `1`, `2`, `3`, `4`, `NULL` |
| `SELECT ALL a FROM d ORDER BY id` | `1`, `1`, `2`, `NULL`, `NULL`, `3`, `3`, `4`, `4` |

MySQL accepts repeated modifiers when they all request the same mode:

| SQL | Behavior |
| --- | --- |
| `SELECT DISTINCT DISTINCT a FROM d` | accepted; behaves as `DISTINCT` |
| `SELECT DISTINCT DISTINCTROW a FROM d` | accepted; behaves as `DISTINCT` |
| `SELECT ALL ALL a FROM d LIMIT 2` | accepted; behaves as `ALL` |

Mixed `ALL` and `DISTINCT` modes fail during statement validation:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT ALL DISTINCT a FROM d` | error 1221 / `HY000`, incorrect usage of `ALL` and `DISTINCT` |
| `SELECT DISTINCT ALL a FROM d` | error 1221 / `HY000`, incorrect usage of `ALL` and `DISTINCT` |
| `SELECT ALL DISTINCTROW a FROM d` | error 1221 / `HY000`, incorrect usage of `ALL` and `DISTINCT` |

### Duplicate identity

Duplicate elimination compares the complete projected row after expression
evaluation and wildcard expansion. Aliases do not participate in row identity.
For multi-column output, two rows are duplicates only when every output column
compares equal under that output expression's equality semantics.

Representative runtime results:

| SQL | Ordered result |
| --- | --- |
| `SELECT DISTINCT a,b FROM d WHERE a IS NULL OR a=4 ORDER BY a IS NULL, a, b IS NULL, b` | `(4,NULL)`, `(NULL,'n')` |
| `SELECT DISTINCT a,b FROM d ORDER BY a IS NULL, a, b` | `(1,'x')`, `(2,'y')`, `(3,'A')`, `(4,NULL)`, `(NULL,'n')` |
| `SELECT DISTINCT 1 AS one FROM d` | one row: `1` |

For duplicate elimination, `NULL` values collapse like equal duplicate keys.
This differs from ordinary `=` comparison, where `NULL = NULL` evaluates to
`NULL`.

### Collation and type behavior

String duplicate comparison follows the resolved collation of each projected
string expression. Under `utf8mb4_0900_ai_ci`, `A` and `a` compare equal for
duplicate elimination. Under `utf8mb4_bin`, they remain distinct.

Representative runtime results:

| SQL | Ordered result |
| --- | --- |
| `SELECT DISTINCT b FROM d WHERE a=3 ORDER BY b` | one row: `'A'` |
| `SELECT DISTINCT c FROM d WHERE a=3 ORDER BY c` | `'A'`, `'a'` |

Expression output is evaluated before duplicate comparison. The duplicate key
uses the expression result's value, type, and resolved collation rather than the
source column tuple:

| SQL | Ordered result |
| --- | --- |
| `SELECT DISTINCT a + 1 AS plus_one FROM d WHERE a IS NOT NULL ORDER BY plus_one` | `2`, `3`, `4`, `5` |

MyLite should reuse the Task 16, Task 24, Task 25, and charset/collation
evaluation descriptors so duplicate keys compare the same values that are
exposed to result metadata and `ORDER BY`.

### Metadata, aliases, and wildcards

`DISTINCT` changes row cardinality only. It does not change result labels,
origin metadata, field types, lengths, flags, decimals, collation ids, or
nullability.

Observed metadata with `SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci`:

```sql
SELECT DISTINCT a AS alias_a, b AS label_b, a + 1 AS plus_one
FROM d
ORDER BY alias_a IS NULL, alias_a, label_b
LIMIT 3;
```

The result columns are:

| Column | Metadata expectation |
| --- | --- |
| `alias_a` | `LONG`, origin `mylite_task28.d.a`, binary collation id `63` |
| `label_b` | `VAR_STRING`, origin `mylite_task28.d.b`, `utf8mb4_0900_ai_ci` collation id `255` |
| `plus_one` | `LONGLONG`, empty origin metadata, binary collation id `63` |

Wildcard expansion happens before duplicate elimination. Therefore
`SELECT DISTINCT * FROM d` over the fixture keeps rows with different `id` or
`sort_key` values even when `a`, `b`, and `c` match.

### ORDER BY and LIMIT

With `DISTINCT`, `ORDER BY` still uses the Task 18 name-resolution rules:
projection aliases are visible, qualified table-column references bind to base
columns, and hidden sort expressions do not become result columns.

`ONLY_FULL_GROUP_BY` also affects `DISTINCT` with `ORDER BY`. MySQL rejects an
`ORDER BY` expression if it is not equal to a selected expression and references
a selected-table column that is not present in the select list. This prevents a
hidden non-selected value from making the retained duplicate row arbitrary.

Representative runtime observations:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT DISTINCT a AS alias_a FROM d ORDER BY alias_a` | accepted; ordered `NULL`, `1`, `2`, `3`, `4` |
| `SELECT DISTINCT a AS b FROM d ORDER BY (b)` where `d` also has column `b` | accepted; the parenthesized top-level alias remains visible |
| `SELECT DISTINCT a FROM d ORDER BY a + 0` | accepted because the hidden expression references selected column `a` |
| `SELECT DISTINCT a AS b FROM d ORDER BY b + 0` where `d` also has column `b` | error 3065 / `HY000`; inside the compound expression `b` resolves to the hidden base column |
| `SELECT DISTINCT a FROM d ORDER BY sort_key` | error 3065 / `HY000`, order expression is incompatible with `DISTINCT` |
| `SELECT DISTINCT a FROM d ORDER BY sort_key + 0` | error 3065 / `HY000`, order expression is incompatible with `DISTINCT` |

`LIMIT` applies to the duplicate-eliminated, ordered result when `ORDER BY` is
present. Without a complete `ORDER BY`, MySQL may return unique rows in an
execution-dependent order; tests must not depend on that order.

Representative runtime results:

| SQL | Ordered result |
| --- | --- |
| `SELECT DISTINCT a FROM d ORDER BY a LIMIT 3` | `NULL`, `1`, `2` |
| `SELECT DISTINCT a FROM d ORDER BY a DESC LIMIT 3` | `4`, `3`, `2` |

### Joins

For joined row sources, duplicate elimination runs after join row production,
`ON`/`USING` handling, outer-join null extension, and `WHERE`, and before final
`ORDER BY`/`LIMIT` output. The duplicate key is still the visible projection
tuple, not the underlying joined base rows.

Representative runtime result:

| SQL | Ordered result |
| --- | --- |
| `SELECT DISTINCT d.a, e.tag FROM d JOIN e ON d.a <=> e.d_a ORDER BY d.a, e.tag` | `(NULL,'nil')`, `(1,'one')`, `(1,'two')`, `(2,'two')` |

The two duplicate `e` rows with tag `'one'` collapse after joining to both
matching `d.a = 1` rows, while the distinct tag `'two'` remains.

### Aggregates and grouping

Top-level `SELECT DISTINCT` applies to rows produced by aggregate evaluation.
Without `GROUP BY`, an aggregate query produces at most one row, so top-level
`DISTINCT` is normally cardinality-neutral. With `GROUP BY`, it can remove
duplicate aggregate output rows when groups project to the same tuple.

Representative runtime results:

| SQL | Ordered result |
| --- | --- |
| `SELECT DISTINCT COUNT(*) AS n FROM d` | one row: `9` |
| `SELECT DISTINCT a, COUNT(*) AS n FROM d GROUP BY a ORDER BY a IS NULL, a` | `(1,2)`, `(2,1)`, `(3,2)`, `(4,2)`, `(NULL,2)` |

Aggregate-local `DISTINCT` is a separate feature surface. MySQL supports it for
some aggregates, but MyLite must leave it deferred for this task:

| SQL | MySQL 8.4.9 result | MyLite Task 28 status |
| --- | --- | --- |
| `SELECT COUNT(DISTINCT a) AS distinct_nonnull_a FROM d` | `4` | deferred to aggregate-local `DISTINCT` work |

## Parser and AST design

The parser should preserve duplicate-mode tokens after `SELECT` instead of
folding them away during lexing. Statement validation can then distinguish
accepted repeated same-kind modifiers from mixed-mode diagnostics.

Recommended AST representation:

- `MYLITE_SQL_SELECT_DUPLICATES_IMPLICIT_ALL`
- `MYLITE_SQL_SELECT_DUPLICATES_ALL`
- `MYLITE_SQL_SELECT_DUPLICATES_DISTINCT`
- a boolean or count flag recording whether the mode was explicit
- source spans for each duplicate-mode token, used for diagnostics and tests

`DISTINCTROW` should normalize to the `DISTINCT` duplicate mode while retaining
the original token kind for source spans and parser tests.

### MyLite Lemon-style grammar snippets

These snippets describe the intended MyLite grammar shape. They are
independently authored for MyLite and are not copied from MySQL grammar.

```lemon
select_statement(A) ::= SELECT select_duplicate_mode(M) select_list(L)
                        opt_from_clause(F) opt_where_clause(W)
                        opt_group_by_clause(G) opt_having_clause(H)
                        opt_order_by_clause(O) opt_limit_clause(N). {
    A = mylite_sql_ast_select_new(M, L, F, W, G, H, O, N);
}

select_duplicate_mode(A) ::= . {
    A = mylite_sql_select_duplicate_mode_implicit_all();
}

select_duplicate_mode(A) ::= select_duplicate_mode_list(L). {
    A = mylite_sql_select_duplicate_mode_from_list(L);
}

select_duplicate_mode_list(A) ::= select_duplicate_mode_item(I). {
    A = mylite_sql_select_duplicate_mode_list_new(I);
}

select_duplicate_mode_list(A) ::= select_duplicate_mode_list(L)
                                  select_duplicate_mode_item(I). {
    A = mylite_sql_select_duplicate_mode_list_append(L, I);
}

select_duplicate_mode_item(A) ::= ALL(T). {
    A = mylite_sql_select_duplicate_mode_item_all(T);
}

select_duplicate_mode_item(A) ::= DISTINCT(T). {
    A = mylite_sql_select_duplicate_mode_item_distinct(T);
}

select_duplicate_mode_item(A) ::= DISTINCTROW(T). {
    A = mylite_sql_select_duplicate_mode_item_distinctrow(T);
}
```

The normalizer should accept an empty list as implicit `ALL`, a list containing
only `ALL` tokens as explicit `ALL`, and a list containing only `DISTINCT` or
`DISTINCTROW` tokens as `DISTINCT`. A list containing both `ALL` and a
distinct-kind token must be rejected with error 1221.

## Analyzer and runtime design

Analysis requirements:

- Normalize duplicate mode before planning.
- Reject mixed `ALL` and `DISTINCT`/`DISTINCTROW` modifiers with MySQL error
  1221 / SQLSTATE `HY000`.
- Preserve all result-column descriptors from the projection analyzer.
- Validate `DISTINCT` plus `ORDER BY` after order expressions are bound:
  - accept an order expression equal to a select-list expression
  - accept an order expression whose selected-table column references are all
    present as select-list elements
  - reject an order expression that references a selected-table column absent
    from the select list with error 3065 / SQLSTATE `HY000`
- Keep hidden `ORDER BY` expressions internal; they must not affect visible
  metadata or duplicate keys.

Runtime requirements:

- For `ALL`, use the existing row path unchanged.
- For `DISTINCT`, evaluate rows through the existing projection path, then
  compare the complete visible output tuple using MySQL-compatible value,
  type, `NULL`, and collation semantics.
- Preserve the statement warning list produced by expression, predicate,
  aggregate, and order-key evaluation.
- Avoid relying on SQLite `SELECT DISTINCT` unless every involved type,
  collation, `NULL`, warning, metadata, and order restriction has already been
  proven equivalent for the current MyLite surface.
- A simple initial implementation may materialize unique result tuples in a
  statement-owned structure keyed by MyLite values and collation-aware
  comparators. That keeps correctness isolated from SQLite collation gaps and
  allows later optimizer pushdown when compatibility tests prove it safe.
- `ORDER BY` and `LIMIT` should operate over the duplicate-eliminated logical
  output for user-visible semantics. If an implementation uses an optimized
  order-before-distinct path, it must still satisfy the 3065 validation rule and
  produce MySQL-equivalent rows.

Storage impact is limited to statement-owned temporary memory. This feature
does not change the `.mylite` file format, schema catalog, or SQLite offset
model.

## Diagnostics and warnings

| Condition | MySQL behavior | MyLite requirement |
| --- | --- | --- |
| Mixed `ALL` and `DISTINCT`/`DISTINCTROW` modifiers | error 1221 / `HY000` | same error and warning-list entry |
| `DISTINCT` with hidden non-selected `ORDER BY` column | error 3065 / `HY000` | same error and warning-list entry |
| Successful `DISTINCT`, `DISTINCTROW`, or `ALL` query | no duplicate-mode warning | preserve warnings from expressions, predicates, aggregates, and ordering only |

The 3065 diagnostic message should name the one-based `ORDER BY` expression
position and the offending column when the analyzer can do so. Tests may assert
error code and SQLSTATE first, then message text once MyLite diagnostics expose
full MySQL-compatible messages.

## Test plan

Add MySQL-runtime comparison tests for rows, metadata, errors, warning counts,
and statement side effects. The first executable slice should use the fixture
above or an equivalent fixture built through supported MyLite DDL/DML.

Core row tests:

| Case | SQL | Expected result |
| --- | --- | --- |
| single-table duplicates | `SELECT DISTINCT a FROM d ORDER BY a IS NULL, a` | `1`, `2`, `3`, `4`, `NULL` |
| `DISTINCTROW` synonym | `SELECT DISTINCTROW a FROM d ORDER BY a IS NULL, a` | `1`, `2`, `3`, `4`, `NULL` |
| explicit `ALL` | `SELECT ALL a FROM d ORDER BY id` | all nine fixture values in `id` order |
| `NULL` duplicate behavior | `SELECT DISTINCT a,b FROM d WHERE a IS NULL OR a=4 ORDER BY a IS NULL, a, b IS NULL, b` | `(4,NULL)`, `(NULL,'n')` |
| multiple columns | `SELECT DISTINCT a,b FROM d ORDER BY a IS NULL, a, b` | `(1,'x')`, `(2,'y')`, `(3,'A')`, `(4,NULL)`, `(NULL,'n')` |
| expression output | `SELECT DISTINCT a + 1 AS plus_one FROM d WHERE a IS NOT NULL ORDER BY plus_one` | `2`, `3`, `4`, `5` |
| alias ordering | `SELECT DISTINCT a AS alias_a FROM d ORDER BY alias_a` | `NULL`, `1`, `2`, `3`, `4` |
| parenthesized alias ordering | `SELECT DISTINCT a AS b FROM d ORDER BY (b)` where `d` also has column `b` | accepted; `NULL`, `1`, `2`, `3`, `4` |
| case-insensitive collation | `SELECT DISTINCT b FROM d WHERE a=3 ORDER BY b` | one row: `'A'` |
| binary collation | `SELECT DISTINCT c FROM d WHERE a=3 ORDER BY c` | `'A'`, `'a'` |
| selected-column hidden order expression | `SELECT DISTINCT a FROM d ORDER BY a + 0` | accepted; `NULL`, `1`, `2`, `3`, `4` |
| hidden non-selected order key | `SELECT DISTINCT a FROM d ORDER BY sort_key` | error 3065 / `HY000` |
| hidden non-selected order expression | `SELECT DISTINCT a FROM d ORDER BY sort_key + 0` | error 3065 / `HY000` |
| alias-shadowed hidden order expression | `SELECT DISTINCT a AS b FROM d ORDER BY b + 0` where `d` also has column `b` | error 3065 / `HY000` |
| ordered limit | `SELECT DISTINCT a FROM d ORDER BY a LIMIT 3` | `NULL`, `1`, `2` |
| descending ordered limit | `SELECT DISTINCT a FROM d ORDER BY a DESC LIMIT 3` | `4`, `3`, `2` |
| joins | `SELECT DISTINCT d.a, e.tag FROM d JOIN e ON d.a <=> e.d_a ORDER BY d.a, e.tag` | `(NULL,'nil')`, `(1,'one')`, `(1,'two')`, `(2,'two')` |
| outer joins | `SELECT DISTINCT d.a FROM d LEFT JOIN e ON d.a <=> e.d_a ORDER BY d.a IS NULL, d.a` | `1`, `2`, `3`, `4`, `NULL` |
| aggregate no group | `SELECT DISTINCT COUNT(*) AS n FROM d` | one row: `9` |
| aggregate duplicate collapse | `SELECT DISTINCT COUNT(*) AS n FROM d GROUP BY b ORDER BY n` | `1`, `2` |
| aggregate with group | `SELECT DISTINCT a, COUNT(*) AS n FROM d GROUP BY a ORDER BY a IS NULL, a` | `(1,2)`, `(2,1)`, `(3,2)`, `(4,2)`, `(NULL,2)` |

Modifier validation tests:

| SQL | Expected behavior |
| --- | --- |
| `SELECT DISTINCT DISTINCT a FROM d` | accepted; behaves as `DISTINCT` |
| `SELECT DISTINCT DISTINCTROW a FROM d` | accepted; behaves as `DISTINCT` |
| `SELECT ALL ALL a FROM d LIMIT 2` | accepted; behaves as `ALL` |
| `SELECT ALL DISTINCT a FROM d` | error 1221 / `HY000` |
| `SELECT DISTINCT ALL a FROM d` | error 1221 / `HY000` |
| `SELECT ALL DISTINCTROW a FROM d` | error 1221 / `HY000` |

Metadata tests:

| SQL | Expected metadata |
| --- | --- |
| `SELECT DISTINCT a AS alias_a, b AS label_b, a + 1 AS plus_one FROM d ORDER BY alias_a IS NULL, alias_a, label_b LIMIT 3` | same labels, origins, field types, lengths, decimals, flags, charset ids, and nullability as the non-`DISTINCT` query |
| `SELECT DISTINCT * FROM d WHERE a IN (1,2) ORDER BY id` | wildcard columns expand exactly as Task 15/23 specify; `DISTINCT` does not add or remove columns |
| `SELECT DISTINCT d.* FROM d JOIN e ON d.a <=> e.d_a WHERE d.a IN (1,2) ORDER BY d.id` | qualified wildcard metadata stays tied to table `d`; duplicate elimination only affects row cardinality |

Deferred-surface tests should assert that Task 28 does not accidentally accept
or claim support for:

- `COUNT(DISTINCT a)` and other aggregate-local `DISTINCT` modifiers
- `GROUP_CONCAT(DISTINCT ...)`
- set-operation `ALL`/`DISTINCT`
- `DISTINCT` in derived tables, CTEs, subqueries, views, `TABLE`, or `VALUES`
  once those syntactic surfaces are independently introduced
- `DISTINCT` with `ROLLUP` until rollup support exists

## Compatibility status

Task 28 is implemented for the scoped no-table scalar and table-backed SELECT
surfaces. MyLite still defers aggregate-local `DISTINCT`, set-operation
duplicate modes, derived tables, subqueries, rollup, and other row-source
surfaces that this specification explicitly leaves out of scope.
