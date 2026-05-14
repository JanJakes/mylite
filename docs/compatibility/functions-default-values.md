# Default-value functions

Expression helpers tied to column defaults and DML value references.

| Function | Status | Notes |
| --- | --- | --- |
| `DEFAULT()` | ❌ | Return default value for a table column |
| `VALUES()` | 🟡 | Limited deprecated same-target `VALUES(column_name)` references inside supported `INSERT ... ON DUPLICATE KEY UPDATE` assignments, with warning `1287` per occurrence; no general function use, expression use, qualified references, or cross-column duplicate assignment references |

[Back to compatibility overview](../../COMPATIBILITY.md)
