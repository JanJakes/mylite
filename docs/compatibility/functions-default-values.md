# Default-value functions

Expression helpers tied to column defaults and DML value references.

| Function | Status | Notes |
| --- | --- | --- |
| `DEFAULT()` | 🟡 | Limited descriptor-owned `DEFAULT(column_name)` over one descriptor table source in row-scalar `SELECT`, plus supported `INSERT`/`REPLACE` `VALUES` and `SET` value positions, `UPDATE` assignments, and supported `INSERT ... ON DUPLICATE KEY UPDATE` assignments; source defaults are resolved from MyLite descriptors and support literal, implicit nullable, auto-increment `0`, and MySQL implicit `CURRENT_TIMESTAMP` descriptor defaults; expression defaults, generated current date/time defaults, no-source use, arbitrary expressions, broad implicit conversion, and unsupported statement shapes remain rejected. See [baseline DEFAULT() function](../specs/baseline-default-function/specs.md). |
| `VALUES()` | 🟡 | Deprecated `VALUES(column_name)` references inside supported `INSERT ... ON DUPLICATE KEY UPDATE` assignments, including copy-compatible direct cross-column assignments, direct row-scalar integer arithmetic such as `VALUES(col) + integer`, and nested supported row-scalar function expressions such as `CONCAT(VALUES(col), ...)` and `GREATEST(VALUES(col) + integer, ...)`, with warning `1287` per occurrence. No executable general non-ODKU function use, qualified references, or arbitrary expression evaluation. See [baseline ODKU VALUES cross-column references](../specs/baseline-odku-values-cross-column/specs.md). |

[Back to compatibility overview](../../COMPATIBILITY.md)
