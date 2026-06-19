# Default-value functions

Expression helpers tied to column defaults and DML value references.

| Function | Status | Notes |
| --- | --- | --- |
| `DEFAULT()` | 🟡 | Limited descriptor-owned `DEFAULT(column_name)` over one descriptor table source in row-scalar `SELECT`, plus supported `INSERT`/`REPLACE` `VALUES` and `SET` value positions, `UPDATE` assignments, and supported `INSERT ... ON DUPLICATE KEY UPDATE` assignments; source defaults are resolved from MyLite descriptors and support literal or implicit nullable defaults plus auto-increment source value `0`; expression defaults, current date/time defaults, no-source use, arbitrary expressions, broad implicit conversion, and unsupported statement shapes remain rejected. See [baseline DEFAULT() function](../specs/baseline-default-function/specs.md). |
| `VALUES()` | 🟡 | Deprecated direct `VALUES(column_name)` references inside supported `INSERT ... ON DUPLICATE KEY UPDATE` assignments, including copy-compatible cross-column assignments, with warning `1287` per occurrence. The parser additionally admits nested `VALUES(column_name)` expression placeholders in duplicate-key update expressions and rejects unsupported expression execution deterministically. No executable general function use, qualified references, or arbitrary expression evaluation. See [baseline ODKU VALUES cross-column references](../specs/baseline-odku-values-cross-column/specs.md). |

[Back to compatibility overview](../../COMPATIBILITY.md)
