# SQL prepared statements

SQL-level prepared statement lifecycle compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `PREPARE` | ❌ | Markers, variables, errors |
| `EXECUTE` | ❌ | USING variables and metadata |
| `DEALLOCATE PREPARE` / `DROP PREPARE` | ❌ | Prepared statement cleanup |

[Back to compatibility overview](../../COMPATIBILITY.md)
