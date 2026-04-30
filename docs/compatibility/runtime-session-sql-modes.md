# Runtime session state and SQL modes

Session-local MySQL state and SQL modes that affect parsing, coercion, diagnostics, and runtime behavior.

## Session and runtime state

| Feature | Status | Notes |
| --- | --- | --- |
| Default schema | ❌ | Current schema state |
| Connection character set state | ❌ | Connection charset/collation state |
| Time zone state | ❌ | Time zone variables and conversion |
| Autocommit state | ❌ | Autocommit and transaction boundaries |
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
| `ERROR_FOR_DIVISION_BY_ZERO` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `HIGH_NOT_PRECEDENCE` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `IGNORE_SPACE` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_AUTO_VALUE_ON_ZERO` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_BACKSLASH_ESCAPES` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_DIR_IN_CREATE` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_ENGINE_SUBSTITUTION` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_UNSIGNED_SUBTRACTION` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_ZERO_DATE` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `NO_ZERO_IN_DATE` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `ONLY_FULL_GROUP_BY` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `PAD_CHAR_TO_FULL_LENGTH` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `PIPES_AS_CONCAT` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `REAL_AS_FLOAT` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `STRICT_ALL_TABLES` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `STRICT_TRANS_TABLES` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `TIME_TRUNCATE_FRACTIONAL` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |
| `TRADITIONAL` | ❌ | Mode effects on parsing, DDL/DML, coercion, diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
