# SQL query expressions

Core query expression, SELECT, set operation, ordering, limiting, locking, modifier, and result metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `SELECT` | ❌ | Full query expression surface |
| Query expression grammar | ❌ | Query terms and primaries |
| Projection list | ❌ | Aliases, wildcards, duplicate names |
| `SELECT ... FROM DUAL` | ❌ | `DUAL` one-row table semantics |
| `WHERE` | ❌ | Predicate semantics and warnings |
| `GROUP BY` | ❌ | Grouping and ONLY_FULL_GROUP_BY |
| `WITH ROLLUP` | ❌ | Super-aggregate rows and GROUPING() behavior |
| `HAVING` | ❌ | Post-group predicate semantics and alias resolution |
| Window definitions | ❌ | Named windows, frames, restrictions |
| `ORDER BY` | ❌ | Aliases, ordinals, collation |
| `LIMIT` / `OFFSET` | ❌ | Row limits and marker handling |
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
