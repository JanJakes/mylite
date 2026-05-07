# Aggregate functions

Aggregate functions and grouping helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `ANY_VALUE()` | ❌ | Suppress ONLY_FULL_GROUP_BY value rejection |
| `AVG()` | ❌ | Return average value of the argument |
| `BIT_AND()` | ❌ | Return bitwise AND |
| `BIT_OR()` | ❌ | Return bitwise OR |
| `BIT_XOR()` | ❌ | Return bitwise XOR |
| `COUNT()` | 🟡 | Limited `COUNT(*)` in one-item `SELECT` with no source, `FROM DUAL`, or one descriptor-backed persistent base table with optional baseline `WHERE`; no `COUNT(expr)`, `DISTINCT`, grouping, aliases, ordering, limiting, or window forms |
| `COUNT(DISTINCT)` | ❌ | Return count of a number of different values |
| `GROUP_CONCAT()` | ❌ | Return a concatenated string |
| `GROUPING()` | ❌ | Distinguish super-aggregate ROLLUP rows from regular rows |
| `MAX()` | ❌ | Return maximum value |
| `MIN()` | ❌ | Return minimum value |
| `STD()` | ❌ | Return population standard deviation |
| `STDDEV()` | ❌ | Return population standard deviation |
| `STDDEV_POP()` | ❌ | Return population standard deviation |
| `STDDEV_SAMP()` | ❌ | Return sample standard deviation |
| `SUM()` | ❌ | Return sum |
| `VAR_POP()` | ❌ | Return population standard variance |
| `VAR_SAMP()` | ❌ | Return sample variance |
| `VARIANCE()` | ❌ | Return population standard variance |

[Back to compatibility overview](../../COMPATIBILITY.md)
