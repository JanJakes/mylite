# SQL routines

Stored procedure and function DDL, loadable-function declarations, and procedure invocation behavior.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER FUNCTION` | ❌ | Stored-function metadata characteristics |
| `ALTER PROCEDURE` | ❌ | Stored-procedure metadata characteristics |
| `CREATE FUNCTION` (stored) | ❌ | Stored-function definition and body |
| `CREATE FUNCTION` (loadable) | ❌ | Loadable-function diagnostics |
| `CREATE PROCEDURE` | 🟡 | Limited session-local no-argument procedure descriptors with one `SELECT` body inside `BEGIN ... END`; no persistence, parameters, characteristics, variables, handlers, privileges, or routine catalog rows |
| `DROP FUNCTION` (stored) | ❌ | Stored-function deletion and routine metadata cleanup |
| `DROP FUNCTION` (loadable) | ❌ | Loadable-function deregistration syntax |
| `DROP PROCEDURE` | 🟡 | Limited session-local descriptor removal for the current no-argument single-`SELECT` procedure subset, including `IF EXISTS` note warning behavior; no persistent metadata cleanup |
| `CALL` | 🟡 | Limited invocation of session-local no-argument single-`SELECT` procedures, returning the body result set and `ROW_COUNT() = 0`; no parameters, multiple result sets, OUT values, or stored-program control flow |
| `SHOW CREATE PROCEDURE` | 🟡 | Limited MySQL-shaped six-column metadata for session-local no-argument single-`SELECT` procedures; no persistent routine catalog, privilege filtering, or full body formatting support |
| `INFORMATION_SCHEMA.PARAMETERS` | 🟡 | Queryable empty synthetic stored-routine parameter metadata view with MySQL 8.4.9 column shape; no routine descriptors, parameter descriptors, function return rows, definitions, definers, privileges, or rows for the session-local procedure bridge |
| `INFORMATION_SCHEMA.ROUTINES` | 🟡 | Queryable empty synthetic stored-routine metadata view with MySQL 8.4.9 column shape; no persistent routine descriptors, rows, parameter descriptors, definitions, definers, privileges, or rows for the session-local procedure bridge |

[Back to compatibility overview](../../COMPATIBILITY.md)
