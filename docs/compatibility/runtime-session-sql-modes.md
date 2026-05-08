# Runtime session state and SQL modes

Session-local MySQL state and SQL modes that affect parsing, coercion, diagnostics, and runtime behavior.

## Session and runtime state

| Feature | Status | Notes |
| --- | --- | --- |
| Default schema | ❌ | Current schema state |
| SQL mode scalar value | 🟡 | Limited read-only `@@sql_mode` scalar reads expose MySQL 8.4.9's default mode string; no `SET`, mutable mode state, parser effects, conversion effects, or statement behavior changes |
| Connection character set state | ❌ | Connection charset/collation state |
| Time zone state | ❌ | Time zone variables and conversion |
| Autocommit state | 🟡 | Limited scalar `@@autocommit` reads report fixed enabled value `1`; no mutable `SET autocommit`, transaction boundaries, commit/rollback behavior, or protocol status flags |
| Last insert id | ❌ | Insert-id state and packets |
| Affected rows | ❌ | Found/changed row counts |
| Warnings and diagnostics | ❌ | Warnings, errors, diagnostics area |
| Prepared statement registry | ❌ | Per-connection prepared statements |
| Temporary table namespace | ❌ | Per-session temporary table metadata and shadowing |
| Role and privilege state | ❌ | Role and privilege state |
| Locks | ❌ | Named, table, metadata, backup locks |

## SQL modes

| SQL mode | Status | Notes |
| --- | --- | --- |
| `ALLOW_INVALID_DATES` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `ANSI` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `ANSI_QUOTES` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `ERROR_FOR_DIVISION_BY_ZERO` | ❌ | Name appears in the limited read-only default `@@sql_mode` string; mode effects on parsing, DDL/DML, coercion, and diagnostics remain unsupported |
| `HIGH_NOT_PRECEDENCE` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `IGNORE_SPACE` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_AUTO_VALUE_ON_ZERO` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_BACKSLASH_ESCAPES` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_DIR_IN_CREATE` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_ENGINE_SUBSTITUTION` | ❌ | Name appears in the limited read-only default `@@sql_mode` string; mode effects on parsing, DDL/DML, coercion, and diagnostics remain unsupported |
| `NO_UNSIGNED_SUBTRACTION` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_ZERO_DATE` | ❌ | Name appears in the limited read-only default `@@sql_mode` string; mode effects on parsing, DDL/DML, coercion, and diagnostics remain unsupported |
| `NO_ZERO_IN_DATE` | ❌ | Name appears in the limited read-only default `@@sql_mode` string; mode effects on parsing, DDL/DML, coercion, and diagnostics remain unsupported |
| `ONLY_FULL_GROUP_BY` | ❌ | Name appears in the limited read-only default `@@sql_mode` string; mode effects on parsing, DDL/DML, coercion, and diagnostics remain unsupported |
| `PAD_CHAR_TO_FULL_LENGTH` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `PIPES_AS_CONCAT` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `REAL_AS_FLOAT` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `STRICT_ALL_TABLES` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `STRICT_TRANS_TABLES` | ❌ | Name appears in the limited read-only default `@@sql_mode` string; mode effects on parsing, DDL/DML, coercion, and diagnostics remain unsupported |
| `TIME_TRUNCATE_FRACTIONAL` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `TRADITIONAL` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
