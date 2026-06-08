# SQL routines

Stored procedure and function DDL, loadable-function declarations, and procedure invocation behavior.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER FUNCTION` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no stored-function descriptors or metadata changes |
| `ALTER PROCEDURE` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no stored-procedure descriptors or metadata changes |
| `CREATE FUNCTION` (stored) | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no stored-function descriptors, body execution, or metadata |
| `CREATE FUNCTION` (loadable) | ❌ | Loadable-function diagnostics |
| `CREATE PROCEDURE` | 🟡 | Limited session-local no-argument procedure descriptors with one `SELECT` body inside `BEGIN ... END`; broader procedure syntax is parsed and rejected as unsupported; no persistence, parameters, characteristics, variables, handlers, privileges, or routine catalog rows |
| `DROP FUNCTION` (stored) | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no stored-function deletion or routine metadata cleanup |
| `DROP FUNCTION` (loadable) | ❌ | Loadable-function deregistration syntax |
| `DROP PROCEDURE` | 🟡 | Limited session-local descriptor removal for the current no-argument single-`SELECT` procedure subset, including `IF EXISTS` note warning behavior; no persistent metadata cleanup |
| `CALL` | 🟡 | Limited invocation of session-local no-argument single-`SELECT` procedures, returning the body result set and `ROW_COUNT() = 0`; argument-bearing and unsupported-parameter forms parse and fail with unsupported diagnostics; no parameters, multiple result sets, OUT values, or stored-program control flow |
| `SHOW CREATE PROCEDURE` | 🟡 | Limited MySQL-shaped six-column metadata for session-local no-argument single-`SELECT` procedures; no persistent routine catalog, privilege filtering, or full body formatting support |
| `INFORMATION_SCHEMA.PARAMETERS` | 🟡 | Queryable empty synthetic stored-routine parameter metadata view with MySQL 8.4.9 column shape; no routine descriptors, parameter descriptors, function return rows, definitions, definers, privileges, or rows for the session-local procedure bridge |
| `INFORMATION_SCHEMA.ROUTINES` | 🟡 | Queryable empty synthetic stored-routine metadata view with MySQL 8.4.9 column shape; no persistent routine descriptors, rows, parameter descriptors, definitions, definers, privileges, or rows for the session-local procedure bridge |

[Back to compatibility overview](../../COMPATIBILITY.md)
