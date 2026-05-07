# System functions

Session context, named locks, statement diagnostics, XML, IP, timing, file, and
miscellaneous system helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `BENCHMARK()` | ❌ | Repeatedly execute an expression |
| `CONNECTION_ID()` | ❌ | Return connection ID (thread ID) for the connection |
| `CURRENT_ROLE()` | ❌ | Return current active roles |
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
| `LAST_INSERT_ID()` | ❌ | Value of the AUTOINCREMENT column for the last INSERT |
| `LOAD_FILE()` | ❌ | Load the named file |
| `NAME_CONST()` | ❌ | Cause the column to have the given name |
| `RELEASE_ALL_LOCKS()` | ❌ | Release all current named locks |
| `RELEASE_LOCK()` | ❌ | Release the named lock |
| `ROW_COUNT()` | ❌ | The number of rows updated |
| `SCHEMA()` | 🟡 | Limited synonym for `DATABASE()` in the same scalar-select slice |
| `SESSION_USER()` | ❌ | Synonym for USER() |
| `SLEEP()` | ❌ | Sleep for a number of seconds |
| `SYSTEM_USER()` | ❌ | Synonym for USER() |
| `UpdateXML()` | ❌ | Return replaced XML fragment |
| `USER()` | 🟡 | Limited one-row scalar select returns MyLite's embedded client identity `root@%`; no authentication, host matching, privileges, or table-backed evaluation |
| `VERSION()` | 🟡 | Limited one-row scalar `SELECT VERSION()` with optional `FROM DUAL`; returns MyLite's engine version string, not an impersonated MySQL server version; no `@@version`, aliases, table-backed evaluation, clauses, or protocol handshake version reporting |

[Back to compatibility overview](../../COMPATIBILITY.md)
