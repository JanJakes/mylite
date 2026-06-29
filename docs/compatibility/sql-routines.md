# SQL routines

Stored procedure and function DDL, loadable-function declarations, and procedure invocation behavior.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER FUNCTION` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no stored-function descriptors or metadata changes |
| `ALTER PROCEDURE` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no stored-procedure descriptors or metadata changes |
| `CREATE FUNCTION` (stored) | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no stored-function descriptors, body execution, or metadata |
| `CREATE FUNCTION` (loadable) | ⚪ | Parsed and rejected at runtime with an unsupported diagnostic; no native shared-library loading, UDF registry, `mysql.func` mutation, or privilege handling |
| `CREATE PROCEDURE` | 🟡 | Limited session-local no-argument procedure descriptors with either one `SELECT` body or the bounded `DECLARE`/`SET` local-variable block plus final bare-local `SELECT` subset; broader procedure syntax is parsed and rejected as unsupported; no persistence, parameters, characteristics, handlers, privileges, or routine catalog rows |
| `DROP FUNCTION` (stored) | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no stored-function deletion or routine metadata cleanup |
| `DROP FUNCTION` (loadable) | ⚪ | Parsed and rejected at runtime with an unsupported diagnostic; no loadable-function registry lookup, shared-library lifecycle, `mysql.func` mutation, or missing-UDF note behavior |
| `DROP PROCEDURE` | 🟡 | Limited session-local descriptor removal for the current no-argument procedure bridge subset, including `IF EXISTS` note warning behavior; no persistent metadata cleanup |
| `CALL` | 🟡 | Limited invocation of session-local no-argument procedures, returning the final `SELECT` result set and `ROW_COUNT() = 0`; local defaults and assignments are evaluated through the existing scalar execution path; argument-bearing and unsupported-parameter forms parse and fail with unsupported diagnostics; no parameters, multiple result sets, OUT values, or stored-program control flow |
| `SHOW CREATE PROCEDURE` | 🟡 | Limited MySQL-shaped six-column metadata for session-local no-argument procedure bridge definitions, including the bounded local-variable body text; no persistent routine catalog, privilege filtering, mutable quote-control state, or full body formatting support |
| `INFORMATION_SCHEMA.PARAMETERS` | 🟡 | Queryable empty synthetic stored-routine parameter metadata view with MySQL 8.4.9 column shape; no routine descriptors, parameter descriptors, function return rows, definitions, definers, privileges, or rows for the session-local procedure bridge |
| `INFORMATION_SCHEMA.ROUTINES` | 🟡 | Queryable empty synthetic stored-routine metadata view with MySQL 8.4.9 column shape; no persistent routine descriptors, rows, parameter descriptors, definitions, definers, privileges, or rows for the session-local procedure bridge |

[Back to compatibility overview](../../COMPATIBILITY.md)
