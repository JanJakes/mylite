# Table Lock Placeholders

## Scope

This feature recognizes MySQL 8.4 `LOCK TABLES` and `UNLOCK TABLES` statements
as embedded-compatible parser placeholders.

The first slice accepts the statement shapes emitted by import/export tools and
MySQL-oriented applications:

- `LOCK TABLES t READ`
- `LOCK TABLES t READ LOCAL`
- `LOCK TABLES t WRITE`
- `LOCK TABLES t AS alias WRITE`
- `LOCK TABLES db.t WRITE, u READ LOCAL`
- `UNLOCK TABLES`

The singular `LOCK TABLE` and `UNLOCK TABLE` spellings are also accepted because
they are part of the MySQL 8.4 syntax.

## MySQL Reference Behavior

MySQL 8.4 documents `LOCK TABLES` as a per-session table lock statement with
one or more table names, optional aliases, and `READ [LOCAL]` or `WRITE` lock
types. `UNLOCK TABLES` releases the current session's table locks.

Reference:

- https://dev.mysql.com/doc/refman/8.4/en/lock-tables.html

The full MySQL behavior includes metadata locks, privilege checks, implicit
commit interactions, restrictions while tables are locked, and storage-engine
specific behavior. MyLite does not yet have a server-wide lock manager, grant
tables, or the full implicit-commit model.

## MyLite Behavior

MyLite accepts the supported statement shapes and prepares them as parser
placeholders. Execution:

- returns `MYLITE_DONE`
- exposes zero result columns
- reports zero affected rows
- appends warning `1235` (`ER_NOT_SUPPORTED_YET`)
- validates `LOCK TABLES` targets before the placeholder warning:
  - unqualified names require a selected schema and otherwise fail with error
    `1046` (`ER_NO_DB_ERROR`)
  - explicit unknown schemas fail with error `1049` (`ER_BAD_DB_ERROR`)
  - system schemas fail with error `1044` (`ER_DBACCESS_DENIED_ERROR`)
  - missing tables fail with error `1146` (`ER_NO_SUCH_TABLE`)
- does not acquire or release any runtime lock
- does not change transaction state

This is intentionally conservative. Rejecting these statements breaks common
MySQL imports, while silently pretending to enforce table locks would be
misleading until MyLite implements table-lock state and implicit-commit
semantics.

## Syntax

The intended MyLite grammar is:

```lemon
lock_tables_statement ::= LOCK TABLE lock_table_name_list.
lock_tables_statement ::= LOCK TABLES lock_table_name_list.
unlock_tables_statement ::= UNLOCK TABLE.
unlock_tables_statement ::= UNLOCK TABLES.

lock_table_name_list ::= lock_table_name.
lock_table_name_list ::= lock_table_name_list COMMA lock_table_name.

lock_table_name ::= table_name opt_table_alias lock_type.

lock_type ::= READ opt_local.
lock_type ::= WRITE.

opt_local ::= .
opt_local ::= LOCAL.
```

The table list is retained only for source-span and future analysis use; lock
types and aliases are currently accepted and ignored after target validation.
The AST uses dedicated placeholder kinds:

- `MYLITE_SQL_AST_PLACEHOLDER_LOCK_TABLES`
- `MYLITE_SQL_AST_PLACEHOLDER_UNLOCK_TABLES`

## Deferred

- actual table-lock acquisition and release
- privilege checks
- implicit commit behavior
- restrictions on statements while locks are active
- metadata lock visibility
- exact warning-free MySQL success behavior

## Tests

Parser tests cover representative table lists, aliases, lock types, and syntax
errors for missing table names or missing `TABLES`.

Runtime tests cover placeholder execution diagnostics for `LOCK TABLES` and
`UNLOCK TABLES`, including MySQL-compatible `LOCK TABLES` errors for no selected
schema, unknown explicit schemas, system schemas, and missing tables.
