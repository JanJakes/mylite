# SQL routines

Stored procedure and function DDL, loadable-function declarations, and procedure invocation behavior.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER FUNCTION` | ❌ | Stored-function metadata characteristics |
| `ALTER PROCEDURE` | ❌ | Stored-procedure metadata characteristics |
| `CREATE FUNCTION` (stored) | ❌ | Stored-function definition and body |
| `CREATE FUNCTION` (loadable) | ❌ | Loadable-function diagnostics |
| `CREATE PROCEDURE` | ❌ | Procedure definition and body |
| `DROP FUNCTION` (stored) | ❌ | Stored-function deletion and routine metadata cleanup |
| `DROP FUNCTION` (loadable) | ❌ | Loadable-function deregistration syntax |
| `DROP PROCEDURE` | ❌ | Stored-procedure deletion and metadata cleanup |
| `CALL` | ❌ | Procedure invocation results |
| `INFORMATION_SCHEMA.ROUTINES` | 🟡 | Queryable empty synthetic stored-routine metadata view with MySQL 8.4.9 column shape; no routine descriptors, rows, DDL, parameters, definitions, definers, privileges, or execution |

[Back to compatibility overview](../../COMPATIBILITY.md)
