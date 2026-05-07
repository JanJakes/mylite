# SQL query expressions

Core query expression, SELECT, set operation, ordering, limiting, locking, modifier, and result metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `SELECT` | 🟡 | Descriptor-driven single persistent base-table `SELECT *` and `SELECT column[, column ...] FROM table` with optional limited `WHERE`, single-column `ORDER BY`, and `LIMIT`/`OFFSET`, using unqualified or schema-qualified table names; no expression projection, aliases, joins, grouping, locking, or arbitrary SQLite pass-through |
| Query expression grammar | ❌ | Query terms and primaries |
| Projection list | 🟡 | Wildcard uses catalog ordinal order; explicit projections resolve unqualified descriptor column names only, with duplicate projected columns allowed and no aliases, expression metadata, or table-qualified references |
| `SELECT ... FROM DUAL` | ❌ | `DUAL` one-row table semantics |
| `WHERE` | 🟡 | One unqualified descriptor column predicate for filtered table `SELECT`: integer comparisons with a non-`NULL` decimal integer right operand (`=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, `>=`) plus `IS NULL` and `IS NOT NULL`; no boolean composition, literal-left comparisons, table-qualified columns, `col = NULL`, functions, parameters, string/decimal/float/hex/bit literals, subqueries, or general expression predicates |
| `GROUP BY` | ❌ | Grouping and ONLY_FULL_GROUP_BY |
| `WITH ROLLUP` | ❌ | Super-aggregate rows and GROUPING() behavior |
| `HAVING` | ❌ | Post-group predicate semantics and alias resolution |
| Window definitions | ❌ | Named windows, frames, restrictions |
| `ORDER BY` | 🟡 | One unqualified descriptor column for supported base-table `SELECT`; optional `ASC`/`DESC`; `ASC` is the default, `NULL` sorts before non-`NULL` ascending and after non-`NULL` descending; no aliases, ordinals, expressions, table-qualified keys, collations, multiple keys, or tie-order guarantee |
| `LIMIT` / `OFFSET` | 🟡 | Supported base-table `SELECT` forms are `LIMIT row_count`, `LIMIT row_count OFFSET offset`, and `LIMIT offset, row_count` with unsigned decimal integer literals in the signed 64-bit range; `LIMIT 0` returns metadata and no rows; no signed, string, decimal, float, hex, bit, parameter, or expression limits |
| `DISTINCT` / `DISTINCTROW` | ❌ | Duplicate elimination semantics and metadata |
| `UNION` | ❌ | ALL/DISTINCT, metadata, ordering |
| `INTERSECT` | ❌ | set operator semantics |
| `EXCEPT` | ❌ | set operator semantics |
| `TABLE` | ❌ | Table-value statement syntax and ordering/limit behavior |
| `VALUES` | ❌ | Standalone values statement and row constructor behavior |
| Locking clauses | ❌ | FOR UPDATE/SHARE options |
| SELECT modifiers | ❌ | Modifiers including SQL_CALC_FOUND_ROWS |
| Expression metadata | ❌ | Type, flags, charset, origin |

[Back to compatibility overview](../../COMPATIBILITY.md)
