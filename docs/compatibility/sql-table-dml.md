# SQL table DML

Table-shaped data modification, import, replace, and direct table access
statements.

| Feature | Status | Notes |
| --- | --- | --- |
| `DELETE` (single-table) | 🟡 | Limited persistent base-table `DELETE FROM table` with optional baseline integer/`NULL` `WHERE` predicate, one unqualified descriptor `ORDER BY` column, optional `ASC`/`DESC`, exact affected rows, and `LIMIT row_count` using unsigned decimal literals in signed 64-bit range; no aliases, partitions, modifiers, joined deletes, `USING`, full ordering, offset forms, triggers, cascades, foreign keys, or privilege semantics |
| `DELETE` (multi-table) | ❌ | Multi-table forms and affected rows |
| `HANDLER` | ❌ | HANDLER OPEN, READ, and CLOSE cursor-like table access |
| `INSERT ... VALUES` | 🟡 | Limited single- and multi-row inserts into persistent base tables with optional unqualified column lists, integer/`NULL` values, descriptor-driven assignment, strict range/nullability diagnostics, affected rows, and all-or-nothing statement behavior; no defaults, keys, warnings, insert ids, `IGNORE`, `ON DUPLICATE KEY UPDATE`, `SET`, or `SELECT` form |
| `INSERT ... SET` | ❌ | MySQL SET-form insert semantics |
| `INSERT ... SELECT` | ❌ | Query insert and metadata inference |
| `INSERT ... ON DUPLICATE KEY UPDATE` | ❌ | Conflict handling and warnings |
| `INSERT IGNORE` | ❌ | Duplicate, conversion, and constraint warning demotion rules |
| `INSERT DELAYED` | ❌ | Deprecated delayed insert syntax, diagnostics |
| `INSERT LOW_PRIORITY` / `HIGH_PRIORITY` | ❌ | Priority modifiers and embedded-compatible treatment |
| `LOAD DATA INFILE` | ❌ | Server-side import syntax |
| `LOAD DATA LOCAL INFILE` | ❌ | LOCAL INFILE flow and security |
| `LOAD XML INFILE` | ❌ | XML import syntax |
| `LOAD XML LOCAL INFILE` | ❌ | Client-side XML import request behavior diagnostics |
| `REPLACE ... VALUES` | ❌ | Delete-insert semantics |
| `REPLACE ... SET` | ❌ | SET-form replace semantics |
| `REPLACE ... SELECT` | ❌ | Replace from query expression semantics |
| `REPLACE LOW_PRIORITY` / `DELAYED` | ❌ | Priority and deprecated delayed modifiers for REPLACE |
| `UPDATE` (single-table) | ❌ | Assignment order, LIMIT, modifiers |
| `UPDATE` (multi-table) | ❌ | Joined update semantics |

[Back to compatibility overview](../../COMPATIBILITY.md)
