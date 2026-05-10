# System functions

Session context, named locks, statement diagnostics, XML, IP, timing, file, and
miscellaneous system helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `BENCHMARK()` | ❌ | Repeatedly execute an expression |
| `CONNECTION_ID()` | 🟡 | Limited one-row scalar `SELECT CONNECTION_ID()` with optional `FROM DUAL`; returns a MyLite process-local nonzero handle id; no server thread, process-list, Performance Schema, `pseudo_thread_id`, aliases, table-backed evaluation, or protocol metadata |
| `CURRENT_ROLE()` | 🟡 | Limited one-row scalar `SELECT CURRENT_ROLE()` with optional `FROM DUAL`; returns MyLite's no-active-role value `NONE`; no role catalog, role grants, `SET ROLE`, bare `CURRENT_ROLE`, aliases, table-backed evaluation, clauses, or general expression support |
| `CURRENT_USER(), CURRENT_USER` | 🟡 | Limited one-row scalar select returns MyLite's embedded current identity `root@%`; no accounts, definers, roles, privileges, or table-backed evaluation |
| `DATABASE()` | 🟡 | Limited one-row scalar `SELECT DATABASE()` with optional `FROM DUAL`; returns selected MyLite catalog schema or `NULL`; no aliases, table-backed evaluation, clauses, or general expression support |
| `ExtractValue()` | ❌ | Extract a value from an XML string using XPath notation |
| `FOUND_ROWS()` | ❌ | Rows before LIMIT |
| `GET_LOCK()` | ❌ | Get a named lock |
| `ICU_VERSION()` | ❌ | ICU library version |
| `INET_ATON()` | ❌ | Return numeric value of an IP address |
| `INET_NTOA()` | ❌ | Return IP address from a numeric value |
| `IS_FREE_LOCK()` | ❌ | Whether the named lock is free |
| `IS_USED_LOCK()` | ❌ | Named-lock owner lookup |
| `LAST_INSERT_ID()` | 🟡 | Limited zero-argument one-row scalar `SELECT LAST_INSERT_ID()` with optional `FROM DUAL`; returns the current no-generated-id baseline value `0`; no `AUTO_INCREMENT`, `LAST_INSERT_ID(expr)`, protocol insert-id metadata, C API state, aliases, table-backed evaluation, or generated id behavior |
| `LOAD_FILE()` | ❌ | Load the named file |
| `NAME_CONST()` | ❌ | Cause the column to have the given name |
| `RELEASE_ALL_LOCKS()` | ❌ | Release all current named locks |
| `RELEASE_LOCK()` | ❌ | Release the named lock |
| `ROW_COUNT()` | 🟡 | Limited one-row scalar `SELECT ROW_COUNT()` with optional `FROM DUAL`; returns connection-local row-count state for supported baseline statements; no protocol OK-packet parity, `CLIENT_FOUND_ROWS`, aliases, table-backed evaluation, or full diagnostics-area support |
| `SCHEMA()` | 🟡 | Limited synonym for `DATABASE()` in the same scalar-select slice |
| `SESSION_USER()` | 🟡 | Limited no-whitespace one-row scalar select returns MyLite's embedded client identity `root@%` as a `USER()` synonym; no `IGNORE_SPACE`, stored-function resolution, authentication, host matching, privileges, or table-backed evaluation |
| `SLEEP()` | ❌ | Sleep for a number of seconds |
| `SYSTEM_USER()` | 🟡 | Limited no-whitespace one-row scalar select returns MyLite's embedded client identity `root@%` as a `USER()` synonym; no `SYSTEM_USER` privilege semantics, authentication, host matching, privileges, or table-backed evaluation |
| `UpdateXML()` | ❌ | Return replaced XML fragment |
| `USER()` | 🟡 | Limited one-row scalar select returns MyLite's embedded client identity `root@%`; no authentication, host matching, privileges, or table-backed evaluation |
| `VERSION()` | 🟡 | Limited one-row scalar `SELECT VERSION()` with optional `FROM DUAL`; returns MyLite's engine version string, not an impersonated MySQL server version; no aliases, table-backed evaluation, clauses, or protocol handshake version reporting |

[Back to compatibility overview](../../COMPATIBILITY.md)
