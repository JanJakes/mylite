# SELECT Ordering Compatibility and ONLY_FULL_GROUP_BY

## Scope

This slice makes existing SELECT validation sensitive to the session
`sql_mode`, specifically the `ONLY_FULL_GROUP_BY` mode:

- default session `sql_mode` includes `ONLY_FULL_GROUP_BY`
- `GROUP BY` and aggregate hidden-column validation remains strict while
  `ONLY_FULL_GROUP_BY` is enabled
- `SELECT DISTINCT ... ORDER BY` hidden-column validation remains strict while
  `ONLY_FULL_GROUP_BY` is enabled
- clearing `ONLY_FULL_GROUP_BY` permits the same nondeterministic MySQL
  extension queries where MyLite can already execute them
- `SET [SESSION|LOCAL] sql_mode = '...'`, `SET sql_mode = '...'`, and
  `SET sql_mode = DEFAULT`
- the common `REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '')`
  assignment form for removing the mode from the current session value
- `SHOW [SESSION|LOCAL] VARIABLES LIKE 'sql_mode'` exposes the current session
  value; `SHOW GLOBAL VARIABLES` exposes the default global value

Out of scope:

- process-global mutable `sql_mode`
- persisted system variables
- user variables and multi-assignment `SET` statements
- expression reads such as `SELECT @@SESSION.sql_mode`
- broad SQL-mode behavioral effects outside this SELECT validation slice
- exact nondeterministic row-choice guarantees when strict grouping is disabled

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, MySQL Handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4 Reference Manual, `SET` Syntax for Variable Assignment:
  https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- MySQL 8.4 Reference Manual, `SHOW VARIABLES` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-variables.html

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849-regexp` using `mysql --table --force --show-warnings`.

This specification is independently authored from official MySQL
documentation and observed MySQL 8.4.9 behavior. It does not copy MySQL
grammar, documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

The default MySQL 8.4.9 session mode observed in the runtime probe was:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

With the default mode, strict grouping validation rejects nonaggregated,
non-grouped columns in the select list, `HAVING`, and `ORDER BY`. A grouped
ordering probe:

```sql
SELECT grp, COUNT(*) AS c FROM t GROUP BY grp ORDER BY val;
```

failed with error 1055 and text stating that the `ORDER BY` expression is not
in the `GROUP BY` clause and is incompatible with
`sql_mode=only_full_group_by`.

`ONLY_FULL_GROUP_BY` also affects `DISTINCT` plus `ORDER BY`. A probe:

```sql
SELECT DISTINCT grp FROM t ORDER BY val;
```

failed with error 3065 because the `ORDER BY` expression referenced a selected
table column that was not in the select list. These probes succeeded under the
same connection after:

```sql
SET SESSION sql_mode = REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '');
```

The same connection also showed:

- `SET SESSION sql_mode = ''` clears the session mode.
- `SET sql_mode = 'only_full_group_by,strict_trans_tables'` normalizes the
  value to uppercase mode names.
- `SET @@SESSION.sql_mode = 'STRICT_TRANS_TABLES'` updates the session in
  MySQL; MyLite may defer `@@` assignment parsing in this slice.
- `SET @@sql_mode = DEFAULT` restores the default mode in MySQL.
- `SET SESSION sql_mode = 'NO_SUCH_MODE'` fails with error 1231 and does not
  change the current value.
- `SHOW VARIABLES LIKE 'sql_mode'` returns the session value by default.
- `LOCAL` is a synonym for `SESSION` in `SHOW VARIABLES`.

## MyLite Lemon Grammar Snippets

The intended accepted surface is:

```lemon
statement(A) ::= set_system_variable_statement(A).

set_system_variable_statement(A) ::= SET(T) opt_system_variable_scope
        system_variable_name(V) EQ system_variable_value(E).

opt_system_variable_scope ::= .
opt_system_variable_scope ::= SESSION.
opt_system_variable_scope ::= LOCAL.
opt_system_variable_scope ::= GLOBAL.

system_variable_name(A) ::= identifier(A).
system_variable_name(A) ::= SYSTEM_VARIABLE(A).

system_variable_value(A) ::= literal(A).
system_variable_value(A) ::= PLUS numeric_literal(A).
system_variable_value(A) ::= MINUS numeric_literal(A).
system_variable_value(A) ::= DEFAULT(A).
system_variable_value(A) ::= REPLACE LPAREN system_variable_name COMMA STRING COMMA
        STRING RPAREN.
```

The runtime must verify that the target variable name is `sql_mode`,
`@@sql_mode`, or `@@SESSION.sql_mode` before applying the value. `GLOBAL`,
`PERSIST`, and multi-assignment `SET` forms are intentionally deferred.

## Session Mode Semantics

MyLite stores session SQL mode on `mylite_db`.

- New connections initialize the session to the MySQL 8.4 default mode listed
  above.
- `SET sql_mode = DEFAULT` restores that default.
- `SET SESSION sql_mode = ''` stores an empty string and disables
  `ONLY_FULL_GROUP_BY`.
- `SET SESSION sql_mode =
  REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '')` evaluates the current
  session mode when the statement executes and then canonicalizes the result.
- Mode names are matched case-insensitively and serialized in canonical
  uppercase order.
- Unknown mode names fail with MySQL error 1231 and leave the previous session
  value unchanged.
- Combination modes such as `ANSI` and `TRADITIONAL` may expand to the mode
  tokens MyLite recognizes, but this slice only depends on whether
  `ONLY_FULL_GROUP_BY` is present.
- Global mode display remains the built-in default because MyLite has no
  mutable process-global server state.

## SELECT Validation Semantics

When `ONLY_FULL_GROUP_BY` is enabled:

- existing grouping validation runs for grouped queries and implicit aggregate
  queries
- primary-key and non-null unique-key functional dependence allows selecting
  or ordering by other columns from the same grouped table
- existing hidden-column validation runs for `SELECT DISTINCT ... ORDER BY`
- diagnostics continue to use MySQL-compatible codes 1055, 1140, and 3065
  where the current validators already do so

When `ONLY_FULL_GROUP_BY` is absent:

- grouping validation must not reject nonaggregated, non-grouped select-list,
  `HAVING`, or `ORDER BY` references solely because they are not functionally
  dependent on the grouping keys
- `SELECT DISTINCT ... ORDER BY` must not reject an expression solely because
  it references a selected table column that is absent from the select list
- ordinary name-resolution errors still apply; clearing the mode does not make
  unknown columns or ambiguous names valid
- result ordering and representative row choice remain nondeterministic in the
  same class of cases MySQL permits

## Tests

Required coverage:

- parser acceptance for `SET sql_mode = ''`
- parser acceptance for `SET SESSION sql_mode = 'ONLY_FULL_GROUP_BY'`
- parser acceptance for `SET LOCAL sql_mode = DEFAULT`
- parser and runtime acceptance for
  `SET SESSION sql_mode =
  REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '')`
- `SHOW VARIABLES LIKE 'sql_mode'` exposes the default session value
- `SHOW SESSION VARIABLES LIKE 'sql_mode'` observes session changes
- `SHOW GLOBAL VARIABLES LIKE 'sql_mode'` remains the default value
- invalid mode name returns error 1231 and leaves the prior value unchanged
- default mode rejects grouped `ORDER BY` hidden columns with error 1055
- default mode accepts grouped primary-key and non-null unique-key functional
  dependencies
- clearing `ONLY_FULL_GROUP_BY` allows grouped `ORDER BY` hidden columns
- restoring default mode rejects the same grouped query again
- default mode rejects `SELECT DISTINCT ... ORDER BY` hidden columns with error
  3065
- clearing `ONLY_FULL_GROUP_BY` allows `SELECT DISTINCT ... ORDER BY` hidden
  columns where normal name resolution succeeds
