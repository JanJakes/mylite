# SQL table DML

Table-shaped data modification, import, replace, and direct table access
statements.

| Feature | Status | Notes |
| --- | --- | --- |
| `DELETE` (single-table) | 🟡 | Limited persistent base-table `DELETE FROM table` with optional baseline integer/`NULL` `WHERE` predicate, one unqualified descriptor `ORDER BY` column, optional `ASC`/`DESC`, exact affected rows, and `LIMIT row_count` using unsigned decimal literals in signed 64-bit range; no aliases, partitions, modifiers, joined deletes, `USING`, full ordering, offset forms, triggers, cascades, foreign keys, or privilege semantics |
| `DELETE` (multi-table) | ❌ | Multi-table forms and affected rows |
| `HANDLER` | ❌ | HANDLER OPEN, READ, and CLOSE cursor-like table access |
| `INSERT ... VALUES` | 🟡 | Limited single- and multi-row inserts into persistent base tables with optional unqualified column lists, integer/`NULL`/`TRUE`/`FALSE` values, descriptor-driven assignment, omitted columns filled from descriptor integer defaults or effective `NULL` defaults, strict range/nullability/no-default diagnostics, affected rows, and all-or-nothing statement behavior; no-column-list inserts target visible descriptor columns only, while explicit column lists may name invisible columns; no DML `DEFAULT` keyword values, keys, warnings, insert ids, `IGNORE`, `ON DUPLICATE KEY UPDATE`, or `SELECT` form |
| `INSERT ... SET` | 🟡 | Limited one-row `INSERT [INTO] table_name SET column_name = value[, ...]` into persistent base tables with unqualified assignment columns, supported decimal integer/`NULL`/`TRUE`/`FALSE` values, descriptor-driven assignment including invisible columns when explicitly named, omitted columns filled from descriptor integer defaults or effective `NULL` defaults, strict required-column/range/nullability diagnostics, one affected row, and all-or-nothing statement behavior; no modifiers, table-qualified assignment targets, expression assignments, DML `DEFAULT` keyword values, generated values, aliases, partitions, warnings, insert ids, `IGNORE`, `ON DUPLICATE KEY UPDATE`, or `SELECT` form |
| `INSERT ... SELECT` | ❌ | Query insert and metadata inference |
| `INSERT ... ON DUPLICATE KEY UPDATE` | ❌ | Conflict handling and warnings |
| `INSERT IGNORE` | ❌ | Duplicate, conversion, and constraint warning demotion rules |
| `INSERT DELAYED` | ❌ | Deprecated delayed insert syntax, diagnostics |
| `INSERT LOW_PRIORITY` / `HIGH_PRIORITY` | ❌ | Priority modifiers and embedded-compatible treatment |
| `LOAD DATA INFILE` | ❌ | Server-side import syntax |
| `LOAD DATA LOCAL INFILE` | ❌ | LOCAL INFILE flow and security |
| `LOAD XML INFILE` | ❌ | XML import syntax |
| `LOAD XML LOCAL INFILE` | ❌ | Client-side XML import request behavior diagnostics |
| `REPLACE ... VALUES` | 🟡 | Limited no-key-descriptor `REPLACE [INTO] table_name [(column_name[, ...])] VALUES (value[, ...])[, ...]` insert-equivalent path for persistent base tables with integer/`NULL`/`TRUE`/`FALSE` values, descriptor column resolution including explicitly named invisible columns, omitted-column descriptor default filling, strict range/nullability/no-default diagnostics, affected rows equal to inserted rows, and all-or-nothing statement behavior; duplicate-key delete-insert semantics await primary/unique key descriptors, and there is no `VALUE` synonym, row constructors, modifiers, partitions, `SELECT`, generated values, warnings, or insert ids |
| `REPLACE ... SET` | 🟡 | Limited no-key-descriptor one-row `REPLACE [INTO] table_name SET column_name = value[, ...]` insert-equivalent path for persistent base tables with unqualified descriptor assignment targets, integer/`NULL`/`TRUE`/`FALSE` values, omitted-column descriptor default filling, strict range/nullability/no-default diagnostics, one affected row, and all-or-nothing statement behavior; duplicate-key delete-insert semantics await primary/unique key descriptors, and there are no modifiers, partitions, qualified assignment targets, expression/default assignments, `VALUES`, or `SELECT` forms |
| `REPLACE ... SELECT` | ❌ | Replace from query expression semantics |
| `REPLACE LOW_PRIORITY` / `DELAYED` | ❌ | Priority and deprecated delayed modifiers for REPLACE |
| `UPDATE` (single-table) | 🟡 | Limited persistent base-table `UPDATE table_name SET column_name = value` with one unqualified assignment column, supported decimal integer/`NULL`/`TRUE`/`FALSE` assignment values, optional baseline `WHERE`, optional one-column `ORDER BY` with `ASC`/`DESC`, and optional `LIMIT row_count` using unsigned decimal literals in signed 64-bit range; assignment, predicate, and ordering columns resolve through descriptors and may explicitly name invisible columns; affected rows report changed rows; no aliases, table-qualified assignment targets, multiple assignments, expression assignments, defaults, partitions, modifiers, joined updates, multi-table updates, table-qualified/order expression/ordinal/multiple-key ordering, offset forms, triggers, cascades, foreign keys, generated columns, or privilege semantics |
| `UPDATE` (multi-table) | ❌ | Joined update semantics |
| SQL safe updates variable | 🟡 | Limited scalar `@@sql_safe_updates` reads report fixed disabled value `0`; no mutable safe-updates state, `sql_select_limit`, `max_join_size`, key-aware DML checks, or changed `UPDATE`/`DELETE` behavior |
| SQL warnings variable | 🟡 | Limited scalar `@@sql_warnings` reads report fixed disabled value `0`; no mutable warning-reporting state, warning-producing insert conversions, `INSERT IGNORE`, strict warning demotion, or single-row insert information strings |

[Back to compatibility overview](../../COMPATIBILITY.md)
