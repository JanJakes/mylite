# Window functions

Window-only ranking, distribution, navigation, and frame-value functions.

| Function | Status | Notes |
| --- | --- | --- |
| `CUME_DIST()` | ❌ | Cumulative distribution value |
| `DENSE_RANK()` | ❌ | Rank of current row within its partition, without gaps |
| `FIRST_VALUE()` | ❌ | Value of argument from first row of window frame |
| `LAG()` | ❌ | Previous-row value |
| `LAST_VALUE()` | ❌ | Value of argument from last row of window frame |
| `LEAD()` | ❌ | Next-row value |
| `NTH_VALUE()` | ❌ | Value of argument from N-th row of window frame |
| `NTILE()` | ❌ | Bucket number of current row within its partition |
| `PERCENT_RANK()` | ❌ | Percentage rank value |
| `RANK()` | ❌ | Rank of current row within its partition, with gaps |
| `ROW_NUMBER()` | 🟡 | Limited projection-only `ROW_NUMBER() OVER (...)` for no-source, `DUAL`, and one descriptor-backed table source, with optional one descriptor-column `PARTITION BY`, optional one descriptor-column `ORDER BY` plus `ASC` / `DESC`, and existing row-scalar `WHERE` / outer `ORDER BY` / `LIMIT`; no named windows, frame clauses, expressions, multiple keys, joins, grouped selects, predicates, DML contexts, or other window functions. See [baseline ROW_NUMBER window function](../specs/baseline-row-number-window-function/specs.md). |

[Back to compatibility overview](../../COMPATIBILITY.md)
