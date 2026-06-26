# Baseline InnoDB Full-Text System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata placeholders for these InnoDB
full-text system variables:

- `innodb_ft_aux_table`
- `innodb_ft_cache_size`
- `innodb_ft_enable_diag_print`
- `innodb_ft_enable_stopword`
- `innodb_ft_max_token_size`
- `innodb_ft_min_token_size`
- `innodb_ft_num_word_optimize`
- `innodb_ft_result_cache_limit`
- `innodb_ft_server_stopword_table`
- `innodb_ft_sort_pll_degree`
- `innodb_ft_total_cache_size`
- `innodb_ft_user_stopword_table`

The goal is baseline compatibility for scalar reads, `SHOW VARIABLES` rows,
scope diagnostics, read-only diagnostics, default/null table-name assignment
handling, and the session-local `innodb_ft_enable_stopword` read/write path.
This does not implement physical InnoDB full-text indexing, auxiliary table row
selection, stopword table validation, diagnostic printing, cache sizing, token
limits, result-cache sizing, or optimize batch behavior.

## MySQL 8.4.9 Observations

Runtime probes against MySQL 8.4.9 showed these defaults and scopes:

| Variable | Scalar value | SHOW value | Scope | Mutation |
| --- | --- | --- | --- | --- |
| `innodb_ft_aux_table` | `NULL` | empty | global | dynamic global |
| `innodb_ft_cache_size` | `8000000` | `8000000` | global | read-only |
| `innodb_ft_enable_diag_print` | `0` | `OFF` | global | dynamic global |
| `innodb_ft_enable_stopword` | `1` | `ON` | global/session | dynamic session and global |
| `innodb_ft_max_token_size` | `84` | `84` | global | read-only |
| `innodb_ft_min_token_size` | `3` | `3` | global | read-only |
| `innodb_ft_num_word_optimize` | `2000` | `2000` | global | dynamic global |
| `innodb_ft_result_cache_limit` | `2000000000` | `2000000000` | global | dynamic global |
| `innodb_ft_server_stopword_table` | `NULL` | empty | global | dynamic global |
| `innodb_ft_sort_pll_degree` | `2` | `2` | global | read-only |
| `innodb_ft_total_cache_size` | `640000000` | `640000000` | global | read-only |
| `innodb_ft_user_stopword_table` | `NULL` | empty | global/session | dynamic session and global |

All variables appear in default, global, and session `SHOW VARIABLES` output.
For global-only variables, scalar `@@SESSION` and `@@LOCAL` reads fail with
`1238/HY000` and text containing `Variable '<name>' is a GLOBAL variable`.

For global-only dynamic variables, unqualified, `SESSION`, and `LOCAL`
assignment fail with `1229/HY000` and text containing `GLOBAL variable and
should be set with SET GLOBAL`. For read-only variables, every assignment scope
fails with `1238/HY000` and text containing `read only variable`.

`innodb_ft_aux_table`, `innodb_ft_server_stopword_table`, and
`innodb_ft_user_stopword_table` accept `DEFAULT` and `NULL` for their supported
scopes when no auxiliary/stopword table is selected. Empty strings and
unresolved table names fail with `1231/42000` and text containing
`can't be set to the value`.

`innodb_ft_enable_stopword` accepts session `ON`/`OFF` changes and reports the
session value through unqualified and session scalar reads plus default/session
`SHOW VARIABLES`. MySQL also accepts global alternate values for mutable
full-text variables; MyLite's embedded baseline intentionally keeps server
global state fixed.

## MyLite Behavior

MyLite exposes fixed placeholder values matching the observed defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

`@@SESSION.variable` and `@@LOCAL.variable` return the MySQL-shaped
global-variable diagnostic for every global-only variable in this slice.

For dynamic global-only variables, MyLite accepts `SET GLOBAL variable =
DEFAULT` and exact fixed-value `SET GLOBAL` assignments as no-ops. For nullable
full-text table-name variables, MyLite accepts `DEFAULT` and `NULL` as no-ops
and rejects non-null table names with MySQL-shaped `1231/42000` diagnostics.

For `innodb_ft_enable_stopword`, MyLite supports handle-local unqualified,
`SESSION`, and `LOCAL` boolean assignments with scalar and `SHOW` readback.
`SET GLOBAL innodb_ft_enable_stopword` accepts only the default `ON`/`1` value
as a fixed no-op.

For read-only variables, every direct and user-variable assignment path returns
MySQL-shaped read-only diagnostics before value validation.

## Non-Goals

- InnoDB full-text auxiliary table selection or rows in `INFORMATION_SCHEMA`
  full-text auxiliary views.
- Stopword table existence validation, loading, or full-text index build
  effects.
- Physical full-text cache allocation, result-cache sizing, token parser
  changes, optimize batch behavior, or diagnostic logging.
- Mutable shared global state, persisted variables, startup option handling,
  privilege checks, Performance Schema variable tables, or cross-connection
  visibility.
- Full validation or mutation semantics for alternate MySQL-accepted global
  runtime values beyond exact/default no-ops.

## SQLite Integration

No SQLite fork or new SQLite extension point is required. This is MyLite runtime
metadata and diagnostics around system-variable reads and assignment syntax.
