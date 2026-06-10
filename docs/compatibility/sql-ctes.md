# SQL common table expressions

Non-recursive and recursive common table expression compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `WITH` common table expressions | 🟡 | Parser-admitted unsupported placeholders for complete CTE-started `SELECT`, parenthesized query-expression, `TABLE`, `VALUES`, `UPDATE`, and `DELETE` surfaces, including optional CTE column lists and comma-separated CTE definitions; no CTE execution, materialization, name resolution, duplicate-name diagnostics, subquery/view/CTAS/DML-source CTE support, or optimizer behavior |
| `WITH RECURSIVE` | 🟡 | Parser-admitted unsupported placeholder for complete recursive CTE-started surfaces; no recursive execution, cycle handling, working-table semantics, recursion limits, or recursive diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
