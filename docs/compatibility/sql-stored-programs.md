# SQL stored programs

Stored-program body statements, variables, control flow, cursors, handlers, and diagnostics.

| Feature | Status | Notes |
| --- | --- | --- |
| `DO` | ❌ | Expression execution with warning/error semantics |
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
