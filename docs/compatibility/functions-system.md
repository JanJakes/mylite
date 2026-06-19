# System functions

Session context, named locks, statement diagnostics, XML, IP, timing, file, and
miscellaneous system helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `BENCHMARK()` | ❌ | Repeatedly execute an expression |
| `CONNECTION_ID()` | 🟡 | Limited one-row scalar `SELECT CONNECTION_ID() [AS alias]` with optional `FROM DUAL`, limited `DO` expression execution, `SET @user_variable` assignment, and source-free DML value positions; returns a MyLite process-local nonzero handle id; no server thread, process-list, Performance Schema, `pseudo_thread_id`, table-backed evaluation, or protocol metadata |
| `CURRENT_ROLE()` | 🟡 | Limited one-row scalar `SELECT CURRENT_ROLE() [AS alias]` with optional `FROM DUAL` plus limited `DO` expression execution; returns MyLite's no-active-role value `NONE`; no role catalog, role grants, `SET ROLE`, bare `CURRENT_ROLE`, table-backed evaluation, clauses, or general expression support |
| `CURRENT_USER(), CURRENT_USER` | 🟡 | Limited one-row scalar select and `DO` expression execution return MyLite's embedded current identity `root@%`; limited `INFORMATION_SCHEMA.USER_PRIVILEGES` readback exposes synthetic global privileges for that identity, but there are no accounts, definers, roles, privilege enforcement, or table-backed evaluation |
| `DATABASE()` | ✅ | MySQL-runtime-verified selected-schema readback in no-source and `DUAL` scalar selects, limited `DO` expression execution, and source-backed row-scalar SELECT projection, `WHERE`, and `ORDER BY`; returns `NULL` when no schema is selected; stored-routine schema binding waits for stored-program support |
| `ExtractValue()` | ❌ | Extract a value from an XML string using XPath notation |
| `FOUND_ROWS()` | 🟡 | Limited zero-argument one-row scalar `SELECT FOUND_ROWS() [AS alias]` with optional `FROM DUAL`, plus limited `DO` expression execution, `SET @user_variable` assignment, and source-free DML value positions; returns connection-local found-row state for supported `SELECT` statements, records MySQL deprecation warning 1287 per invocation, and works with the limited `SQL_CALC_FOUND_ROWS` select modifier subset; no arguments, table-backed evaluation, protocol metadata, `CLIENT_FOUND_ROWS`, replication semantics, or full diagnostics-area behavior |
| `GET_LOCK()` | ❌ | Get a named lock |
| `ICU_VERSION()` | ❌ | ICU library version |
| `INET_ATON()` | ❌ | Return numeric value of an IP address |
| `INET_NTOA()` | ❌ | Return IP address from a numeric value |
| `IS_FREE_LOCK()` | ❌ | Whether the named lock is free |
| `IS_USED_LOCK()` | ❌ | Named-lock owner lookup |
| `LAST_INSERT_ID()` | 🟡 | Limited one-row scalar `SELECT LAST_INSERT_ID() [AS alias]` and `SELECT LAST_INSERT_ID(expr) [AS alias]` with optional `FROM DUAL`, plus limited `DO` expression execution, `SET @user_variable` assignment, and source-free DML value positions; zero-argument reads return the connection-local first generated id from the current auto-increment subset, while `expr` may be an integer, boolean, or `NULL` literal that stores unsigned 64-bit session state; auto-increment statement results expose the first generated id or, for explicit-only statements, the last positive explicit auto-increment value through MyLite result metadata and mysqli `insert_id`; no table-backed evaluation, warning-producing string/decimal/float conversion, wire-protocol insert-id packets, mixed-mode allocation parity, or stored-program behavior |
| `LOAD_FILE()` | ❌ | Load the named file |
| `NAME_CONST()` | ❌ | Cause the column to have the given name |
| `RELEASE_ALL_LOCKS()` | ❌ | Release all current named locks |
| `RELEASE_LOCK()` | ❌ | Release the named lock |
| `ROW_COUNT()` | 🟡 | Limited one-row scalar `SELECT ROW_COUNT() [AS alias]` with optional `FROM DUAL`, plus limited `DO` expression execution, `SET @user_variable` assignment, and source-free DML value positions; returns connection-local row-count state for supported baseline statements; no protocol OK-packet parity, `CLIENT_FOUND_ROWS`, table-backed evaluation, or full diagnostics-area support |
| `SCHEMA()` | ✅ | MySQL-runtime-verified synonym for `DATABASE()` in the documented scalar and row-scalar contexts |
| `SESSION_USER()` | 🟡 | Limited no-whitespace one-row scalar select and `DO` expression execution return MyLite's embedded client identity `root@%` as a `USER()` synonym; no `IGNORE_SPACE`, stored-function resolution, authentication, host matching, privilege enforcement, or table-backed evaluation |
| `SLEEP()` | ❌ | Sleep for a number of seconds |
| `SYSTEM_USER()` | 🟡 | Limited no-whitespace one-row scalar select and `DO` expression execution return MyLite's embedded client identity `root@%` as a `USER()` synonym; no `SYSTEM_USER` privilege semantics, authentication, host matching, privilege enforcement, or table-backed evaluation |
| `UpdateXML()` | ❌ | Return replaced XML fragment |
| `USER()` | 🟡 | Limited one-row scalar select and `DO` expression execution return MyLite's embedded client identity `root@%`; limited `INFORMATION_SCHEMA.USER_PRIVILEGES` readback exposes synthetic global privileges for that identity, but there is no authentication, host matching, privilege enforcement, or table-backed evaluation |
| `VERSION()` SQL-visible scalar identity | ✅ | MySQL-runtime-verified one-row scalar `SELECT VERSION() [AS alias]` with optional `FROM DUAL` returns the fixed MySQL 8.4.9 compatibility version string while the public `mylite_version()` API remains MyLite's library version |
| `VERSION()` broader identity coverage | 🟡 | Additional limited `DO` expression execution is supported, but table-backed evaluation, clauses, protocol handshake version reporting, and configurable server-version identity remain incomplete |

[Back to compatibility overview](../../COMPATIBILITY.md)
