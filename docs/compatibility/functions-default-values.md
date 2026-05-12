# Default-value functions

Expression helpers tied to column defaults and DML value references.

| Function | Status | Notes |
| --- | --- | --- |
| `DEFAULT()` | ❌ | Return default value for a table column |
| `VALUES()` | 🟡 | Limited deprecated same-target `VALUES(column_name)` reference inside the supported `INSERT ... ON DUPLICATE KEY UPDATE` tail, with warning `1287`; no general function use, expression use, or cross-column duplicate assignment references |

[Back to compatibility overview](../../COMPATIBILITY.md)
