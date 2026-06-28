# Default-value functions

Expression helpers tied to column defaults and DML value references.

| Function | Status | Notes |
| --- | --- | --- |
| `DEFAULT()` | 🟡 | Limited descriptor-owned `DEFAULT(column_name)` over one descriptor table source in row-scalar `SELECT`, plus supported `INSERT`/`REPLACE` `VALUES` and `SET` value positions, `UPDATE` assignments, and supported `INSERT ... ON DUPLICATE KEY UPDATE` assignments; source defaults are resolved from MyLite descriptors and support literal, implicit nullable, auto-increment `0`, and MySQL implicit `CURRENT_TIMESTAMP` descriptor defaults; expression defaults, generated current date/time defaults, no-source use, arbitrary expressions, broad implicit conversion, and unsupported statement shapes remain rejected. See [baseline DEFAULT() function](../specs/baseline-default-function/specs.md). |
| `VALUES()` | ✅ | Deprecated `VALUES(column_name)` references are supported in documented ODKU assignment expressions and table-backed non-ODKU row-scalar `SELECT`; non-ODKU use resolves qualified source columns, returns `NULL`, and increments only `@@warning_count`. Unsupported statement envelopes remain explicit gaps. See [baseline ODKU VALUES cross-column references](../specs/baseline-odku-values-cross-column/specs.md) and [baseline non-ODKU VALUES function](../specs/baseline-values-function-non-odku/specs.md). |

[Back to compatibility overview](../../COMPATIBILITY.md)
