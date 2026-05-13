# Runtime session state and SQL modes

Session-local MySQL state and SQL modes that affect parsing, coercion, diagnostics, and runtime behavior.

## Session and runtime state

| Feature | Status | Notes |
| --- | --- | --- |
| Default schema | ❌ | Current schema state |
| SQL mode scalar value | 🟡 | Limited session-local `@@sql_mode`, `@@SESSION.sql_mode`, `@@LOCAL.sql_mode`, `SET sql_mode`, and `SHOW VARIABLES` support with fixed MySQL 8.4.9 default/global readback, canonical assigned mode strings, and behavioral effects for `ANSI_QUOTES`, `NO_BACKSLASH_ESCAPES`, `NO_AUTO_VALUE_ON_ZERO`, `REAL_AS_FLOAT`, plus the current canonical `DATE` / `DATETIME` / `TIMESTAMP` zero-temporal subset for `STRICT_TRANS_TABLES`, `STRICT_ALL_TABLES`, `NO_ZERO_DATE`, `NO_ZERO_IN_DATE`, and `ALLOW_INVALID_DATES`; no mutable global state, full strict/non-strict conversion coverage, or broader mode behavior |
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
| `ALLOW_INVALID_DATES` | 🟡 | Accepted and reflected in session `@@sql_mode`; limited effect for canonical `DATE` / `DATETIME` row values, defaults, compatible descriptor copies, and predicates whose year is inside MyLite's current `1000..9999` range, month is `1..12`, and day is `1..31`; no effect for `TIMESTAMP`, relaxed temporal strings, pre-1000 non-full-zero dates, functions, or general conversion |
| `ANSI` | 🟡 | Accepted and reflected as a composite session mode; only its `ANSI_QUOTES` and `REAL_AS_FLOAT` effects are currently implemented |
| `ANSI_QUOTES` | 🟡 | Limited lexer effect: double-quoted tokens in later statements are parsed as quoted identifiers rather than string literals |
| `ERROR_FOR_DIVISION_BY_ZERO` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; division-by-zero and strict-mode effects remain unsupported |
| `HIGH_NOT_PRECEDENCE` | 🟡 | Accepted and reflected in session `@@sql_mode`; operator-precedence effects remain unsupported |
| `IGNORE_SPACE` | 🟡 | Accepted and reflected in session `@@sql_mode`; limited function-name parsing effect for the currently admitted built-in scalar-function slices; no full reserved-function-name parsing parity |
| `NO_AUTO_VALUE_ON_ZERO` | 🟡 | Limited DML effect: explicit `0` inserted into current descriptor-owned `AUTO_INCREMENT` paths stores zero instead of being replaced by a generated value; exact InnoDB-style counter reservation gaps after all prior insert patterns remain outside the current slice |
| `NO_BACKSLASH_ESCAPES` | 🟡 | Limited lexer and string-decoding effect for later admitted string literals; broader string and pattern semantics are still unsupported |
| `NO_DIR_IN_CREATE` | 🟡 | Accepted and reflected in session `@@sql_mode`; directory-option effects remain unsupported |
| `NO_ENGINE_SUBSTITUTION` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; storage-engine substitution effects remain unsupported |
| `NO_UNSIGNED_SUBTRACTION` | 🟡 | Accepted and reflected in session `@@sql_mode`; arithmetic type effects remain unsupported |
| `NO_ZERO_DATE` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; limited effect for full-zero canonical `DATE`, `DATETIME`, and `TIMESTAMP` row values, defaults, compatible descriptor copies, and predicates, including strict errors and nonstrict warning-plus-zero behavior for the supported subset; no relaxed temporal strings, pre-1000 non-full-zero dates, functions, or general conversion |
| `NO_ZERO_IN_DATE` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; limited effect for canonical partial-zero `DATE` and `DATETIME` row values, defaults, compatible descriptor copies, and predicates inside MyLite's current `1000..9999` year envelope, including strict errors and nonstrict warning-plus-full-zero behavior; no effect for `TIMESTAMP`, relaxed temporal strings, pre-1000 non-full-zero dates, functions, or general conversion |
| `ONLY_FULL_GROUP_BY` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; grouping validation effects remain unsupported |
| `PAD_CHAR_TO_FULL_LENGTH` | 🟡 | Accepted and reflected in session `@@sql_mode` and emits the verified MySQL deprecation warning; `CHAR` readback padding effects remain unsupported |
| `PIPES_AS_CONCAT` | 🟡 | Accepted and reflected in session `@@sql_mode`; `||` string-concatenation parsing remains unsupported |
| `REAL_AS_FLOAT` | 🟡 | Limited DDL effect: later `REAL` column definitions map to MyLite's `FLOAT` descriptor subset instead of `DOUBLE` |
| `STRICT_ALL_TABLES` | 🟡 | Accepted and reflected in session `@@sql_mode`; limited strict conversion effect for the current canonical zero-temporal `DATE` / `DATETIME` / `TIMESTAMP` subset when combined with `NO_ZERO_DATE`, `NO_ZERO_IN_DATE`, or invalid temporal inputs; broader strict conversion and warning escalation effects remain unsupported |
| `STRICT_TRANS_TABLES` | 🟡 | Accepted and reflected in session `@@sql_mode` and appears in the default/global value; limited strict conversion effect for the current canonical zero-temporal `DATE` / `DATETIME` / `TIMESTAMP` subset when combined with `NO_ZERO_DATE`, `NO_ZERO_IN_DATE`, or invalid temporal inputs; broader strict conversion and warning escalation effects remain unsupported |
| `TIME_TRUNCATE_FRACTIONAL` | 🟡 | Accepted and reflected in session `@@sql_mode`; fractional-time truncation effects remain unsupported |
| `TRADITIONAL` | 🟡 | Accepted and reflected as a composite session mode; only the currently implemented constituent effects are observable, including the limited zero-temporal strict behavior |

[Back to compatibility overview](../../COMPATIBILITY.md)
