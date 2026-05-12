# Runtime session state and SQL modes

Session-local MySQL state and SQL modes that affect parsing, coercion, diagnostics, and runtime behavior.

## Session and runtime state

| Feature | Status | Notes |
| --- | --- | --- |
| Default schema | ❌ | Current schema state |
| SQL mode scalar value | 🟡 | Limited session-local `@@sql_mode`, `@@SESSION.sql_mode`, `@@LOCAL.sql_mode`, `SET sql_mode`, and `SHOW VARIABLES` support with fixed MySQL 8.4.9 default/global readback, canonical assigned mode strings, and behavioral effects only for `ANSI_QUOTES`, `NO_BACKSLASH_ESCAPES`, `NO_AUTO_VALUE_ON_ZERO`, and `REAL_AS_FLOAT`; no mutable global state, strict/non-strict conversion effects, or broader mode behavior |
| Connection character set state | 🟡 | Fixed `utf8mb4` / `utf8mb4_0900_ai_ci` baseline with limited scalar reads and no-op `SET NAMES` / `SET CHARACTER SET` forms that preserve that baseline; no mutable conversion state |
| Time zone state | ❌ | Time zone variables and conversion |
| Autocommit state | 🟡 | Limited scalar `@@autocommit` reads and fixed no-op `SET autocommit = 1` forms report/preserve fixed enabled value `1`; no mutable `SET autocommit = 0`, transaction boundaries, commit/rollback behavior, or protocol status flags |
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
| `ALLOW_INVALID_DATES` | 🟡 | Accepted and reflected in session `@@sql_mode`; invalid-date conversion effects remain unsupported |
| `ANSI` | 🟡 | Accepted and reflected as a composite session mode; only its `ANSI_QUOTES` and `REAL_AS_FLOAT` effects are currently implemented |
| `ANSI_QUOTES` | 🟡 | Limited lexer effect: double-quoted tokens in later statements are parsed as quoted identifiers rather than string literals |
| `ERROR_FOR_DIVISION_BY_ZERO` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; division-by-zero and strict-mode effects remain unsupported |
| `HIGH_NOT_PRECEDENCE` | 🟡 | Accepted and reflected in session `@@sql_mode`; operator-precedence effects remain unsupported |
| `IGNORE_SPACE` | 🟡 | Accepted and reflected in session `@@sql_mode`; function-name parsing effects remain unsupported |
| `NO_AUTO_VALUE_ON_ZERO` | 🟡 | Limited DML effect: explicit `0` inserted into current descriptor-owned `AUTO_INCREMENT` paths stores zero instead of being replaced by a generated value; exact InnoDB-style counter reservation gaps after all prior insert patterns remain outside the current slice |
| `NO_BACKSLASH_ESCAPES` | 🟡 | Limited lexer and string-decoding effect for later admitted string literals; broader string and pattern semantics are still unsupported |
| `NO_DIR_IN_CREATE` | 🟡 | Accepted and reflected in session `@@sql_mode`; directory-option effects remain unsupported |
| `NO_ENGINE_SUBSTITUTION` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; storage-engine substitution effects remain unsupported |
| `NO_UNSIGNED_SUBTRACTION` | 🟡 | Accepted and reflected in session `@@sql_mode`; arithmetic type effects remain unsupported |
| `NO_ZERO_DATE` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; zero-date conversion and strict-mode effects remain unsupported |
| `NO_ZERO_IN_DATE` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; zero-in-date conversion and strict-mode effects remain unsupported |
| `ONLY_FULL_GROUP_BY` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; grouping validation effects remain unsupported |
| `PAD_CHAR_TO_FULL_LENGTH` | 🟡 | Accepted and reflected in session `@@sql_mode` and emits the verified MySQL deprecation warning; `CHAR` readback padding effects remain unsupported |
| `PIPES_AS_CONCAT` | 🟡 | Accepted and reflected in session `@@sql_mode`; `||` string-concatenation parsing remains unsupported |
| `REAL_AS_FLOAT` | 🟡 | Limited DDL effect: later `REAL` column definitions map to MyLite's `FLOAT` descriptor subset instead of `DOUBLE` |
| `STRICT_ALL_TABLES` | 🟡 | Accepted and reflected in session `@@sql_mode`; strict conversion and warning escalation effects remain unsupported |
| `STRICT_TRANS_TABLES` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; strict conversion and warning escalation effects remain unsupported |
| `TIME_TRUNCATE_FRACTIONAL` | 🟡 | Accepted and reflected in session `@@sql_mode`; fractional-time truncation effects remain unsupported |
| `TRADITIONAL` | 🟡 | Accepted and reflected as a composite session mode, but its strict conversion effects remain unsupported |

[Back to compatibility overview](../../COMPATIBILITY.md)
