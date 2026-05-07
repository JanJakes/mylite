# ANSI_QUOTES SQL mode

## Scope

This feature aligns MyLite's first SQL-mode-sensitive lexical behavior with
MySQL 8.4.9 for ordinary statement preparation and SQL-level prepared
statements.

In scope:

- `SET SESSION sql_mode = 'ANSI_QUOTES'` affecting later statement parsing
- double-quoted text as a string literal when `ANSI_QUOTES` is absent
- double-quoted text as a quoted identifier when `ANSI_QUOTES` is present
- SQL-level `PREPARE` storing the lexical mode active at prepare time
- `EXECUTE` reusing the prepared lexical mode even if `sql_mode` changes later
- interaction with `NO_BACKSLASH_ESCAPES` for prepared statement source text

Out of scope:

- stored program body parsing
- binary protocol prepared statements
- exhaustive parser behavior for every DDL/DML statement form
- other SQL modes that alter grammar or expression semantics

## Sources

- MySQL 8.4 Reference Manual, String Literals:
  https://dev.mysql.com/doc/refman/8.4/en/string-literals.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior

Observed behavior:

```sql
SET SESSION sql_mode = '';
SELECT "abc" AS v;
-- v = abc

SET SESSION sql_mode = 'ANSI_QUOTES';
SELECT "c" FROM t;
-- reads column c
```

SQL-level prepared statements keep the lexical mode used by `PREPARE`:

```sql
SET SESSION sql_mode = '';
PREPARE s_default FROM 'SELECT "abc" AS v';
SET SESSION sql_mode = 'ANSI_QUOTES';
EXECUTE s_default;
-- v = abc

SET SESSION sql_mode = 'ANSI_QUOTES';
PREPARE s_ansi FROM 'SELECT "c" FROM t';
SET SESSION sql_mode = '';
EXECUTE s_ansi;
-- reads column c
```

The same rule applies to `NO_BACKSLASH_ESCAPES` lexical string handling in
prepared source text:

```sql
SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES';
PREPARE s_nbe FROM 'SELECT HEX(''a\0b'') AS h, LENGTH(''a\0b'') AS l';
SET SESSION sql_mode = '';
EXECUTE s_nbe;
-- h = 615C3062, l = 4
```

## MyLite Design

Ordinary `mylite_prepare()` derives parser lexer flags from the current
connection SQL mode. `ANSI_QUOTES` sets the lexer flag that makes double
quotes produce quoted-identifier tokens. Without that flag, double quotes
produce string-literal tokens. `NO_BACKSLASH_ESCAPES` keeps backslash ordinary
inside quoted strings and marks string literal AST nodes for runtime decoding.

SQL-level prepared statements store the active lexical mode flags in the
prepared statement registry entry. The stored flags are used for:

- marker scanning while validating the prepared SQL source
- parsing the marker-substituted SQL text at `EXECUTE` time
- marker scanning during `EXECUTE ... USING` substitution

This keeps prepared statement lexical interpretation stable across later
`sql_mode` changes, matching MySQL's prepare-time parsing behavior.

## Tests

Runtime tests compare MyLite against MySQL 8.4.9-observed behavior for:

- an ordinary `ANSI_QUOTES` statement using a double-quoted column identifier
- a default-mode prepared statement containing a double-quoted string, executed
  after switching to `ANSI_QUOTES`
- an `ANSI_QUOTES` prepared statement containing a double-quoted identifier,
  executed after switching back to default mode
- a `NO_BACKSLASH_ESCAPES` prepared statement containing `\0` in string
  literals, executed after switching back to default mode
