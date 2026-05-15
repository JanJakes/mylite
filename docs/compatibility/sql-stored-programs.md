# SQL stored programs

Stored-program body statements, variables, control flow, cursors, handlers, and diagnostics.

| Feature | Status | Notes |
| --- | --- | --- |
| `DO` | 🟡 | Limited expression-execution statement over the current no-source scalar expression domain, including supported session scalar/system-variable reads, integer/`NULL`/boolean literals, control-flow helpers, signed-64 arithmetic, top-level scalar `/` division, comparison, logical, and scalar-`IS`, limited unsigned-64 numeric bitwise operators, limited numeric `ABS()`, `SIGN()`, `CEIL()`/`CEILING()`/`FLOOR()`, one-argument `ROUND()`, `BIT_COUNT()`, `BIN()`, `OCT()`, `CONV()`, `SQRT()`, `DEGREES()`, `RADIANS()`, `ACOS()`, `ASIN()`, `SIN()`, `COS()`, `TAN()`, `COT()`, `ATAN()`, and `ATAN2()`, limited `CRC32()`, `FORMAT()`, and `TRUNCATE()`, limited string length and ASCII string case functions, limited top-level `PI()` constant, and top-level `CASE`; `ROUND()` is limited to one argument; returns no result rows, affected rows `0`, and evaluated-expression warnings for the admitted subset; no aliases, table-backed evaluation, variables, assignment, subqueries, parameters, general string/decimal/float/hex/bit expressions, stored-program blocks, or arbitrary SQLite pass-through |
| `SELECT ... INTO var_list` | ❌ | User/local variable assignment semantics |
| `BEGIN ... END` | ❌ | Stored-program block scope |
| Statement labels | ❌ | LEAVE/ITERATE label binding |
| `DECLARE` local variables | ❌ | Local variables and scope |
| `DECLARE ... CONDITION` | ❌ | Named condition declarations |
| `DECLARE ... CURSOR` | ❌ | Cursor declaration over SELECT statements |
| `DECLARE ... HANDLER` | ❌ | Handler declaration semantics |
| `CASE` statement | ❌ | Stored-program CASE statement semantics |
| `IF` statement | ❌ | Stored-program IF/ELSEIF/ELSE semantics |
| `LOOP` | ❌ | Stored-program LOOP semantics |
| `REPEAT` | ❌ | Stored-program REPEAT UNTIL semantics |
| `WHILE` | ❌ | Stored-program WHILE semantics |
| `ITERATE` | ❌ | Loop iteration transfer |
| `LEAVE` | ❌ | Block/loop exit transfer |
| `RETURN` | ❌ | Stored-function return semantics |
| `OPEN` cursor | ❌ | Cursor open lifecycle |
| `FETCH` cursor | ❌ | Cursor fetch into variables and NOT FOUND handling |
| `CLOSE` cursor | ❌ | Cursor close lifecycle |
| `GET DIAGNOSTICS` | ❌ | Current and stacked diagnostics retrieval |
| `SIGNAL` | ❌ | User-raised SQLSTATE and condition item semantics |
| `RESIGNAL` | ❌ | Handler rethrow and diagnostics mutation |

[Back to compatibility overview](../../COMPATIBILITY.md)
