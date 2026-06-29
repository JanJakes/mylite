# SQL stored programs

Stored-program body statements, variables, control flow, cursors, handlers, and diagnostics.

MyLite parses broad stored-program DDL and condition-handling surfaces as
placeholders when the normal grammar cannot execute them, then returns an
unsupported diagnostic without catalog, data, or transaction side effects.
The only executable stored-program subset is the existing session-local
no-argument procedure bridge for a single `SELECT` body, plus a bounded
leading `DECLARE`/`SET` local-variable subset inside that bridge.

| Feature | Status | Notes |
| --- | --- | --- |
| `DO` | 🟡 | Limited expression-execution statement over the current no-source scalar expression domain, including supported session scalar/system-variable reads, integer/`NULL`/boolean literals, control-flow helpers, signed-64 arithmetic, top-level scalar `/` division, comparison, logical, and scalar-`IS`, limited unsigned-64 numeric bitwise operators, limited numeric `ABS()`, `SIGN()`, `CEIL()`/`CEILING()`/`FLOOR()`, limited integer-domain `ROUND()` including the current signed-64 negative-place subset, `BIT_COUNT()`, `BIN()`, `OCT()`, `CONV()`, `SQRT()`, `DEGREES()`, `RADIANS()`, `ACOS()`, `ASIN()`, `SIN()`, `COS()`, `TAN()`, `COT()`, `ATAN()`, and `ATAN2()`, limited `CRC32()`, `FORMAT()`, and `TRUNCATE()`, limited string length and ASCII string case functions, limited top-level `PI()` constant, and top-level `CASE`; returns no result rows, affected rows `0`, and evaluated-expression warnings for the admitted subset; no aliases, table-backed evaluation, variables, assignment, subqueries, parameters, general string/decimal/float expressions outside the documented scalar comparison subset, hex/bit expressions, stored-program blocks, or arbitrary SQLite pass-through |
| `SELECT ... INTO var_list` | 🟡 | Top-level supported `SELECT ... INTO @user_variable[, ...]` assignment only; no stored-program local variables, routine scope, cursors, handlers, or `OUTFILE`/`DUMPFILE`. See [baseline SELECT INTO user variables](../specs/baseline-select-into-user-variables/specs.md) |
| `BEGIN ... END` | 🟡 | Limited executable support in the session-local no-argument procedure bridge for a single `SELECT` body with optional leading local-variable declarations and assignments; broader block scope, nested blocks, control flow, cursors, handlers, parameters, persistent routine catalog state, and DML body statements remain unsupported. See [baseline stored-program body placeholders](../specs/baseline-stored-program-body-placeholders/specs.md) and [baseline stored procedure local variables](../specs/baseline-stored-procedure-local-variables/specs.md) |
| Statement labels | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no label binding |
| `DECLARE` local variables | 🟡 | Limited leading local variables in the session-local no-argument procedure bridge, with optional `DEFAULT`, simple `SET` assignments before the final `SELECT`, case-insensitive names, and supported scalar expression evaluation; no parameters, nested scopes, cursors, handlers, `SELECT ... INTO`, DML body statements, or full stored-routine catalog semantics. See [baseline stored procedure local variables](../specs/baseline-stored-procedure-local-variables/specs.md) |
| `DECLARE ... CONDITION` | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no named condition semantics |
| `DECLARE ... CURSOR` | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no cursor declaration lifecycle |
| `DECLARE ... HANDLER` | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no handler dispatch |
| `CASE` statement | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no statement execution |
| `IF` statement | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no branch execution |
| `LOOP` | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no loop execution |
| `REPEAT` | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no loop execution |
| `WHILE` | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no loop execution |
| `ITERATE` | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no control transfer |
| `LEAVE` | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no control transfer |
| `RETURN` | ⚪ | Recognized inside stored-function DDL placeholders with unsupported diagnostics; no function return execution |
| `OPEN` cursor | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no cursor open lifecycle |
| `FETCH` cursor | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no cursor fetch lifecycle |
| `CLOSE` cursor | ⚪ | Recognized inside stored-program DDL placeholders with unsupported diagnostics; no cursor close lifecycle |
| `GET DIAGNOSTICS` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no current or stacked diagnostics assignment surface |
| `SIGNAL` | ⚪ | Parsed as an unsupported stored-program/condition-handling placeholder; no user-raised SQLSTATE, warning/error, or condition item semantics |
| `RESIGNAL` | ⚪ | Parsed as an unsupported stored-program/condition-handling placeholder; no handler rethrow, active-handler validation, or diagnostics mutation |

[Back to compatibility overview](../../COMPATIBILITY.md)
