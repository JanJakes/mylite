# Baseline SELECT ORDER BY Ordinals

## Status

This feature adds MySQL-compatible positional order keys for the currently
supported `SELECT` ordering envelope:

```sql
SELECT select_item [, select_item ...]
FROM table_source
[WHERE predicate]
ORDER BY positive_integer [ASC | DESC] [, ...]
[LIMIT ...]
```

The ordinal is a one-based selected-output position. This slice covers ordinary
descriptor-backed `SELECT` and the current table-backed row-scalar `SELECT`
path. It does not add `GROUP BY` ordinals, `TABLE ... ORDER BY` ordinals, DML
ordinals, compound-query global ordering, or general constant/expression order
keys.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline SELECT ORDER BY multiple columns:
  `docs/specs/baseline-select-order-by-multiple-columns/specs.md`
- Baseline row-scalar projection specifications under `docs/specs/`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against local MySQL 8.4.9 in the `mylite-mysql-849` container with the
documented MySQL 8.4 default `sql_mode`:

- selected output positions in `ORDER BY` are accepted and start at `1`;
- `ORDER BY 2, 1 DESC` sorts by the second selected output first, then by the
  first selected output descending for ties;
- `ASC` and `DESC` apply per ordinal key, and omitted direction means
  ascending;
- valid positional order keys do not increment `@@warning_count`, even though
  the MySQL manual marks column-position syntax as deprecated;
- `ORDER BY 0` fails with `1054 / 42S22`,
  `Unknown column '0' in 'order clause'`;
- an ordinal larger than the selected output count fails with `1054 / 42S22`,
  `Unknown column '<n>' in 'order clause'`;
- `SELECT DISTINCT selected_expr ORDER BY 1` is valid when the ordinal resolves
  to the selected output; and
- row-scalar selected expressions such as `CONCAT(...)` may be ordered by their
  selected output position.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_select_order_by_ordinals_expectations.sh`.

## Scope

The implementation must add:

- parser support for integer literals in ordinary `SELECT` `ORDER BY` keys;
- one-based selected-output ordinal resolution for descriptor-backed
  `SELECT`;
- descriptor-backed ordinal ordering through the existing descriptor order
  planner so current MySQL-compatible numeric, temporal, string, `BIT`, and
  `NULL` ordering behavior is preserved;
- ordinal resolution inside the current row-scalar table-backed `SELECT`
  planner, rendered as a validated SQLite positional order key so selected
  expressions are not planned or evaluated twice;
- comma-separated ordinal lists in the row-scalar path when all order keys are
  valid ordinals;
- optional `ASC` and `DESC` per ordinal key;
- composition with current `WHERE`, `DISTINCT` / `DISTINCTROW`, selected
  aliases, row-scalar expressions, and `LIMIT` support; and
- MySQL-compatible `1054 / 42S22` diagnostics for zero, out-of-range, or
  unparseable positive-integer position tokens in this supported subset.

## Non-Goals

This slice intentionally does not add:

- `GROUP BY` ordinals;
- top-level `TABLE ... ORDER BY 1`;
- `UPDATE` or `DELETE` ordinal order keys;
- global `ORDER BY` for `UNION` / `INTERSECT` / `EXCEPT`;
- ordinal support in grouped aggregate order planning;
- negative numeric order expressions such as `ORDER BY -1`;
- oversized numeric constants that MySQL treats as nonordinal constant
  expressions;
- string-literal order keys;
- arbitrary expression order keys beyond the current documented row-scalar
  slices; or
- warning emission for valid ordinal syntax, because MySQL 8.4.9 does not emit
  one for the observed statements.

## MyLite Lemon Syntax

The intended grammar extension is:

```lemon
select_order_key(A) ::= qualified_identifier(K). {
  A = K;
}
select_order_key(A) ::= INTEGER(T). {
  A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
select_order_key(A) ::= qualified_identifier(B) PLUS(T) INTEGER(C). {
  A = mylite_sql_parser_make_binary_expression(...);
}
```

The existing select-order list grammar continues to own comma-separated keys
and per-key direction:

```lemon
select_order_clause_opt(A) ::=
  ORDER(O) BY select_order_key(K) order_direction_opt(D) select_order_tail_opt(T).

select_order_item(A) ::= select_order_key(K) order_direction_opt(D).
```

No new AST node kind is required. Ordinal keys are represented as existing
integer literal nodes in order-key position.

## Runtime Semantics

Descriptor-backed `SELECT` resolves an ordinal against the already-planned
selected output descriptors. The resolved descriptor order item then uses the
same validation and SQL generation as an explicitly named selected descriptor
column. This keeps MyLite-owned MySQL semantics for admitted descriptor types
instead of relying on SQLite's default comparison for descriptor storage.

Row-scalar `SELECT` validates that each ordinal is within the selected item
count and emits that validated numeric position into the generated SQLite
`ORDER BY`. This lets SQLite order by the selected result expression without
duplicating nontrivial generated SQL or re-evaluating functions.

Invalid ordinals are rejected before SQLite preparation using the existing
MySQL-shaped unknown-order-column diagnostic.

## Storage, SQLite, And Performance

This is a MyLite wrapper/planner feature. It uses the public SQLite SQL surface
for row-scalar selected-expression ordering and existing descriptor-generated
SQLite order SQL for descriptor-backed ordering. It requires no SQLite fork
patch, no file-format change, and no new dependency.

Planning cost is a small ordinal parse and bounds check per ordinal key.
Execution cost is unchanged for descriptor order keys. Row-scalar ordinals avoid
duplicating selected expression SQL in the generated `ORDER BY`, which is both
simpler and avoids avoidable repeated evaluation.

## Tests

Coverage must include:

- parser acceptance for `ORDER BY 1 DESC, 2 ASC`;
- descriptor-backed `SELECT id FROM t ORDER BY 1 DESC`;
- `SELECT DISTINCT n FROM t ORDER BY 1 DESC`;
- row-scalar `SELECT CONCAT(...) FROM t ORDER BY 1 DESC`;
- invalid `ORDER BY 0`;
- invalid out-of-range descriptor ordinal;
- invalid out-of-range row-scalar ordinal; and
- MySQL 8.4.9 expectation script coverage for result rows, warning count, and
  diagnostics.
