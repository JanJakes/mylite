# SHOW VARIABLES

## Scope

This feature implements a focused first executable slice of `SHOW VARIABLES`:

- `SHOW VARIABLES`
- `SHOW GLOBAL VARIABLES`
- `SHOW SESSION VARIABLES`
- `SHOW LOCAL VARIABLES`
- all scope forms with `LIKE 'pattern'`
- all scope forms with `WHERE expr`, parsed but rejected at execution time

The slice exposes a practical catalog of high-value MySQL system variables that
MyLite can answer from current handle state, schema defaults, diagnostics
policy, fixed compatibility defaults, and the public version constant.

Deferred surfaces:

- execution of `SHOW VARIABLES ... WHERE expr`
- direct `SET` assignment to most system variables beyond the current
  `sql_mode` and `group_concat_max_len` slices
- `SELECT @@var_name`, `SELECT @@SESSION.var_name`, and
  `SELECT @@GLOBAL.var_name`
- complete MySQL server variable catalog
- variable metadata tables in Performance Schema
- privilege-sensitive or plugin/build-dependent variable behavior
- persisted variables and startup-option integration

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW VARIABLES` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-variables.html
- MySQL 8.4 Reference Manual, Extensions to `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- Runtime observations verified against `mylite-mysql-849`, MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW VARIABLES LIKE 'character_set_%'` | Columns `Variable_name`, `Value`; returns the session `character_set_*` rows plus `character_sets_dir` because the unescaped `_` also matches the `s` in `character_sets_dir`. |
| `SHOW SESSION VARIABLES LIKE 'character_set_%'` | Same session rows as the no-scope form. |
| `SHOW LOCAL VARIABLES LIKE 'character_set_%'` | Accepted; same rows as `SHOW SESSION VARIABLES`. |
| `SHOW GLOBAL VARIABLES LIKE 'character_set_%'` | Returns global defaults. In the verification container, client/connection/results values are `utf8mb4`, not the client session's initial `latin1`. |
| `SHOW VARIABLES LIKE 'character\_set\_%'` | Backslash escapes `_`, so only names with literal underscores after `character` and `set` match; `character_sets_dir` is excluded. |
| `SHOW VARIABLES LIKE 'CHARACTER\_SET\_%'` | Matches the same rows as the lowercase escaped pattern; variable-name pattern matching is case-insensitive on the verified runtime. |
| `SHOW VARIABLES LIKE 'collation_%'` | Returns `collation_connection`, `collation_database`, and `collation_server`. |
| `SHOW VARIABLES LIKE 'autocommit'` | Returns `autocommit`, `ON`. |
| `SHOW VARIABLES LIKE 'sql_mode'` | Returns `ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`. |
| `SHOW VARIABLES LIKE 'group_concat_max_len'` | Returns `group_concat_max_len`, `1024` by default. |
| `SET SESSION group_concat_max_len = 4; SHOW VARIABLES LIKE 'group_concat_max_len'` | Returns `group_concat_max_len`, `4`. |
| `SHOW VARIABLES LIKE 'warning_count'` | Returns `warning_count`, `0` for the statement itself. `SHOW VARIABLES` is nondiagnostic and clears a previous warning before reporting this variable. |
| `SHOW VARIABLES LIKE 'error_count'` | Returns `error_count`, `0` for the statement itself. |
| `SHOW GLOBAL VARIABLES LIKE 'warning_count'` | Returns no row because `warning_count` has no global value. |
| `SHOW GLOBAL VARIABLES LIKE 'error_count'` | Returns no row because `error_count` has no global value. |
| `SHOW VARIABLES LIKE 'max_error_count'` | Returns `max_error_count`, `1024`. |
| `SHOW VARIABLES LIKE 'sql_notes'` | Returns `sql_notes`, `ON`. |
| `SHOW VARIABLES LIKE 'version%'` | Returns version-related rows including `version`, `version_comment`, `version_compile_machine`, `version_compile_os`, and `version_compile_zlib` on the verification server. |
| `SET NAMES utf8mb4 COLLATE utf8mb4_bin; SHOW VARIABLES LIKE 'character\_set\_%'` | Session client/connection/results rows become `utf8mb4`; database/server remain `utf8mb4`; filesystem is `binary`; system is `utf8mb3`. |
| `SET NAMES utf8mb4 COLLATE utf8mb4_bin; SHOW VARIABLES LIKE 'collation_connection'` | Returns `utf8mb4_bin`. |
| `SET CHARACTER SET utf8mb4; SHOW VARIABLES LIKE 'collation_connection'` | Resets `collation_connection` to the selected database default, `utf8mb4_0900_ai_ci` in the no-selected-database verification case. |
| `SHOW VARIABLES LIKE 'no_such_variable'` | Returns no rows with stable two-column metadata. |
| `SHOW VARIABLES WHERE Variable_name = 'autocommit'` | Filters rows in MySQL and returns `autocommit`, `ON`; MyLite parses it but rejects execution until shared SHOW filtering exists. |
| `SHOW VARIABLES WHERE Value = 'ON'` | Filters against displayed column names and returns many rows, including `autocommit` and `sql_notes`. |
| `SHOW VARIABLES WHERE Variable_name LIKE 'character\_set\_%'` | Filters through the general `WHERE` path and returns the literal-underscore `character_set_*` rows. |
| `SHOW VARIABLES LIMIT 1` | Syntax error `1064`; `LIMIT` is not part of the statement syntax. |

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_variables_statement.

show_variables_statement ::= SHOW opt_show_variables_scope VARIABLES
                             opt_show_variables_filter.

opt_show_variables_scope ::= .
opt_show_variables_scope ::= GLOBAL.
opt_show_variables_scope ::= SESSION.
opt_show_variables_scope ::= LOCAL.

opt_show_variables_filter ::= .
opt_show_variables_filter ::= LIKE STRING.
opt_show_variables_filter ::= where_clause.
```

`GLOBAL`, `SESSION`, `LOCAL`, and `VARIABLES` must remain available as
nonreserved identifiers outside this production.

## AST

Add a `show_variables_statement` AST node with:

- a scope marker:
  - omitted, `SESSION`, and `LOCAL` normalize to session scope
  - `GLOBAL` uses global/default scope
- an optional string-literal `LIKE` pattern child or `WHERE` clause child

The statement must preserve the source span from `SHOW` through the last token.

## Runtime Semantics

Rows:

- The result set always has columns `Variable_name` and `Value`.
- Rows are ordered by variable name case-insensitively, then by binary name for
  deterministic tie-breaking.
- Successful `SHOW VARIABLES` produces no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only SQLite-backed result.
- `SHOW VARIABLES` is a nondiagnostic statement. Like MySQL, it clears prior
  diagnostics before producing rows, so `warning_count` and `error_count`
  report `0` for the statement itself.

Scope:

- Omitted scope, `SESSION`, and `LOCAL` expose session values.
- `GLOBAL` exposes default/global values when MyLite can represent them.
- Session-only diagnostic counters such as `warning_count` and `error_count`
  are omitted from `GLOBAL` results.
- MyLite has no process-global mutable state in this slice. For variables where
  MyLite has only fixed compatibility defaults, session and global values are
  intentionally the same.

Catalog for this slice:

| Variable | Session value | Global value | Notes |
| --- | --- | --- | --- |
| `autocommit` | `ON` | `ON` | MyLite currently implements autocommit-on behavior except inside explicit transaction statements; direct `SET autocommit` is deferred. |
| `character_set_client` | current handle state | `utf8mb4` | Updated by `SET NAMES` and `SET CHARACTER SET`. |
| `character_set_connection` | current handle state | `utf8mb4` | Updated by `SET NAMES` and selected-schema/default-schema behavior in `SET CHARACTER SET`. |
| `character_set_database` | selected schema default, or server default when no schema is selected | `utf8mb4` | Backed by MyLite schema defaults for session scope. |
| `character_set_filesystem` | `binary` | `binary` | Fixed compatibility value. |
| `character_set_results` | current handle state | `utf8mb4` | Updated by `SET NAMES` and `SET CHARACTER SET`. |
| `character_set_server` | `utf8mb4` | `utf8mb4` | MyLite's current server default. |
| `character_set_system` | `utf8mb3` | `utf8mb3` | Fixed MySQL-compatible system charset value. |
| `character_sets_dir` | empty string | empty string | MyLite uses an embedded registry rather than a filesystem charset directory. |
| `collation_connection` | current handle state | `utf8mb4_0900_ai_ci` | Updated by `SET NAMES` and `SET CHARACTER SET`. |
| `collation_database` | selected schema default, or server default when no schema is selected | `utf8mb4_0900_ai_ci` | Backed by MyLite schema defaults for session scope. |
| `collation_server` | `utf8mb4_0900_ai_ci` | `utf8mb4_0900_ai_ci` | MyLite's current server default collation. |
| `error_count` | `0` | omitted | SHOW VARIABLES clears prior diagnostics before reporting. |
| `group_concat_max_len` | current handle state, default `1024` | `1024` | Session/local assignment is implemented; global mutation is deferred. |
| `max_error_count` | `1024` | `1024` | Storage cap behavior remains deferred; diagnostics currently store all generated conditions in memory. |
| `sql_mode` | current handle state, default MySQL 8.4 mode string | MySQL 8.4 default mode string | Session/local assignment is implemented for recognized modes and the focused `REPLACE(...)` removal idiom. Implemented expression/DDL/DML behavior may still be narrower than the full mode surface. |
| `sql_notes` | `ON` | `ON` | Direct assignment is deferred; current implemented note paths behave as enabled. |
| `transaction_isolation` | `REPEATABLE-READ` | `REPEATABLE-READ` | Fixed compatibility value; isolation-level semantics are deferred. |
| `transaction_read_only` | `OFF` | `OFF` | This variable is the default transaction mode, not the current active transaction access mode. |
| `version` | `mylite_version()` | same as session | Public MyLite compatibility version. |
| `version_comment` | `MyLite` | same as session | Embedded runtime identification. |
| `version_compile_machine` | empty string | empty string | No stable compile-machine ABI value is exposed yet. |
| `version_compile_os` | empty string | empty string | No stable compile-OS ABI value is exposed yet. |
| `version_compile_zlib` | empty string | empty string | MyLite does not expose a stable zlib version value yet. |

LIKE filtering:

- `%` matches any byte sequence.
- `_` matches one byte.
- Backslash escapes the following byte for SHOW-pattern purposes.
- Matching is case-insensitive for variable names, matching verified MySQL
  behavior for this statement.

WHERE filtering:

- The parser accepts `WHERE expr` because MySQL supports general SHOW filters
  over the displayed result columns.
- Execution returns `MYLITE_UNSUPPORTED` with message
  `SHOW VARIABLES WHERE is not supported` until shared SHOW result-set
  filtering lands.

## Storage And Performance

This feature is read-only and requires no file format change. Runtime execution
materializes a small deterministic in-memory variable catalog into a SQLite
read statement. The catalog is intentionally small in this first slice; adding
new variables should remain cheap and explicit.

## Tests

Parser coverage:

- `SHOW VARIABLES`
- `SHOW GLOBAL VARIABLES`
- `SHOW SESSION VARIABLES`
- `SHOW LOCAL VARIABLES`
- scope forms with `LIKE`
- `SHOW VARIABLES WHERE Variable_name = 'autocommit'`
- `SHOW VARIABLES WHERE Value = 'ON'`
- `GLOBAL`, `SESSION`, `LOCAL`, and `VARIABLES` as unquoted identifiers where
  the grammar permits them
- syntax rejection for non-string `LIKE`, combined `LIKE` plus `WHERE`,
  `SHOW VARIABLES LIMIT 1`, repeated scope modifiers, and trailing scope
  modifiers

Runtime coverage:

- exact result column names
- unfiltered catalog contains expected high-value rows
- deterministic row ordering
- `LIKE` exact, wildcard, escaped underscore, case-insensitive, and empty
  filtering
- `SESSION`, omitted scope, and `LOCAL` return session values
- `GLOBAL` returns default/global values for connection charset and collation
  variables and omits session-only diagnostic counters
- `SET NAMES` changes session charset/collation rows without changing global
  rows
- `SET CHARACTER SET` reflects selected-schema/default-schema connection
  collation behavior
- selected schema defaults change `character_set_database` and
  `collation_database`
- `SET SESSION group_concat_max_len` changes the session row without changing
  the global/default row
- `WHERE` returns the clear unsupported diagnostic
- `LIMIT` remains a syntax error
- `SHOW VARIABLES` clears prior diagnostics before reporting `warning_count`
  and `error_count` as `0`

## Deferred Work

- Full MySQL system-variable catalog and metadata.
- General `SHOW ... WHERE` filtering.
- General system-variable assignment and validation beyond `sql_mode` and
  `group_concat_max_len`.
- SQL system-variable expression forms through `@@`.
- Mutable `sql_notes`, `max_error_count`, and autocommit state.
- Complete warning/error generated-condition counters under a future
  `max_error_count` cap.
- Performance Schema system-variable tables and protocol-level variable
  exposure.
