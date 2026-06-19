# System functions

Session context, named locks, statement diagnostics, XML, IP, timing, file, and
miscellaneous system helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `BENCHMARK()` | ❌ | Repeatedly execute an expression |
| `CONNECTION_ID()` | ✅ | MySQL-runtime-verified MyLite handle-local connection id readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, `SET @user_variable`, source-free DML value positions, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; server thread, process-list, Performance Schema, protocol, and `pseudo_thread_id` identity semantics are tracked outside this function row |
| `CURRENT_ROLE()` | ✅ | MySQL-runtime-verified no-active-role `CURRENT_ROLE()` readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; role catalog, role grants, `SET ROLE`, active-role state, bare `CURRENT_ROLE`, and general expression gaps are tracked outside this function row |
| `CURRENT_USER(), CURRENT_USER` | ✅ | MySQL-runtime-verified embedded current identity `root@%` readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; accounts, definers, roles, and privilege enforcement are tracked outside this function row |
| `DATABASE()` | ✅ | MySQL-runtime-verified selected-schema readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; returns `NULL` when no schema is selected; stored-routine schema binding waits for stored-program support |
| `ExtractValue()` | ❌ | Extract a value from an XML string using XPath notation |
| `FOUND_ROWS()` | ✅ | MySQL-runtime-verified zero-argument readback in no-source and `DUAL` scalar selects, limited `DO`, `SET @user_variable`, source-free DML value positions, source-backed row-scalar projection, `WHERE`, and `ORDER BY`; records MySQL deprecation warning 1287 per supported invocation and works with the limited `SQL_CALC_FOUND_ROWS` subset; protocol metadata, `CLIENT_FOUND_ROWS`, replication semantics, and full diagnostics-area behavior are tracked separately |
| `GET_LOCK()` | ❌ | Get a named lock |
| `ICU_VERSION()` | ❌ | ICU library version |
| `INET_ATON()` | ✅ | MySQL-runtime-verified IPv4 dotted-address to unsigned-integer conversion in scalar and row-scalar expression contexts; IPv6 helpers remain separate gaps |
| `INET_NTOA()` | ✅ | MySQL-runtime-verified unsigned-integer to IPv4 dotted-address conversion in scalar and row-scalar expression contexts; IPv6 helpers remain separate gaps |
| `IS_FREE_LOCK()` | ❌ | Whether the named lock is free |
| `IS_USED_LOCK()` | ❌ | Named-lock owner lookup |
| `LAST_INSERT_ID()` | ✅ | MySQL-runtime-verified zero-argument readback plus supported `LAST_INSERT_ID(expr)` state-setting forms in scalar and row-scalar expression contexts, including sequence-style single-table `UPDATE`; warning-producing string/decimal/float conversion, wire-protocol insert-id packets, mixed-mode allocation parity, and stored-program behavior are tracked separately |
| `LOAD_FILE()` | ❌ | Load the named file |
| `NAME_CONST()` | ❌ | Cause the column to have the given name |
| `RELEASE_ALL_LOCKS()` | ❌ | Release all current named locks |
| `RELEASE_LOCK()` | ❌ | Release the named lock |
| `ROW_COUNT()` | ✅ | MySQL-runtime-verified zero-argument readback in no-source and `DUAL` scalar selects, limited `DO`, `SET @user_variable`, source-free DML value positions, and source-backed row-scalar projection, `WHERE`, and `ORDER BY`; returns connection-local row-count state for supported baseline statements; protocol OK-packet parity, `CLIENT_FOUND_ROWS`, and full diagnostics-area behavior are tracked separately |
| `SCHEMA()` | ✅ | MySQL-runtime-verified synonym for `DATABASE()` in the documented scalar and row-scalar contexts |
| `SESSION_USER()` | ✅ | MySQL-runtime-verified no-whitespace embedded client identity `root@%` readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; `IGNORE_SPACE`, stored-function resolution, authentication, host matching, and privilege enforcement are tracked outside this function row |
| `SLEEP()` | ❌ | Sleep for a number of seconds |
| `SYSTEM_USER()` | ✅ | MySQL-runtime-verified no-whitespace embedded client identity `root@%` readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; `SYSTEM_USER` privilege semantics, authentication, host matching, and privilege enforcement are tracked outside this function row |
| `UpdateXML()` | ❌ | Return replaced XML fragment |
| `USER()` | ✅ | MySQL-runtime-verified embedded client identity `root@%` readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; authentication, host matching, and privilege enforcement are tracked outside this function row |
| `VERSION()` | ✅ | MySQL-runtime-verified SQL-visible MySQL 8.4.9 version readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; protocol handshake and configurable server-version identity are tracked outside this function row |

[Back to compatibility overview](../../COMPATIBILITY.md)
