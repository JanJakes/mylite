# SQL mode and ONLY_FULL_GROUP_BY

## Scope

This feature adds session `sql_mode` state for the MySQL-compatible grouping
behaviors that depend on `ONLY_FULL_GROUP_BY`:

- default session mode matching MySQL 8.4.9:
  `ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`
- `SET [SESSION|LOCAL] sql_mode = '<mode-list>'`
- `SET [SESSION|LOCAL] sql_mode = DEFAULT`
- `SET @@sql_mode = ...`, `SET @@session.sql_mode = ...`, and
  `SET @@local.sql_mode = ...`
- `SET [SESSION|LOCAL] sql_mode =
  REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '')`
- `SHOW [SESSION|LOCAL] VARIABLES LIKE 'sql_mode'` exposing the session value
- `SHOW GLOBAL VARIABLES LIKE 'sql_mode'` exposing the immutable MyLite default
- `GROUP BY` strict validation only when `ONLY_FULL_GROUP_BY` is active
- `SELECT DISTINCT ... ORDER BY` hidden-expression validation only when
  `ONLY_FULL_GROUP_BY` is active

The feature does not attempt to make every SQL mode affect its broader MySQL
surface. It stores and canonicalizes recognized mode names so applications can
toggle `ONLY_FULL_GROUP_BY` without losing common mode-list compatibility.
Other mode-specific behavior remains owned by its feature area.

## Sources

- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, `SET` Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- MySQL 8.4 Reference Manual, MySQL Handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4 Reference Manual, `DISTINCT` Optimization:
  https://dev.mysql.com/doc/refman/8.4/en/distinct-optimization.html

Runtime observations were verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849-regexp`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy external implementation
sources.

## MySQL 8.4.9 observations

The verified default session and global SQL mode is:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

`SET SESSION sql_mode = ''` removes all session modes. `SET SESSION
sql_mode = DEFAULT` restores the current global value. Duplicate modes collapse
to one canonical entry, and lowercase spellings are normalized. Unknown modes
raise error 1231 with text beginning `Variable 'sql_mode' can't be set`.
Common removal through
`REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '')` evaluates the current
session value at statement execution time.

With `ONLY_FULL_GROUP_BY` enabled:

- `SELECT a, SUM(b) FROM t GROUP BY a ORDER BY b` fails with error 1055.
- `SELECT DISTINCT a FROM t ORDER BY b` fails with error 3065.
- `SELECT id, name FROM t GROUP BY id` is accepted when `id` is a primary key
  or every column of a non-null unique key is grouped.

With `ONLY_FULL_GROUP_BY` absent, both statements are accepted. Nonaggregated
values from grouped rows follow MySQL's non-strict representative-row behavior
and are not guaranteed by SQL semantics; tests should use fixtures where the
representative is deterministic for MyLite's current row materialization.

## Syntax

MyLite-owned Lemon grammar shape:

```lemon
set_sql_mode_statement ::= SET opt_set_sql_mode_scope set_sql_mode_variable EQ
    set_sql_mode_value.

opt_set_sql_mode_scope ::= .
opt_set_sql_mode_scope ::= SESSION.
opt_set_sql_mode_scope ::= LOCAL.

set_sql_mode_variable ::= identifier.
set_sql_mode_variable ::= SYSTEM_VARIABLE.

set_sql_mode_value ::= STRING.
set_sql_mode_value ::= DEFAULT.
set_sql_mode_value ::= REPLACE LPAREN set_sql_mode_variable COMMA STRING
    COMMA STRING RPAREN.
```

Only session/local assignment is supported. `GLOBAL` assignment remains
unsupported because MyLite has no mutable server-global state.

## Semantics

`mylite_db` owns the session SQL mode string. Connection open initializes it to
the MySQL 8.4.9 default. Closing the connection frees it.

Assignment canonicalizes recognized modes in MySQL display order, expands
combination modes such as `ANSI` and `TRADITIONAL`, removes duplicates, and
preserves an empty list as the empty string. Unknown mode names fail before
mutating the connection state.

`SHOW VARIABLES` reads the session value for session/local scope and the
immutable default for global scope.

The grouping and distinct-order validators must query the session state:

- if `ONLY_FULL_GROUP_BY` is present, keep existing MySQL-compatible strict
  validation and diagnostics
- if absent, skip those validators and let execution use the existing grouped
  and distinct rowset machinery

Strict grouping validation treats all columns of a table as functionally
dependent when every part of a primary key or non-null unique key for that
table is grouped. Nullable unique keys and prefix/expression key parts do not
prove functional dependence in this slice.

## Tests

Required coverage:

- parser accepts session/local `sql_mode` assignments and `@@session.sql_mode`
- default `SHOW VARIABLES` exposes the MySQL 8.4.9 default
- `SET SESSION sql_mode = ''` exposes an empty session value without changing
  the global value
- `SET SESSION sql_mode =
  REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '')` removes only the mode
  token from the current session value
- lowercase and duplicate mode lists canonicalize
- `SET SESSION sql_mode = DEFAULT` restores the default
- unknown modes fail with error 1231-style diagnostics
- grouped primary-key and non-null unique-key queries are accepted under
  `ONLY_FULL_GROUP_BY`
- grouped query with hidden non-grouped select/order expressions fails with
  `ONLY_FULL_GROUP_BY` enabled and succeeds when absent
- `SELECT DISTINCT ... ORDER BY` hidden expressions fail with
  `ONLY_FULL_GROUP_BY` enabled and succeed when absent

## Compatibility status

After implementation, `SHOW VARIABLES` and `SELECT` compatibility text should
state that session `sql_mode` is stored and exposed for recognized mode names,
with `ONLY_FULL_GROUP_BY` actively controlling the relevant grouping and
`DISTINCT` ordering validators. Other SQL mode effects remain scoped to their
individual feature implementations.
