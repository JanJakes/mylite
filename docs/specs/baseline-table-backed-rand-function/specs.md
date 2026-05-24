# Baseline Table-Backed RAND Function

## Goal

Extend the existing `RAND()` / `RAND(seed)` scalar baseline to the narrow
single-table `SELECT` paths that real applications commonly use for random
projection and random ordering. This phase keeps MyLite's descriptor-owned SQL
pipeline intact and uses a private SQLite scalar hook only as an execution
primitive for per-row evaluation.

## Compatibility Sources

- Official MySQL 8.4 Reference Manual, "Mathematical Functions",
  `RAND([N])`: https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html
- Observed MySQL 8.4.9 runtime behavior in the local `mysql:8.4.9` container.

The MySQL manual states that `RAND()` returns a floating-point value in the
range `0 <= value < 1.0`, a constant seed initializes the sequence once per
statement, and `ORDER BY RAND()` is the random-order idiom. Runtime probes on
MySQL 8.4.9 confirm the exact first table-backed seeded sequences used by this
slice:

```sql
CREATE TABLE t(id INT, k INT NULL);
INSERT INTO t VALUES (1, NULL), (2, 2), (3, 2), (4, 4), (5, NULL);
SELECT id, RAND(1) AS r FROM t ORDER BY id;
```

returns:

```text
1  0.40540353712197724
2  0.8716141803857071
3  0.1418603212962489
4  0.09445909605776807
5  0.04671454713373868
```

and:

```sql
SELECT id FROM t ORDER BY RAND(1);
```

returns:

```text
5
4
3
1
2
```

Two separate `RAND(1)` expressions in the same select list produce the same
per-row sequence independently:

```sql
SELECT id, RAND(1) AS a, RAND(1) AS b FROM t ORDER BY id;
```

returns identical `a` and `b` values per row.

## Supported Syntax

This phase admits only already-parsed `RAND()` expressions in single-table
`SELECT` projection items and single-key `SELECT ... ORDER BY` clauses:

```sql
SELECT RAND() FROM table_name [WHERE predicate] [ORDER BY order_key] [LIMIT limit]
SELECT RAND(seed_literal) FROM table_name [WHERE predicate] [ORDER BY order_key] [LIMIT limit]
SELECT column_list FROM table_name [WHERE predicate] ORDER BY RAND() [ASC|DESC] [LIMIT limit]
SELECT column_list FROM table_name [WHERE predicate] ORDER BY RAND(seed_literal) [ASC|DESC] [LIMIT limit]
```

The existing no-source, `DUAL`, and `DO` `RAND()` / `RAND(seed)` forms remain
unchanged.

`seed_literal` is the same literal seed domain as the existing scalar baseline:
decimal integer literals with optional unary sign, `TRUE`, `FALSE`, and `NULL`.
The seed is converted to an unsigned 32-bit value by wrapping accepted unsigned
64-bit magnitudes, with negative integer literals converted by two's-complement
wrap like the existing scalar implementation. Other seed expressions are still
rejected deterministically.

## MyLite Grammar Snippet

The existing expression grammar remains the source of `RAND()` parsing. This
feature only widens `SELECT ORDER BY` keys:

```lemon
select_order_key(A) ::= qualified_identifier(K). { A = K; }
select_order_key(A) ::= select_field_order_expression(K). { A = K; }
select_order_key(A) ::= select_rand_order_expression(K). { A = K; }

select_rand_order_expression(A) ::= RAND(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_RAND_FUNCTION, R);
}
select_rand_order_expression(A) ::= RAND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RAND_SEED_FUNCTION, B, R);
}
select_rand_order_expression(A) ::= RAND(T) LPAREN expression(B)
    COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR, C, R);
}
select_rand_order_expression(A) ::= LPAREN(L) select_rand_order_expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
```

`table_order_clause_opt` used by `UPDATE`, `DELETE`, and DDL remains descriptor
column-only in this phase.

## Semantics

- `RAND()` is evaluated once per selected row when it appears in a table-backed
  projection or order key. Values are in `[0, 1)`.
- `RAND(seed_literal)` initializes one sequence per statement expression and
  advances the sequence once per invocation. Each syntactically separate
  expression has independent state, so two `RAND(1)` projection items produce
  identical sequences.
- `RAND(NULL)` uses seed `0`; `RAND(TRUE)` uses seed `1`; `RAND(FALSE)` uses
  seed `0`.
- `ORDER BY RAND()` and `ORDER BY RAND(seed_literal)` are accepted with default
  ascending order, explicit `ASC`, and explicit `DESC`.
- `ORDER BY RAND(seed_literal)` is deterministic for a given table scan order
  and seed. This phase verifies the visible order for the current stable
  physical rowid table baseline but does not claim broader optimizer stability.
- `ORDER BY RAND()` is intentionally nondeterministic. Tests assert row-set
  integrity and range behavior, not a fixed order.
- `ORDER BY RAND(...) LIMIT n` uses the existing descriptor-backed `SELECT`
  limit subset and lets SQLite apply the sort and limit.
- Multiple order keys involving `RAND()` are rejected in this phase.
- `RAND()` in `WHERE`, joins, grouped queries, aggregate arguments, DML
  assignments, generated columns, defaults, constraints, subqueries, arithmetic
  expressions, and nested function arguments remains outside this feature.

## Architecture

- Public API: no ABI change. Results continue to flow through `mylite_execute()`
  and existing `mylite_result` conventions.
- Parser/AST: existing `RAND()` expression nodes are reused. `SELECT ORDER BY`
  admits only a `RAND()` order expression in addition to current descriptor
  columns and `FIELD(...)`.
- Analyzer/planner: table-backed RAND projection is planned as a row-scalar
  expression. `ORDER BY RAND()` is planned as a new order item that owns a
  row-scalar expression. Descriptor table resolution, schema selection, index
  hints, predicates, and limits remain existing descriptor-driven paths.
- Runtime lowering: MyLite emits private SQLite scalar calls:
  `_mylite_rand()` and `_mylite_rand_seeded(?N)`. Seed literals are converted by
  MyLite before binding.
- SQLite physical execution: SQLite performs the table scan, predicate filter,
  sort, and limit. MyLite does not materialize rows for random ordering.
- SQLite integration: this uses public `sqlite3_create_function_v2()` and
  `sqlite3_set_auxdata()` / `sqlite3_get_auxdata()` for expression-instance
  seed state. No SQLite fork patch is required.
- Catalog/storage: no catalog rows, descriptor versions, descriptor caches,
  schema generation counters, `.mylite` preamble bytes, or SQLite file-format
  payload invariants change.

## Diagnostics

- `RAND()` argument-count errors continue to report MySQL-compatible
  `1582 / 42000` native-function parameter-count diagnostics.
- Unsupported seed expressions continue to report MyLite's existing
  deterministic unsupported-seed diagnostics.
- Seed magnitudes outside the existing unsigned-64 literal envelope continue to
  report the existing out-of-range diagnostic.
- `ORDER BY RAND()` with more than one order key reports a deterministic
  MyLite unsupported diagnostic for this limited slice.
- `ORDER BY RAND(column)`, `ORDER BY RAND(1 + 0)`, string/decimal/float/hex/bit
  seeds, parameters, functions, and subqueries report the existing unsupported
  seed diagnostics.
- Physical SQLite callback failures are mapped through the existing row-scalar
  physical error path unless a MyLite diagnostic has already been set.

## Tests

Add MySQL-runtime-verified expectation coverage for:

- table-backed `SELECT id, RAND() FROM t ORDER BY id`;
- table-backed deterministic `RAND(1)` and `RAND(3)` sequences;
- duplicate seeded projection expressions;
- `ORDER BY RAND(1)`, `ORDER BY RAND(1) LIMIT n`, and `ORDER BY RAND(1) DESC`;
- `ORDER BY RAND()` row-set integrity;
- seed literal wrapping inherited from scalar `RAND(seed)`;
- unsupported multiple random order keys and unsupported seed expressions.

Add fast C tests under `packages/libmylite/tests/` by extending the existing
RAND runtime test and expectation scripts. Cover result columns, warning count,
affected rows, no catalog/preamble mutation, reopen persistence of table data,
and preservation of existing unsupported nested-expression cases.

## Out Of Scope

Column-valued seeds, arbitrary seed expressions, parameters, random predicates,
random DML assignments, `ORDER BY RAND()` for joined/grouped/compound queries,
multiple random order keys, expression metadata beyond the existing scalar
DOUBLE result shape, random integer helper expressions, replication safety
warnings, cryptographic randomness, and SQLite fork changes.
