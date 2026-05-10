# Aggregate functions

Aggregate functions and grouping helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `ANY_VALUE()` | ❌ | Suppress ONLY_FULL_GROUP_BY value rejection |
| `AVG()` | ❌ | Return average value of the argument |
| `BIT_AND()` | ❌ | Return bitwise AND |
| `BIT_OR()` | ❌ | Return bitwise OR |
| `BIT_XOR()` | ❌ | Return bitwise XOR |
| `COUNT()` | 🟡 | Limited `COUNT(*)` and `COUNT(integer/NULL/boolean literal)` in one-item `SELECT` with no source, `FROM DUAL`, or one descriptor-backed persistent base table, plus limited `COUNT(column)` and `COUNT(DISTINCT column)` over one descriptor-backed persistent base table; optional source table alias and baseline `WHERE` are supported for table-backed forms; no general `COUNT(expr)`, grouping, select-item aliases, alias-qualified arguments, ordering, limiting, or window forms |
| `COUNT(DISTINCT)` | 🟡 | Limited one-item `COUNT(DISTINCT column)` over one descriptor-backed persistent base table with optional source table alias and baseline `WHERE`; integer/`NULL` descriptor columns only, no multiple expressions, expression arguments, literals, table-qualified or alias-qualified arguments, grouping, select-item aliases, ordering, limiting, or window forms |
| `GROUP_CONCAT()` | ❌ | Return a concatenated string |
| `GROUPING()` | ❌ | Distinguish super-aggregate ROLLUP rows from regular rows |
| `MAX()` | 🟡 | Limited one-item `SELECT MAX(column)` over one descriptor-backed persistent base table with optional source table alias and baseline `WHERE`; integer/`NULL` descriptor columns only, no expression arguments, `DISTINCT`, grouping, select-item aliases, alias-qualified arguments, ordering, limiting, or window forms |
| `MIN()` | 🟡 | Limited one-item `SELECT MIN(column)` over one descriptor-backed persistent base table with optional source table alias and baseline `WHERE`; integer/`NULL` descriptor columns only, no expression arguments, `DISTINCT`, grouping, select-item aliases, alias-qualified arguments, ordering, limiting, or window forms |
| `STD()` | ❌ | Return population standard deviation |
| `STDDEV()` | ❌ | Return population standard deviation |
| `STDDEV_POP()` | ❌ | Return population standard deviation |
| `STDDEV_SAMP()` | ❌ | Return sample standard deviation |
| `SUM()` | ❌ | Return sum |
| `VAR_POP()` | ❌ | Return population standard variance |
| `VAR_SAMP()` | ❌ | Return sample variance |
| `VARIANCE()` | ❌ | Return population standard variance |

[Back to compatibility overview](../../COMPATIBILITY.md)
