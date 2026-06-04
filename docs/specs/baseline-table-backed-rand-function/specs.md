# Baseline Table-Backed RAND Function

## Goal

Extend the existing `RAND()` / `RAND(seed)` scalar baseline to the narrow
single-table paths that real applications commonly use for random projection,
random ordering, simple random predicates, and WordPress-style DML assignment
values. This phase keeps MyLite's descriptor-owned SQL pipeline intact and uses
private SQLite scalar hooks only as execution primitives for row-scalar
evaluation.

## Compatibility Sources

- Official MySQL 8.4 Reference Manual, "Mathematical Functions",
  `RAND([N])`: https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html
- Observed MySQL 8.4.9 runtime behavior in the local `mysql:8.4.9` container.

The MySQL manual states that `RAND()` returns a floating-point value in the
range `0 <= value < 1.0`, a constant seed initializes the sequence once per
statement, a nonconstant seed reinitializes per invocation, `RAND()` in `WHERE`
is row-evaluated, and `ORDER BY RAND()` is the random-order idiom. Runtime
probes on MySQL 8.4.9 confirm the exact first table-backed seeded sequences used
by this slice:

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

Descriptor-derived seeds are one-shot per row:

```sql
SELECT id, RAND(CAST(k AS SIGNED)) FROM t ORDER BY id;
```

returns:

```text
1  0.15522042769493574
2  0.6555866465490187
3  0.6555866465490187
4  0.15595286540310166
5  0.15522042769493574
```

The same runtime probe confirms that `WHERE RAND() < 2` matches all five rows,
`WHERE RAND() > 2` matches none, and assigning `RAND(1)` to a `TEXT` column
stores `0.40540353712197724`.

## Supported Syntax

This phase admits `RAND()` expressions in single-table `SELECT` projection
items, single-key `SELECT ... ORDER BY` clauses, limited `WHERE` comparisons,
and nonbinary string DML values:

```sql
SELECT RAND() FROM table_name [WHERE predicate] [ORDER BY order_key] [LIMIT limit]
SELECT RAND(seed_literal) FROM table_name [WHERE predicate] [ORDER BY order_key] [LIMIT limit]
SELECT RAND(CAST(column_name AS SIGNED)) FROM table_name [WHERE predicate] [ORDER BY order_key] [LIMIT limit]
SELECT column_list FROM table_name [WHERE predicate] ORDER BY RAND() [ASC|DESC] [LIMIT limit]
SELECT column_list FROM table_name [WHERE predicate] ORDER BY RAND(seed_literal) [ASC|DESC] [LIMIT limit]
SELECT COUNT(*) FROM table_name WHERE RAND() comparison_operator literal
INSERT INTO table_name (string_column) VALUES (RAND([seed]))
UPDATE table_name SET string_column = RAND([seed]) WHERE single_row_predicate
```

The existing no-source, `DUAL`, and `DO` `RAND()` / `RAND(seed)` forms are
managed by their own scalar baseline phases. The later no-source seed-coercion
baseline widens those scalar forms, but this table-backed phase keeps the
literal-only seed domain below.

`seed_literal` is the same literal seed domain as the existing scalar baseline:
decimal integer literals with optional unary sign, `TRUE`, `FALSE`, and `NULL`.
The seed is converted to an unsigned 32-bit value by wrapping accepted unsigned
64-bit magnitudes, with negative integer literals converted by two's-complement
wrap like the existing scalar implementation. Table-backed descriptor seeds are
limited to warning-free integer `CAST()` / `CONVERT()` expressions that reference
a source descriptor column. Other seed expressions are still rejected
deterministically.

## MyLite Grammar Snippet

The expression grammar owns a reusable `rand_expression` node. This feature
widens `SELECT ORDER BY` keys and admits the same node in limited DML values and
predicate comparisons:

```lemon
expression(A) ::= rand_expression(B). { A = B; }

rand_expression(A) ::= RAND(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_RAND_FUNCTION, R);
}
rand_expression(A) ::= RAND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RAND_SEED_FUNCTION, B, R);
}
rand_expression(A) ::= RAND(T) LPAREN expression(B)
    COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR, C, R);
}

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

predicate_atom(A) ::= rand_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}

insert_value(A) ::= rand_expression(B). { A = B; }
update_value(A) ::= rand_expression(B). { A = B; }
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
- `RAND(CAST(column AS SIGNED))` and `RAND(CONVERT(column, SIGNED))` reinitialize
  from the converted descriptor value for each row. `NULL` descriptor values use
  seed `0`.
- `WHERE RAND(...) comparison literal` is evaluated per row for the current
  single-table predicate path. This slice verifies literal comparisons such as
  `RAND() < 2` and `RAND() > 2`; it does not claim full expression predicate
  support.
- WordPress-style `RAND()` / `RAND(seed)` values assigned to `CHAR`, `VARCHAR`,
  and baseline `TEXT` family columns are evaluated through the scalar RAND path,
  converted to MySQL-shaped decimal text, and then passed through existing
  nonbinary string DML validation. This covers single-row UPDATE statements and
  INSERT row-value conversion, not full multi-row randomized DML parity.
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
- `RAND()` in joins, grouped queries, aggregate arguments, generated columns,
  defaults, constraints, subqueries, arithmetic expressions, nested function
  arguments, and non-string DML targets remains outside this feature.

## Architecture

- Public API: no ABI change. Results continue to flow through `mylite_execute()`
  and existing `mylite_result` conventions.
- Parser/AST: existing `RAND()` expression nodes are reused through a shared
  `rand_expression` production. `SELECT ORDER BY` admits only a `RAND()` order
  expression in addition to current descriptor columns and `FIELD(...)`.
- Analyzer/planner: table-backed RAND projection is planned as a row-scalar
  expression. `ORDER BY RAND()` is planned as a new order item that owns a
  row-scalar expression. Limited RAND predicates lower through the existing
  row-scalar comparison predicate node. WordPress-style string-target DML values
  reuse the existing scalar RAND evaluator and nonbinary string conversion
  helpers. Descriptor table resolution, schema selection, index hints, and
  limits remain existing descriptor-driven paths.
- Runtime lowering: MyLite emits private SQLite scalar calls:
  `_mylite_rand()`, `_mylite_rand_seeded(?N)`, and
  `_mylite_rand_seeded_once(row_seed_expression)`. Seed literals are converted by
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
- DML RAND values into non-string targets report a deterministic MyLite
  capability diagnostic until numeric, temporal, binary, and other target
  conversions are specified.

## Tests

Add MySQL-runtime-verified expectation coverage for:

- table-backed `SELECT id, RAND() FROM t ORDER BY id`;
- table-backed deterministic `RAND(1)` and `RAND(3)` sequences;
- duplicate seeded projection expressions;
- descriptor-seeded `RAND(CAST(column AS SIGNED))` projection;
- `WHERE RAND() < 2` and `WHERE RAND() > 2` predicate execution;
- `INSERT` and single-row `UPDATE` of `RAND(1)` into `TEXT`;
- `ORDER BY RAND(1)`, `ORDER BY RAND(1) LIMIT n`, and `ORDER BY RAND(1) DESC`;
- `ORDER BY RAND()` row-set integrity;
- seed literal wrapping inherited from scalar `RAND(seed)`;
- unsupported multiple random order keys and unsupported seed expressions.

Add fast C tests under `packages/libmylite/tests/` by extending the existing
RAND runtime test and expectation scripts. Cover result columns, warning count,
affected rows, no catalog/preamble mutation, reopen persistence of table data,
and preservation of existing unsupported nested-expression cases.

## Out Of Scope

Arbitrary seed expressions, warning-producing table-backed seed conversion,
parameters, broad random predicates, non-string DML targets, full multi-row
randomized DML parity, generated defaults, `ORDER BY RAND()` for
joined/grouped/compound queries, multiple random order keys, expression metadata
beyond the existing scalar DOUBLE result shape, random integer helper
expressions, replication safety warnings, cryptographic randomness, and SQLite
fork changes.
