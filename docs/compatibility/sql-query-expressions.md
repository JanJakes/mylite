# SQL query expressions

Core query expression, SELECT, set operation, ordering, limiting, locking, modifier, and result metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `SELECT` | 🟡 | Descriptor-driven single persistent base-table `SELECT *` and `SELECT column[, column ...] FROM table` with optional limited `WHERE`, single-column `ORDER BY`, and `LIMIT`/`OFFSET`, plus limited one-item `COUNT(*)` with no source, `FROM DUAL`, or one descriptor-backed table with optional baseline `WHERE`, and limited one-item `MIN(column)` / `MAX(column)` over one descriptor-backed table with optional baseline `WHERE`; `SELECT *` expands visible descriptor columns only, while explicit column, predicate, order, and min/max aggregate references may name invisible columns; no general expression projection, aliases, joins, grouping, locking, mutable `sql_select_limit` row caps, `sql_buffer_result` temporary result buffering, `sql_auto_is_null` auto-increment lookup behavior, `sql_big_selects` row-estimate aborts, or arbitrary SQLite pass-through |
| Query expression grammar | ❌ | Query terms and primaries |
| Projection list | 🟡 | Wildcard uses visible descriptor columns in catalog ordinal order; explicit projections resolve unqualified descriptor column names, including invisible columns; limited one-item `COUNT(*)`, `MIN(column)`, and `MAX(column)` aggregate forms are supported; duplicate projected descriptor columns are allowed, with no aliases, expression metadata, or table-qualified references |
| `SELECT ... FROM DUAL` | 🟡 | Limited scalar/session-function and one-item `COUNT(*)` one-row semantics; no table-backed descriptor behavior or arbitrary expression evaluation |
| `WHERE` | 🟡 | One unqualified descriptor column predicate for filtered table `SELECT`, `DELETE`, and `UPDATE`: integer comparisons with a non-`NULL` decimal integer or `TRUE`/`FALSE` right operand (`=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, `>=`) plus normal stored-value `IS NULL` and `IS NOT NULL`; no `WHERE TRUE`, boolean composition, literal-left comparisons, table-qualified columns, `col = NULL`, `IS TRUE`, `sql_auto_is_null` auto-increment lookup behavior, functions, parameters, string/decimal/float/hex/bit literals, subqueries, or general expression predicates |
| `GROUP BY` | ❌ | Grouping and ONLY_FULL_GROUP_BY |
| `WITH ROLLUP` | ❌ | Super-aggregate rows and GROUPING() behavior |
| `HAVING` | ❌ | Post-group predicate semantics and alias resolution |
| Window definitions | ❌ | Named windows, frames, restrictions |
| `ORDER BY` | 🟡 | One unqualified descriptor column for supported base-table `SELECT`, `DELETE`, and `UPDATE`; optional `ASC`/`DESC`; `ASC` is the default, `NULL` sorts before non-`NULL` ascending and after non-`NULL` descending; no aliases, ordinals, expressions, table-qualified keys, collations, multiple keys, or tie-order guarantee |
| `LIMIT` / `OFFSET` | 🟡 | Supported base-table `SELECT` forms are `LIMIT row_count`, `LIMIT row_count OFFSET offset`, and `LIMIT offset, row_count`; supported single-table `DELETE` and `UPDATE` admit only `LIMIT row_count`; all supported forms use unsigned decimal integer literals in the signed 64-bit range; `LIMIT 0` returns no rows for `SELECT` and matches no rows for supported DML; no signed, string, decimal, float, hex, bit, parameter, or expression limits |
| `@@sql_auto_is_null` | 🟡 | Limited scalar reads expose the fixed disabled value `0`; no mutable session state, `AUTO_INCREMENT` metadata, `LAST_INSERT_ID()`-driven special lookup behavior, or special `IS NULL` lookup behavior |
| `@@sql_big_selects` | 🟡 | Limited scalar reads expose the fixed enabled value `1`; no mutable session state, `max_join_size`, optimizer row-estimate aborts, or changed descriptor-backed `SELECT` execution |
| `@@sql_buffer_result` | 🟡 | Limited scalar reads expose the fixed disabled value `0`; no mutable session state, temporary result table materialization, lock-release behavior, or changed descriptor-backed `SELECT` execution |
| `@@sql_select_limit` | 🟡 | Limited scalar reads expose the fixed no-limit value `18446744073709551615`; no mutable session state and no implicit row caps for descriptor-backed `SELECT` |
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
