# Aggregate functions

Aggregate functions and grouping helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `ANY_VALUE()` | ❌ | Suppress ONLY_FULL_GROUP_BY value rejection |
| `AVG()` | 🟡 | Limited one-item `SELECT AVG(column) [AS alias]` over one descriptor-backed persistent base table with optional source table alias, source-qualified column argument, and baseline `WHERE`; integer/`NULL` descriptor columns only, with four-fractional-digit text results while the intermediate signed-64 `SUM(column)` stays in range; no expression arguments, `DISTINCT`, exact decimal widening beyond the signed-64 intermediate envelope, grouping, ordering, limiting, or window forms |
| `BIT_AND()` | ❌ | Return bitwise AND |
| `BIT_OR()` | ❌ | Return bitwise OR |
| `BIT_XOR()` | ❌ | Return bitwise XOR |
| `COUNT()` | 🟡 | Limited `COUNT(*)` and `COUNT(integer/NULL/boolean literal)` in one-item `SELECT` with no source, `FROM DUAL`, or one descriptor-backed persistent base table, plus limited `COUNT(column)` and `COUNT(DISTINCT column)` over one descriptor-backed persistent base table; optional select-item alias labels are supported, and optional source table alias, source-qualified column arguments, and baseline `WHERE` are supported for table-backed forms; no general `COUNT(expr)`, grouping, ordering, limiting, or window forms |
| `COUNT(DISTINCT)` | 🟡 | Limited one-item `COUNT(DISTINCT column) [AS alias]` over one descriptor-backed persistent base table with optional source table alias, source-qualified column argument, and baseline `WHERE`; integer/`NULL` descriptor columns only, no multiple expressions, expression arguments, literals, grouping, ordering, limiting, or window forms |
| `GROUP_CONCAT()` | ❌ | Return a concatenated string |
| `GROUPING()` | ❌ | Distinguish super-aggregate ROLLUP rows from regular rows |
| `MAX()` | 🟡 | Limited one-item `SELECT MAX(column) [AS alias]` over one descriptor-backed persistent base table with optional source table alias, source-qualified column argument, and baseline `WHERE`; integer/`NULL` descriptor columns only, no expression arguments, `DISTINCT`, grouping, ordering, limiting, or window forms |
| `MIN()` | 🟡 | Limited one-item `SELECT MIN(column) [AS alias]` over one descriptor-backed persistent base table with optional source table alias, source-qualified column argument, and baseline `WHERE`; integer/`NULL` descriptor columns only, no expression arguments, `DISTINCT`, grouping, ordering, limiting, or window forms |
| `STD()` | ❌ | Return population standard deviation |
| `STDDEV()` | ❌ | Return population standard deviation |
| `STDDEV_POP()` | ❌ | Return population standard deviation |
| `STDDEV_SAMP()` | ❌ | Return sample standard deviation |
| `SUM()` | 🟡 | Limited one-item `SELECT SUM(column) [AS alias]` over one descriptor-backed persistent base table with optional source table alias, source-qualified column argument, and baseline `WHERE`; integer/`NULL` descriptor columns only, with results limited to MyLite's signed-64 text result envelope; no expression arguments, `DISTINCT`, exact decimal result widening beyond signed 64 bits, grouping, ordering, limiting, or window forms |
| `VAR_POP()` | ❌ | Return population standard variance |
| `VAR_SAMP()` | ❌ | Return sample variance |
| `VARIANCE()` | ❌ | Return population standard variance |

[Back to compatibility overview](../../COMPATIBILITY.md)
